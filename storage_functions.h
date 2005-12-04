#ifndef __storage_functions_h
#define __storage_functions_h

#include <stdlib.h>
#include <stdint.h>
#include "storage.h"

/*
 * uint32_t (*_storage_uidvalidity)()
 *
 * The current folder's UIDVALIDITY value. This is a magic number which purely
 * serves to tell that this is the same folder. If it gets deleted and recreated,
 * or if there's ANY DOUBT the number should always be higher than it's 
 * previously been.
 *
 * On UNIXy systems, a timestamp is usually okay for this, but each folder must
 * have a distinct value for this, so that IMAP clients can always look up
 * cached data for message (uidvalidty . uid).
 *
 * This number MUST NOT be zero.
 */

extern uint32_t (*_storage_uidvalidity)();

/*
 * char const ** (*_storage_available_flags)()
 *
 * The available flags for the current folder. A NULL-terminated array of strings.
 *
 * You don't have to account for the flags: \Deleted, \Seen, \Answered,
 * \Flagged, \Draft, \Recent. Everything else you do.
 *
 * Note that \Recent can't be set by the client anyway.
 *
 * Normal response: (char const **){NULL}
 */

extern char const ** (*_storage_available_flags)(); 

/*
 * int _storage_use_mailbox(char const *)
 *
 * Notifies the storage module we'll be using. The return value is NOT currently
 * checked, but should be 1 for success, 0 for failure.
 *
 * See also _storage_select_folder()
 */
extern int (*_storage_use_mailbox)(char const *mailbox);

/*
 * int _storage_lock_folder(char const *)
 *
 * Lock the folder in preparation for writing. Return 1 if successful, 0 (no such folder) otherwise.
 *
 * Notes: 
 * - this function is allowed to sleep as long as it needs to to get
 * the lock.
 * - this might not be the currently selected folder (we also lock for COPY)
 */
extern int (*_storage_lock_folder)(char const *folder);

/*
 * int _storage_select_folder(char const *, char)
 *
 * Prepare to read from a folder. 'readonly' is for information only - the
 * actual enforcement is done in the main program.
 *
 * Return 1 on success (ie, that _storage_folder_metadata() will work), or 0
 * to indicate that the folder cannot be opened (in this mode, at least).
 *
 * Note that this is where you should let the program know that the mailbox
 * itself is inaccessible.
 *
 * IMPORTANT: See also _storage_is_readonly() regarding read-only support
 */
extern int (*_storage_select_folder)(char const *folder, char readonly);


/*
 * char _storage_is_readonly()
 *
 * Say whether the currently SELECTed or EXAMINEd folder is actually read-only.
 * Note that THERE IS A DIFFERENCE between requested-RW-got-RO and requested-RO.
 *
 * The IMAP4rev1 spec says that requested-RW-got-RO means that "per-user changes
 * to the permanent state" may be possible, which effectively seems to mean that
 * you may be able to twiddle (some of) the knobs on the messages, but you can't
 * add (COPY) any. The example given seems to suggest "deleting" messages from
 * your view of an NNTP newsgroup might be an application of this.
 */
extern char (*_storage_is_readonly)();

/*
 * enum arrayStyle _storage_array_style()
 *
 * Say how you have the 'struct imap4message's stored. If they're in a proper array (eg, if you could
 * cheaply count up how many were needed in advance), return asArray. Otherwise (the default) return
 * asLinkedList, and the 'next' pointers in each struct will be used instead.
 */
extern enum arrayStyle (*_storage_array_style)();

/*
 * unsigned long int _storage_message_count()
 *
 * Say how many messages there are (both deleted any undeleted). This gets called a lot, so you may
 * want to cache this value.
 */
extern unsigned long int (*_storage_message_count)();

/*
 * unsigned long int _storage_message_sum()
 *
 * (unused)
 */
extern unsigned long int (*_storage_message_sum)();

/*
 * struct imap4_message *_storage_first_message()
 *
 * Point to the first (real, whether deleted or not) message, ie. the start of the message array (or linked list)
 */
extern struct imap4_message *(*_storage_first_message)();

/*
 * struct imap4_message *_storage_message_number(unsigned long int)
 *
 * Point to a specific message, __where '1' means the first message__. The default implementation of this should be fine,
 * as it uses _storage_array_style() and _storage_first_message() to do the right thing.
 */
extern struct imap4_message *(*_storage_message_number)(unsigned long int);

/*
 * int _storage_uidl_supported()
 *
 * Return 1 if you have/will assign UIDL values to all messages. This only affects the UIDL IMAP4 command.
 */
extern int (*_storage_uidl_supported)();

/*
 * int _storage_dump_fragment(unsigned long int from_offset, unsigned long int to_offset, 
 *  unsigned long int skip_lines, unsigned long int max_lines, FILE * out)
 *
 * Dump some or all of a message to the given file pointer (ie, to the client). You need to byte-stuff leading full stops (.)
 * yourself, and you should guarantee that each line ends with \r\n.
 *
 * The offsets can be bytes, lines, or whatever. That's basically up to you, you just need to make sure that the
 * offsets in your 'struct imap4message's are of the same type. 'skip_lines' is there to help you skip any in-band
 * metadata (like the 'From ' line in mbox files); 'max_lines' is there to help with TOP.
 *
 * See below also.
 */
extern int (*_storage_dump_fragment)(unsigned long int, unsigned long int, unsigned long int, unsigned long int, FILE *);

/*
 * int _storage_dump_message(struct imap4_message *, FILE *)
 *
 * Dump an entire message. The default uses _storage_dump_fragment() and can be left alone unless you have
 * a specific optimisation to make.
 */
extern int (*_storage_dump_message)(struct imap4_message *, FILE *);

/*
 * int _storage_dump_headers(struct imap4_message *, FILE *)
 *
 * Dump just the message headers, as used in TOP. The default uses _storage_dump_fragment()
 */
extern int (*_storage_dump_headers)(struct imap4_message *, FILE *);

/* 
 * int _storage_dump_message_lines(struct imap4_message *, unsigned long int, FILE *)
 *
 * Dump the specified number of lines from the body of the message. Together with _storage_dump_headers(), this
 * forms more-or-less the entire TOP command. This also uses _storage_dump_fragment() by default.
 */
extern int (*_storage_dump_message_lines)(struct imap4_message *, unsigned long int, FILE *);

/*
 * int _storage_synch()
 *
 * Write out the mailbox, ie commit any message deletions to the storage system.
 */
extern int (*_storage_synch)();

/*
 * enum whichUser _storage_need_user()
 *
 * If any user will do, return wuAny; if you really want to be the default "safe" user, return wuDefault
 * otherwise, return wuMailbox to say you need to be the user which matches the mailbox name.
 *
 * Use this to specify which real user you need to be to access the mailbox. Default returns wuAny.
 */
extern enum whichUser (*_storage_need_user)() ;

#endif
