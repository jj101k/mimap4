#include <string.h>
#include <ctype.h>
#include "imap_commands.h"
#include "strings.h"
#include "imap4.h"
#include "auth_functions.h"


#define ABSOLUTE_MAX_USERNAME_LENGTH 1024


#if USE_OPENSSL
#include <openssl/evp.h>
#endif

/* Compound return codes */
/* Values are: is_success, dont_send_OK, message, prefix code */
struct imap4_command_rv
	imap4_rv_misc_success={IMAP4_OK, 0, NULL, NULL}, 							imap4_rv_misc_failure={IMAP4_BAD, 0, NULL, NULL},
	imap4_rv_quiet_success={IMAP4_OK, 1, NULL, NULL}, 						imap4_rv_quiet_failure={IMAP4_BAD, 1, NULL, NULL},
	imap4_rv_invalid={IMAP4_BAD, 0, E_INVALID_COMMAND, NULL}, 			imap4_rv_badargs={IMAP4_BAD, 0, E_BAD_ARGUMENTS},

	imap4_rv_login_success={IMAP4_OK, 0, S_LOGIN_SUCCESS, NULL},

	imap4_rv_bad_message={"FIXME", 0, E_BADMESSAGE, NULL}, 			imap4_rv_edelay={"FIXME", 0, E_DELAY, P3EXT_LOGIN_DELAY},
	imap4_rv_ebadlogin={"NO", 0, E_BADLOGIN, NULL},
	imap4_rv_internal_error={"FIXME", 0, E_INTERNAL_ERROR}, 			imap4_rv_not_implemented={"FIXME", 0, E_NOT_IMPLEMENTED};

/* Name		Symbol			Args(min,max)		States														Valid after failure			Valid after success */
struct popcommand imap4_commands[]={
	{"LOGIN",imap4_LOGIN, 		2,2, BIT(st_PreAuth), 									NULL, 								NULL					},
	{"AUTHENTICATE",imap4_AUTHENTICATE, 		1,1, BIT(st_PreAuth), 									NULL, 								NULL					},

	{"NOOP",imap4_NOOP, 			0,0, BIT(st_PostAuth)|BIT(st_PreAuth)|BIT(st_Selected), 											NULL, 									NULL					},
	{"SELECT",imap4_SELECT, 			1,1, BIT(st_PostAuth)|BIT(st_Selected), 											NULL, 									NULL					},
	{"EXAMINE",imap4_SELECT, 			1,1, BIT(st_PostAuth)|BIT(st_Selected), 											NULL, 									NULL					},
	{"CAPABILITY",imap4_CAPABILITY, 			0,0, BIT(st_PostAuth)|BIT(st_PreAuth)|BIT(st_Selected), 											NULL, 									NULL					},

	{"LOGOUT",imap4_LOGOUT, 			0,0, BIT(st_PostAuth)|BIT(st_PreAuth)|BIT(st_Selected), NULL, 									NULL					},
	{NULL,	NULL,						0,0}
};

/*
 * struct imap4_message * valid_message(unsigned long int index)
 *
 * Tells you if a message number points to an existent, non-deleted message.
 */
struct imap4_message * valid_message(unsigned long int index) {
	struct imap4_message *message;
	if(index<1 || index>_storage_message_count()) {
		return NULL;
	}
	message=_storage_message_number(index);
	return message;
}

#define adhoc_command_rv(response_type, response_already_sent, extra_string, extended_error_code) \
	((struct imap4_command_rv) {response_type, response_already_sent, extra_string, extended_error_code})

/*
 * struct imap4_command_rv imap4_LOGIN(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp)
 *
 * Syntax: LOGIN <user name> <password>
 *
 * Logs the user in.
 *
 */
struct imap4_command_rv imap4_LOGIN(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp) {
	size_t username_length = strlen(argv[1]);
	if(username_length>=ABSOLUTE_MAX_USERNAME_LENGTH || username_length<=0) return imap4_rv_ebadlogin;

	char const *mailbox=_auth_attempt_login(argv[1], argv[2]);
	if(mailbox) {
		if(_auth_login_delay_needed(argv[1])) {
			return imap4_rv_edelay; // delayed
		} else {
			_storage_use_mailbox(mailbox);
			if(_storage_need_user()==wuMailbox) {
				if(!drop_privs_to_user(mailbox)) drop_privs();
			} else {
				drop_privs();
			}
			*current_state=st_PostAuth;
			return imap4_rv_login_success;
		}
	} else {
		return imap4_rv_ebadlogin; // invalid
	}
}

