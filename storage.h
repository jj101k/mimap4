#ifndef __storage_h
#define __storage_h

#include <stdlib.h>

enum imapCoreFlags {imapFlagRecent=1, imapFlagDeleted=2};

struct imap4_message {
	struct imap4_message * next;
	char *uidl;
	// This is the size it will be on output, with all newlines normalised to \r\n
	unsigned long int size;
	// Where the header starts. It's up to you what this means.
	unsigned long int header_offset;
	// Where the header ends (including the last empty line)...
	unsigned long int header_end_offset;
	// ...and where the body (ie, whole message) ends.
	unsigned long int body_end_offset;
	// A count of how many lines in this message match /^\./, which you may find useful
	unsigned long int flags;
	char **extra_flags;
};

enum arrayStyle {asArray, asLinkedList};

enum whichUser {wuDefault, wuAny, wuMailbox};

#endif
