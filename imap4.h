#ifndef __IMAP4_H
#define __IMAP4_H
#include <stdio.h>
enum imap4_state {p3Dead, p3Authorisation, p3Transaction, p3Update, p3EndMarker};

#define BIT(a) (1<<(a))
#define ALL_IMAP4_STATES BIT(p3EndMarker)-1

enum imap4_state command_loop(FILE *ifp, FILE *ofp, enum imap4_state current_state);
int handle_connection(FILE *ifp, FILE *ofp);
int _send_misc(FILE *ofp, char *tag, char *prefix, char *message);

#define IMAP4_OK "OK"
#define IMAP4_BAD "BAD"
#define IMAP4_NO "NO"
#define IMAP4_BYE "BYE"

#define IMAP4_DEFAULT_TAG "*"

#define _send_ERR(ofp, message, extended_error) _send_misc(ofp, NULL, IMAP4_BAD, message)
#define _send_OK(ofp, message) _send_misc(ofp, NULL, IMAP4_OK, message)

#define DEFAULT_PORT htons(110)

// IMAP4 extended error codes follow.
#define P3EXT_LOGIN_DELAY "LOGIN-DELAY"
#define P3EXT_IN_USE "IN-USE"

// rfc 1939
#define RFC_MAX_ARG_LENGTH 40
#define RFC_MAX_OUTPUT_LENGTH 512
#define RFC_MIN_INACTIVITY_TIMER 600
// rfc 2449
#define RFC_MAX_INPUT_LENGTH 255
#define RFC_MIN_ARG_LENGTH 1


#define DEFAULT_TIMEOUT RFC_MIN_INACTIVITY_TIMER+0

/*
 * Utility functions
 * =================
 */

/* 
 * super_chomp(string)
 * super_chomp_n(string, length)
 *
 * s/[\r\n]/\x00/ (Starting from the end and working *backwards*)
 *
 * This returns a character if [\r\n] was found.
 * MS-DOS (\r\n): \n
 * Olde-worlde Mac (\r): \r
 * UNIX (\n): \n
 *
 * The reason for this is so that you have some hope of detecting \r<line length exceeded>\n
 *
 * This is only intended to trim off a trailing \r, \r\n or \n left by (eg) fgets(). If there are such characters in the middle,
 * then you're done for anyway...
 *
 */
char super_chomp(char *);
char super_chomp_n(char *, unsigned long int);


/*
 * _imap4_fprintf(fp, format, ...)
 *
 * Wraps around fprintf, with the exceptions that:
 * - it guarantees no more than 512 characters will be written
 * - it guarantees that it will end with \r\n
 *
 */
int _imap4_fprintf(FILE *out, char const *format, ...);

/*
 * hex_from_binary(out_string, in_string, bytes)
 *
 * Turns 'bytes' bytes of 'in_string' into 'bytes'*2 hex characters in 'out_string', followed by a "\0" character.
 *
 */
int hex_from_binary(char *outstring, unsigned char const *instring, unsigned long bytes);

/*
 * drop_privs_to_user(char const *username)
 *
 * Tries to SU to the given user. Returns 1 on success, 0 on failure.
 *
 * drop_privs()
 *
 * SUs to the default 'safe' user, or exits.
 *
 */
char drop_privs_to_user(char const *);
void drop_privs();
#endif
