#include <string.h>
#include "pop_commands.h"
#include "strings.h"
#include "imap4.h"
#include "auth_functions.h"


#define ABSOLUTE_MAX_USERNAME_LENGTH 1024


#ifdef USE_OPENSSL
#include <openssl/evp.h>
#endif

/* Compound return codes */
/* Values are: is_success, dont_send_OK, message, prefix code */
struct imap4_command_rv
	imap4_rv_misc_success={IMAP4_OK, 0, NULL, NULL}, 							imap4_rv_misc_failure={IMAP4_BAD, 0, NULL, NULL},
	imap4_rv_quiet_success={IMAP4_OK, 1, NULL, NULL}, 						imap4_rv_quiet_failure={IMAP4_BAD, 1, NULL, NULL},
	imap4_rv_invalid={IMAP4_NO, 0, E_INVALID_COMMAND, NULL}, 			imap4_rv_badargs={IMAP4_BAD, 0, E_BAD_ARGUMENTS},

	imap4_rv_login_success={IMAP4_OK, 0, S_LOGIN_SUCCESS, NULL}, 	imap4_rv_user_success={IMAP4_OK, 0, S_USER_SUCCESS, NULL},

	imap4_rv_bad_message={"FIXME", 0, E_BADMESSAGE, NULL}, 			imap4_rv_edelay={"FIXME", 0, E_DELAY, P3EXT_LOGIN_DELAY},
	imap4_rv_elocked={"FIXME", 0, E_LOCKED, P3EXT_IN_USE}, 			imap4_rv_ebadlogin={"FIXME", 0, E_BADLOGIN, NULL},
	imap4_rv_internal_error={"FIXME", 0, E_INTERNAL_ERROR}, 			imap4_rv_not_implemented={"FIXME", 0, E_NOT_IMPLEMENTED};

/* Command definitions */
/* - These two are needed to suppress warnings! - */
char *user_and_pass[]={"USER", "PASS", NULL};
char *user_only[]={"USER", NULL};

/* Name		Symbol			Args(min,max)		States														Valid after failure			Valid after success */
struct popcommand imap4_commands[]={
	{"CAPA",imap4_CAPA, 			0,0, BIT(p3Transaction)|BIT(p3Authorisation), NULL, 									NULL					},
	{"USER",imap4_USERPASS, 	1,1, BIT(p3Authorisation), 										user_and_pass,					NULL					},
	{"PASS",imap4_USERPASS, 	1,1, BIT(p3Authorisation), 										NULL, 									user_only			},
	{"APOP",imap4_APOP, 			2,2, BIT(p3Authorisation), 										user_and_pass,					NULL					},

	{"NOOP",imap4_NOOP, 			0,0, BIT(p3Transaction), 											NULL, 									NULL					},
	{"STAT",imap4_STAT, 			0,0, BIT(p3Transaction), 											NULL, 									NULL					},
	{"LIST",imap4_LIST, 			0,1, BIT(p3Transaction), 											NULL, 									NULL					},
	{"UIDL",imap4_UIDL, 			0,1, BIT(p3Transaction), 											NULL, 									NULL					},
	{"RETR",imap4_RETR, 			1,1, BIT(p3Transaction), 											NULL, 									NULL					},
	{"TOP",imap4_TOP, 				2,2, BIT(p3Transaction), 											NULL, 									NULL					},
	{"DELE",imap4_DELE, 			1,1, BIT(p3Transaction), 											NULL, 									NULL					},
	{"RSET",imap4_RSET, 			0,0, BIT(p3Transaction), 											NULL, 									NULL					},

	{"LOGOUT",imap4_LOGOUT, 			0,0, BIT(p3Transaction)|BIT(p3Authorisation), NULL, 									NULL					},
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
	if((!message) || message->deleted) return NULL;
	return message;
}

/*
 * struct imap4_command_rv imap4_DELE(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp)
 *
 * Syntax: DELE <message number>
 *
 * Marks a message deleted
 */
struct imap4_command_rv imap4_rv_message_deleted={IMAP4_OK, 0, S_MESSAGE_DELETED, NULL};

struct imap4_command_rv imap4_DELE(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp) {
	struct imap4_message *current_message;
	unsigned long int index;
	
	index=strtoull(argv[1], NULL, 10);
	current_message=valid_message(index);
	if(!current_message) {
		return imap4_rv_bad_message;
	}

