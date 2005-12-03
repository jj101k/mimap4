#include <string.h>
#include "imap_commands.h"
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
	imap4_rv_invalid={IMAP4_BAD, 0, E_INVALID_COMMAND, NULL}, 			imap4_rv_badargs={IMAP4_BAD, 0, E_BAD_ARGUMENTS},

	imap4_rv_login_success={IMAP4_OK, 0, S_LOGIN_SUCCESS, NULL},

	imap4_rv_bad_message={"FIXME", 0, E_BADMESSAGE, NULL}, 			imap4_rv_edelay={"FIXME", 0, E_DELAY, P3EXT_LOGIN_DELAY},
	imap4_rv_ebadlogin={"NO", 0, E_BADLOGIN, NULL},
	imap4_rv_internal_error={"FIXME", 0, E_INTERNAL_ERROR}, 			imap4_rv_not_implemented={"FIXME", 0, E_NOT_IMPLEMENTED};

/* Command definitions */
char *login_only[] = {"LOGIN"};

/* Name		Symbol			Args(min,max)		States														Valid after failure			Valid after success */
struct popcommand imap4_commands[]={
	{"LOGIN",imap4_LOGIN, 		2,2, BIT(st_PreAuth), 									login_only, 								NULL					},

	{"NOOP",imap4_NOOP, 			0,0, BIT(st_PostAuth)|BIT(st_PreAuth)|BIT(st_Selected), 											NULL, 									NULL					},

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
	if((!message) || message->deleted) return NULL;
	return message;
}

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
 * struct imap4_command_rv imap4_LOGOUT(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp)
 *
 * Syntax: LOGOUT
 *
 * Logs you out, deleting all messages you've marked for deletion.
 */
struct imap4_command_rv imap4_rv_logout={IMAP4_BYE, 0, S_LOGOUT, NULL};

struct imap4_command_rv imap4_LOGOUT(const char const *tag, int argc, char *argv[], enum imap4_state *current_state, FILE *ifp, FILE *ofp) {
	*current_state=st_Logout;
	return imap4_rv_logout;
}