/*
 * struct imap4_command_rv imap4_SELECT(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp)
 *
 * Syntax: SELECT <folder>
 *
 * Tries to access a folder in the mailbox. This can fail for a few reasons. Returns a bunch of untagged responses.
 *
 */
struct imap4_command_rv imap4_SELECT(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp) {
	char command_name[50];
	unsigned long int i;
	for(i=0;argv[0][i];i++) {
		command_name[i]=(char)toupper(argv[0][i]);
	}
	command_name[i]=0;
	if(_storage_select_folder(argv[1], (!strcmp(command_name, "SELECT"))?0:1)) {

        unsigned long int message_count=0;
        unsigned long int message_recent_count=0;
        struct imap4_message *current_message;

        if(_storage_array_style()==asArray) {
                message_count=_storage_message_count();
                for(i=0, current_message=_storage_first_message();i<message_count;i++, current_message++) {
                        if(current_message->flags&BIT(imapFlagRecent)) {
                                message_recent_count++;
                        }
                }
        } else {
                for(current_message=_storage_first_message();current_message;current_message=current_message->next) {
                        if(!current_message->flags&BIT(imapFlagRecent)) {
                                message_recent_count++;
                        }
                        message_count++;
                }
        }

		_send_printf(ofp, NULL, "%lu EXISTS", message_count);
		_send_printf(ofp, NULL, "%lu RECENT", message_recent_count);
		char flag_list[1024];
		strlcpy(flag_list, "\\Answered \\Flagged \\Deleted \\Seen \\Draft", sizeof(flag_list));
		char const **other_flags = _storage_available_flags();
		if(other_flags) {
			int i;
			for(i=0;other_flags[i];i++) {
				if(!other_flags[i][0]) continue; /* You deserve to suffer if you do this though */
				strlcat(flag_list, " ", sizeof(" "));
				strlcat(flag_list, other_flags[i], sizeof(flag_list));
			}
		}
		_send_printf(ofp, NULL, "FLAGS (%s)", flag_list);
		_send_printf(ofp, NULL, IMAP4_OK " [UIDVALIDITY %lu]", _storage_uidvalidity());
		return adhoc_command_rv(IMAP4_OK, 0, "SELECT/EXAMINE completed", _storage_is_readonly()?"READ-ONLY":"READ-WRITE");
	} else {
		return adhoc_command_rv(IMAP4_NO, 0, "SELECT/EXAMINE can't open folder", NULL);
	}
}



/*
 * struct imap4_command_rv imap4_AUTHENTICATE(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp)
 *
 * Syntax: AUTHENTICATE <mechanism> ...
 *
 * Starts a challenge-response style authentication with the client.
 *
 */
struct imap4_command_rv imap4_AUTHENTICATE(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp) {
	return adhoc_command_rv(IMAP4_NO, 0, "AUTHENTICATE bad mechanism", NULL);
}


/*
 * struct imap4_command_rv imap4_NOOP(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp)
 *
 * Syntax: NOOP
 *
 * Does nothing. Elicits a successful response unless the world has ended.
 */
struct imap4_command_rv imap4_NOOP(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp) {
	return adhoc_command_rv(IMAP4_OK, 0, S_NOOP, NULL);;
}

/*
 * struct imap4_command_rv imap4_CAPABILITY(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp)
 *
 * Syntax: CAPABILITY
 *
 * Tells the user what this server can do.
 */
char *capabilities="IMAP4rev1";

struct imap4_command_rv imap4_CAPABILITY(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp) {
	_send_misc(ofp, NULL, "CAPABILITY", capabilities);
	return adhoc_command_rv(IMAP4_OK, 0, "CAPABILITY completed", NULL);;
}


/*
 * struct imap4_command_rv imap4_LOGOUT(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp)
 *
 * Syntax: LOGOUT
 *
 * Logs you out, deleting all messages you've marked for deletion.
 */
struct imap4_command_rv imap4_LOGOUT(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp) {
	_send_misc(ofp, NULL, "BYE", S_LOGOUT);
	*current_state=st_Logout;
	return adhoc_command_rv(IMAP4_OK, 0, "LOGOUT completed", NULL);;
}

