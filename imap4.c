#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "auth_functions.h"
#include "imap_commands.h"
#include "strings.h"
#include "imap4.h"

// This is a vaguely sensible max
// more practically, this/2 would be okay.
#define MAX_IMAP4_ARG_COUNT RFC_MAX_INPUT_LENGTH

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <pwd.h>
char drop_privs_to_user(char const *username) {
	if(!getuid()) {
		struct passwd *pswd=getpwnam(username);
		if(!pswd) return 0;
		if(!pswd->pw_uid) return 0; // Don't SU to root
		if(!pswd->pw_gid) return 0; // ...or root's buddies
		if(setgid(pswd->pw_gid)!=0) return 0;
		if(setuid(pswd->pw_uid)!=0) return 0;
		return 1;
	} else {
		// If we're already unprivileged, return success.
		return 1;
	}
}

#include <syslog.h>
void drop_privs() {
	if(!drop_privs_to_user(SAFE_USER)) {
		syslog(LOG_ERR, "Failed to drop privs, aborting");
		exit(1);
	}
}


#include <stdarg.h>
int _imap4_vprintf(FILE *out, char const *format, va_list ap) {
	int rv;
	char *buff=malloc(RFC_MAX_OUTPUT_LENGTH+1);
	rv=vsnprintf(buff, RFC_MAX_OUTPUT_LENGTH+1, format, ap);
	if(strlen(buff)<2) {
		strcat(buff, "\r\n");
	} else if(strcmp(buff+strlen(buff)-2, "\r\n")!=0) {
		strcpy(buff+strlen(buff)-2, "\r\n");
	}
	fwrite(buff, strlen(buff), 1, out);
	return rv;
}
int _imap4_fprintf(FILE *out, char const *format, ...) {
	va_list ap;
	int rv;
	va_start(ap, format);
	rv = _imap4_vprintf(out, format, ap);
	va_end(ap);
	return rv;
}

int _send_misc(FILE *ofp, char *tag, char *prefix, char *message) {
	if(!tag) tag=IMAP4_DEFAULT_TAG;
	if(message) {
		return _imap4_fprintf(ofp, "%s %s %s\r\n", tag, prefix, message);
	} else {
		return _imap4_fprintf(ofp, "%s %s\r\n", tag, prefix);
	}
}

int _send_printf(FILE *ofp, char *tag, char *format, ...) {
	va_list ap;
	int rv;
	if(!tag) tag=IMAP4_DEFAULT_TAG;
	fwrite(tag, sizeof(char), strlen(tag), ofp);
	fwrite(" ", sizeof(char), 1, ofp);
	va_start(ap, format);
	rv = _imap4_vprintf(ofp, format, ap);
	va_end(ap);
	return rv;
}

int grep_equal(char *input, char *compare[]) {
	int i;
	for(i=0; compare[i]; i++) {
		if(!strcasecmp(input, compare[i])) {
			return 1;
		}
	}
	return 0;
}

FILE *sigalarm_out;

void handle_sigalarm(int signum) {
	_send_ERR(sigalarm_out, E_TIMEOUT, NULL);
	exit(0);
}

enum imap4_state command_loop(FILE *ifp, FILE *ofp, enum imap4_state current_state) {
	char command_line[RFC_MAX_INPUT_LENGTH];
	static char previous_command_line[RFC_MAX_INPUT_LENGTH]="";
	char *argv[MAX_IMAP4_ARG_COUNT];
	unsigned int argc=0;
	char *cursor;
	int i;
	struct imap4_command_rv imap4_command_rv;
	static struct imap4_command_rv previous_imap4_command_rv={IMAP4_OK,0,NULL};

	{
		sigalarm_out=ofp;
		sig_t old_sigalarm_handler=signal(SIGALRM, handle_sigalarm);
		alarm(DEFAULT_TIMEOUT);

		if(!fgets(command_line, RFC_MAX_INPUT_LENGTH, ifp)) return st_Dead;

		alarm(0);
		signal(SIGALRM, old_sigalarm_handler);
	}

	cursor=command_line;
	super_chomp(command_line);

	while(cursor) {
		argv[argc++]=cursor;
		strsep(&cursor," ");
		if(strlen(argv[argc-1]) > RFC_MAX_ARG_LENGTH || strlen(argv[argc-1]) < RFC_MIN_ARG_LENGTH ) {
			_send_ERR(ofp, E_PROTOCOL_ERROR, NULL);
			return current_state;
		}
	}
	if(argc<=1) {
		_send_misc(ofp, argv[0], IMAP4_BAD, E_PROTOCOL_ERROR);
		return current_state;
	}
	argc--;
	imap4_command_rv=imap4_rv_invalid;
	char bad_command_args=0;
	for(i=0;imap4_commands[i].name;i++) {
		if(! ( imap4_commands[i].valid_states & BIT(current_state) ) ) continue;
		if(!strcasecmp(imap4_commands[i].name, argv[1])) {
			int do_command=0;

			if(argc-1 < imap4_commands[i].min_argc || argc-1 > imap4_commands[i].max_argc) {
				bad_command_args=1;
			} else if(imap4_commands[i].valid_after_failed) {
				if(!previous_command_line[0]) {
					// empty line??
					do_command=1;
				} else if(1/* previous_imap4_command_rv.successful */) {
					do_command=0;
				} else if(grep_equal(previous_command_line, imap4_commands[i].valid_after_failed)) {
					do_command=1;
				} else {
					do_command=0;
				}
			} else if(imap4_commands[i].valid_after_successful) {
				if(!1/* previous_imap4_command_rv.successful */) {
					do_command=0;
				} else if(grep_equal(previous_command_line, imap4_commands[i].valid_after_successful)) {
					do_command=1;
				} else {
					do_command=0;
				}
			} else {
				do_command=1;
			}
			if(do_command) {
				imap4_command_rv=(*(imap4_commands[i].function))(argv[0], argc, argv+1, &current_state, ifp, ofp);
			} else if(bad_command_args) {
				imap4_command_rv=imap4_rv_badargs;
			} else {
				imap4_command_rv=imap4_rv_invalid;
			}
			break;
		}
	}
	strncpy(previous_command_line, command_line, sizeof(previous_command_line));
	previous_imap4_command_rv=imap4_command_rv;

	if(!imap4_command_rv.response_already_sent) {
		if(imap4_command_rv.response_type) {
			_send_misc(ofp, argv[0], imap4_command_rv.response_type, imap4_command_rv.extra_string);
		} else {
			_send_ERR(ofp, imap4_command_rv.extra_string, imap4_command_rv.extended_error_code);
		}
	}
	return current_state;
}

int handle_connection(FILE *ifp, FILE *ofp) {
	enum imap4_state current_state=st_PreAuth;
	_imap4_fprintf(ofp, IMAP4_DEFAULT_TAG " " IMAP4_OK " %s\r\n", S_SERVER_ID);
	while(1) {
		current_state=command_loop(ifp, ofp, current_state);
		if(current_state == st_Dead) {
			break;
		}
		if(current_state == st_Logout) {
			_storage_synch();
			break;
		}
	}
	return 1;
}
