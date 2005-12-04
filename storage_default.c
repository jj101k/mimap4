#include <stdio.h>
#include <time.h>
#include "storage_functions.h"

enum whichUser _default_storage_need_user() {return wuAny;}

int _default_storage_uidl_supported() {return 0;}
enum arrayStyle _default_storage_array_style() {return asLinkedList;}

int _default_storage_dump_fragment(unsigned long int from, unsigned long int to, unsigned long int skip_lines, unsigned long int max_lines, FILE *out) {
	return 0;
}

int _default_storage_dump_message(struct imap4_message *message, FILE *out) {
	if(!message) return 1;
	return _storage_dump_fragment(message->header_offset, message->body_end_offset, 1, 0, out);
}

int _default_storage_dump_headers(struct imap4_message *message, FILE *out) {
	if(!message) return 1;
	return _storage_dump_fragment(message->header_offset, message->header_end_offset, 1, 0, out);
}
int _default_storage_dump_message_lines(struct imap4_message *message, unsigned long int max_lines, FILE *out) {
	if(!message) return 1;
	return _storage_dump_fragment(message->header_end_offset, message->body_end_offset, 0, max_lines+1, out);
}

int _default_storage_use_mailbox(char *mailbox) {
	return 0;
}

int _default_storage_lock_folder(char *folder) {
	return 0;
}

uint32_t _default_storage_uidvalidity() {
	/* This is stupid, but it's the best we can do! */
	return (uint32_t)time(NULL);
}

char _default_storage_is_readonly() {
	return 1;
}

char const *flags[]={NULL};
char const **_default_storage_available_flags() {
	return flags;
}

int _default_storage_select_folder(char *folder, char readonly) {
	return 0;
}

unsigned long int _default_storage_message_count() {return 0;};
unsigned long int _default_storage_message_sum() {return 0;};
struct imap4_message *_default_storage_first_message() {return NULL;};
struct imap4_message *_default_storage_message_number(unsigned long int index) {
	unsigned long int j;
	struct imap4_message *current_message;

	if(index>_storage_message_count()) return NULL;
	if(index<1) return NULL;
	current_message=_storage_first_message();
	if(index==1) return current_message;
	if(_storage_array_style()==asArray) return current_message+index-1;
	for(j=1;j<index;j++, current_message=current_message->next) {
		if(!current_message) return NULL;
	}
	return current_message;
}
int _default_storage_synch() { return 0; }