	current_message->deleted=1;
	return imap4_rv_message_deleted;
}

/*
 * struct imap4_command_rv imap4_RETR(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp)
 *
 * Syntax: RETR <message number>
 *
 * Outputs the message in question, byte-stuffed and LF-converted as appropriate.
 */
struct imap4_command_rv imap4_RETR(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp) {
	struct imap4_message *current_message;
	unsigned long int index;
	
	index=strtoull(argv[1], NULL, 10);
	current_message=valid_message(index);
	if(!current_message) {
		return imap4_rv_bad_message;
	}

	_send_OK(ofp, S_MESSAGE_FOLLOWS);
	_storage_dump_message(current_message, ofp);
	_imap4_fprintf(ofp, ".\r\n");
	
	return imap4_rv_quiet_success;
}

/*
 * struct imap4_command_rv imap4_TOP(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp)
 *
 * Syntax: TOP <message number> <line count>
 *
 * As RETR, but only outputs the body plus the first <line count> lines of the body.
 */
struct imap4_command_rv imap4_TOP(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp) {
	struct imap4_message *current_message;
	unsigned long int index;
	unsigned long int max_lines;
	
	index=strtoull(argv[1], NULL, 10);
	current_message=valid_message(index);
	if(!current_message) {
		return imap4_rv_bad_message;
	}

	max_lines=strtoull(argv[2], NULL, 10);

	_send_OK(ofp, S_MESSAGE_FOLLOWS);
	_storage_dump_headers(current_message, ofp);
	_storage_dump_message_lines(current_message, max_lines, ofp);
	_imap4_fprintf(ofp, ".\r\n");
	
	return imap4_rv_quiet_success;
}

/*
 * struct imap4_command_rv imap4_CAPA(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp)
 *
 * Syntax: CAPA
 *
 * Lists the capabilities of the server. Can theoretically output different values in different states.
 */
struct imap4_command_rv imap4_CAPA(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp) {
	_send_OK(ofp, S_CAPA_RESPONSE);
	if(*current_state==p3Transaction) {
	}
	_imap4_fprintf(ofp, "TOP\r\n");
	_imap4_fprintf(ofp, "USER\r\n");
	_imap4_fprintf(ofp, "RESP-CODES\r\n");
	_imap4_fprintf(ofp, "PIPELINING\r\n");
	if(_storage_uidl_supported()) _imap4_fprintf(ofp, "UIDL\r\n");
	int login_delay=_auth_default_login_delay();
	if(login_delay) _imap4_fprintf(ofp, "LOGIN-DELAY %i\r\n", login_delay);
	_imap4_fprintf(ofp, "IMPLEMENTATION %s\r\n", S_SERVER_IMPLEMENTATION);
	//_imap4_fprintf(ofp, "SASL\r\n"); FIXME
	//_imap4_fprintf(ofp, "EXPIRE NEVER\r\n"); FIXME
	_imap4_fprintf(ofp, ".\r\n");
	return imap4_rv_quiet_success;
}

/*
 * struct imap4_command_rv imap4_USERPASS(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp)
 *
 * Syntax: USER <user name>
 *				 PASS <password>
 *
 * Logs the user in. The interaction between USER and PASS is actually quite complicated, but a successful USER
 * must always precede PASS.
 *
 * Uses a static var to remember the username between calls.
 */
struct imap4_command_rv imap4_USERPASS(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp) {
	static char username[ABSOLUTE_MAX_USERNAME_LENGTH];
	if(!strcasecmp(argv[0],"PASS")) {
		if(!username[0]) {
			return imap4_rv_ebadlogin;
		}
		char const *mailbox=_auth_attempt_login(username, argv[1]);
		if(mailbox) {
			if(_auth_login_delay_needed(username)) {
				return imap4_rv_edelay; // delayed
			} else if(_storage_lock_mailbox(mailbox)) {
				if(_storage_need_user()==wuMailbox) {
					if(!drop_privs_to_user(mailbox)) drop_privs();
				} else {
					drop_privs();
				}
				*current_state=p3Transaction;
				return imap4_rv_login_success;
			} else {
				return imap4_rv_elocked; // locked
			}
		} else {
			return imap4_rv_ebadlogin; // invalid
		}
	} else {
		if(strlen(argv[1])>=ABSOLUTE_MAX_USERNAME_LENGTH) return imap4_rv_ebadlogin;
		strncpy(username, argv[1], strlen(argv[1])+1);
		return imap4_rv_user_success;
	}
}

