#include <stdlib.h>
#include <stdio.h>
#include "imap4.h"

#include "storage_functions.h"

struct imap4_command_rv {
	char *response_type;
	char response_already_sent;
	char *extra_string;
	char *extended_error_code;
};

extern struct imap4_command_rv imap4_rv_misc_success, imap4_rv_misc_failure, imap4_rv_quiet_success, imap4_rv_quiet_failure, imap4_rv_invalid, imap4_rv_badargs;

struct popcommand {
	char *name;
	struct imap4_command_rv (*function)(const char const *, int, char *[], enum imap4_state *, FILE *, FILE *);
	unsigned int min_argc;
	unsigned int max_argc;
	unsigned int valid_states;
	char **valid_after_failed;
	char **valid_after_successful;
};

// trans: stat list retr dele noop rset quit
// opt/trans: top uidl 
// opt/auth: user* pass* apop*

#define DEFINE_IMAP4(cmd) struct imap4_command_rv cmd(const char const *, int, char *[], enum imap4_state *, FILE *, FILE *)

DEFINE_IMAP4(imap4_CAPA);

DEFINE_IMAP4(imap4_USERPASS);
DEFINE_IMAP4(imap4_APOP);
DEFINE_IMAP4(imap4_NOOP);
DEFINE_IMAP4(imap4_STAT);
DEFINE_IMAP4(imap4_LIST);
DEFINE_IMAP4(imap4_UIDL);
DEFINE_IMAP4(imap4_RETR);
DEFINE_IMAP4(imap4_TOP);
DEFINE_IMAP4(imap4_DELE);
DEFINE_IMAP4(imap4_RSET);
DEFINE_IMAP4(imap4_LOGOUT);

extern struct popcommand imap4_commands[];
