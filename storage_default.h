#include <stdint.h>
#include "storage.h"
enum whichUser _default_storage_need_user() ;
int _default_storage_uidl_supported() ;
enum arrayStyle _default_storage_array_style() ;
int _default_storage_dump_fragment(unsigned long int from, unsigned long int to, unsigned long int skip_lines, unsigned long int max_lines, FILE *out) ;
int _default_storage_dump_message(struct imap4_message *message, FILE *out) ;
int _default_storage_dump_headers(struct imap4_message *message, FILE *out) ;
int _default_storage_dump_message_lines(struct imap4_message *message, unsigned long int max_lines, FILE *out) ;
int _default_storage_use_mailbox(char const *mailbox) ;
int _default_storage_lock_folder(char const *folder) ;
int _default_storage_select_folder(char const *folder, char readonly) ;
char const ** _default_storage_available_flags();
uint32_t _default_storage_uidvalidity();
unsigned long int _default_storage_message_count() ;
unsigned long int _default_storage_message_sum() ;
struct imap4_message *_default_storage_first_message() ;
struct imap4_message *_default_storage_message_number(unsigned long int index) ;
int _default_storage_synch() ;
char _default_storage_is_readonly();