/*
 * struct imap4_command_rv imap4_APOP(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp)
 *
 * Syntax: APOP <user name> <digest>
 *
 * Logs the user in. Read the spec if you want to know how this works, but in terms of control flow it's
 * identical to a USER/PASS pair.
 *
 * Only works if OpenSSL is enabled.
 */
struct imap4_command_rv imap4_APOP(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp) {
#ifdef USE_OPENSSL
	char const *timestamp=_auth_timestamp();
	if(!timestamp) return imap4_rv_not_implemented;
	char const *username=argv[1];
	char const *password=_auth_password(username);
	EVP_MD_CTX *ssl_context;
	unsigned char digest_raw[EVP_MAX_MD_SIZE];
	char digest_hex[(2*EVP_MAX_MD_SIZE)+1];

	// Always do the MD5 thing to protect against timing attacks.
	char const *password_for_digest;
	if(!password) {
		password_for_digest="xxxxxxxx";
		return imap4_rv_ebadlogin;
	} else {
		password_for_digest=password;
	}

	ssl_context=EVP_MD_CTX_create();
	if(!EVP_DigestInit(ssl_context, EVP_md5())) return imap4_rv_internal_error;
	if(!EVP_DigestUpdate(ssl_context, timestamp, strlen(timestamp))) {EVP_MD_CTX_cleanup(ssl_context); return imap4_rv_internal_error;}
	if(!EVP_DigestUpdate(ssl_context, password_for_digest, strlen(password))) {EVP_MD_CTX_cleanup(ssl_context); return imap4_rv_internal_error;}
	if(!EVP_DigestFinal(ssl_context, digest_raw, NULL)) {EVP_MD_CTX_cleanup(ssl_context); return imap4_rv_internal_error;}
	hex_from_binary(digest_hex, digest_raw, EVP_MD_size(EVP_md5()));

	if(!password) return imap4_rv_ebadlogin;

	if(!strcasecmp(argv[2],digest_hex)) {
		char const *mailbox=_auth_login(username);
		if(!mailbox) {
			return imap4_rv_internal_error;
		} else if(_auth_login_delay_needed(username)) {
			return imap4_rv_edelay; // delayed
		} else if(_storage_lock_mailbox(mailbox)) {
			if(_storage_need_user()==wuMailbox) {
				if(!drop_privs_to_user(mailbox)) drop_privs();
			} else {
				drop_privs();
			}
			*current_state=p3Transaction;
			return imap4_rv_login_success;
		} else {
			return imap4_rv_elocked; // locked
		}
	} else {
		return imap4_rv_ebadlogin;
	}
#else
	return imap4_rv_not_implemented;
#endif	
}

/*
 * struct imap4_command_rv imap4_NOOP(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp)
 *
 * Syntax: NOOP
 *
 * Does nothing. Elicits a successful response unless the world has ended.
 */
struct imap4_command_rv imap4_rv_noop={IMAP4_OK, 0, S_NOOP, NULL};

struct imap4_command_rv imap4_NOOP(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp) {
	return imap4_rv_noop;
}
/*
 * struct imap4_command_rv imap4_STAT(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp)
 *
 * Syntax: STAT
 *
 * Outputs the mailbox's STATistics: number of messages and total size (in bytes, of the expected output).
 * Theoretically can be used to determine if a mailbox has changed. In practice, UIDL works better for this.
 */
struct imap4_command_rv imap4_STAT(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp) {
	unsigned long int message_count=0;
	unsigned long int message_sum=0;
	struct imap4_message *current_message;
	unsigned long int i;

	if(_storage_array_style()==asArray) {
		message_count=_storage_message_count();
		for(i=0, current_message=_storage_first_message();i<message_count;i++, current_message++) {
			if(!current_message->deleted) {
				message_sum+=current_message->size;
			}
		}
	} else {
		for(current_message=_storage_first_message();current_message;current_message=current_message->next) {
			if(!current_message->deleted) {
				message_count++;
				message_sum+=current_message->size;
			}
		}
	}

	_imap4_fprintf(ofp, IMAP4_OK " %lu %lu\r\n", message_count, message_sum);
	return imap4_rv_quiet_success;;
}

/*
 * struct imap4_command_rv imap4_LIST(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp)
 *
 * Syntax: LIST [<message number>]
 *
 * Outputs the message number and size of either the specified message or all messages (in order).
 */
struct imap4_command_rv imap4_LIST(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp) {
	struct imap4_message *current_message=_storage_first_message();
	long unsigned int i;
	
	if(argc==1) {
		_send_OK(ofp, S_LIST_FOLLOWS);
		if(_storage_array_style()==asArray) {
			unsigned long int message_count=_storage_message_count();
			for(i=1;i<=message_count;i++, current_message++) {
				if(!current_message->deleted) _imap4_fprintf(ofp, "%lu %lu\r\n", i, current_message->size);
			}
		} else {
			for(i=1;current_message;i++, current_message=current_message->next) {
				if(!current_message->deleted) _imap4_fprintf(ofp, "%lu %lu\r\n", i, current_message->size);
			}
		}
		_imap4_fprintf(ofp, ".\r\n");

		return imap4_rv_quiet_success;
	} else {
		i=strtoull(argv[1], NULL, 10);
		current_message=valid_message(i);
		if(!current_message) {
			return imap4_rv_bad_message;
		} else {
			_imap4_fprintf(ofp, IMAP4_OK " %lu %lu\r\n", i, current_message->size);
			return imap4_rv_quiet_success;
		}
	}
}

/*
 * struct imap4_command_rv imap4_RSET(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp)
 *
 * Syntax: RSET
 *
 * Resets all state flags (ie: un-marks all deleted messages) for the mailbox.
 */
struct imap4_command_rv imap4_rv_reset={IMAP4_OK, 0, S_RESET, NULL};

struct imap4_command_rv imap4_RSET(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp) {
	struct imap4_message *current_message=_storage_first_message();
	unsigned long int i;
	
	if(_storage_array_style()==asArray) {
		unsigned long int message_count=_storage_message_count();
		for(i=0;i<message_count;current_message++, i++) {
			current_message->deleted=0;
		}
	} else {
		for(;current_message;current_message=current_message->next) {
			current_message->deleted=0;
		}
	}

	return imap4_rv_reset;
}
/*
 * struct imap4_command_rv imap4_UIDL(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp)
 *
 * Syntax: UIDL [<message number>]
 *
 * Like LIST, only with a magic string instead of the message size. This magic string is supposed to be unique (to the message)
 * for all time, within the scope of this particular mailbox. It's thus used as a have-I-already-seen-this-message marker.
 */
struct imap4_command_rv imap4_UIDL(const char const *tag, int argc, char *argv[], enum imap4_state *unused2, FILE *ifp, FILE *ofp) {
	struct imap4_message *current_message=_storage_first_message();
	long unsigned int i;

	if(!_storage_uidl_supported()) {
		return imap4_rv_not_implemented;
	}
	
	if(argc==1) {
		_send_OK(ofp, S_LIST_FOLLOWS);
		if(_storage_array_style()==asArray) {
			unsigned long int message_count=_storage_message_count();
			for(i=1;i<=message_count;i++, current_message++) {
				if(!current_message->deleted) _imap4_fprintf(ofp, "%lu %s\r\n", i, current_message->uidl);
			}
		} else {
			for(i=1;current_message;i++, current_message=current_message->next) {
				if(!current_message->deleted) _imap4_fprintf(ofp, "%lu %s\r\n", i, current_message->uidl);
			}
		}
		_imap4_fprintf(ofp, ".\r\n");

		return imap4_rv_quiet_success;
	} else {
		i=strtoull(argv[1], NULL, 10);
		current_message=valid_message(i);
		if(!current_message) {
			return imap4_rv_bad_message;
		} else {
			_imap4_fprintf(ofp, IMAP4_OK " %lu %s\r\n", i, current_message->uidl);
			return imap4_rv_quiet_success;
		}
	}

	return imap4_rv_quiet_success;
}

/*
 * struct imap4_command_rv imap4_LOGOUT(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp)
 *
 * Syntax: LOGOUT
 *
 * Logs you out, deleting all messages you've marked for deletion.
 */
struct imap4_command_rv imap4_rv_logout={IMAP4_BYE, 0, S_LOGOUT, NULL};

struct imap4_command_rv imap4_LOGOUT(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp) {
	*current_state=p3Update;
	return imap4_rv_logout;
}

