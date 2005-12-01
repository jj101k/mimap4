#define E_BADMESSAGE "No such message"
#define E_LOCKED "Mailbox locked"
#define E_BADLOGIN "Your username or password is incorrect"
#define E_INVALID_MESSAGE_ID "That message ID does not exist"
#define E_ALREADY_DELETED "That message has been deleted previously"
#define E_INTERNAL_ERROR "internal error"
#define E_NOT_IMPLEMENTED "not implemented"
#define E_INVALID_COMMAND "not implemented, or not available in this context"
#define E_BAD_ARGUMENTS "wrong number of arguments supplied"
#define E_DELAY "you've logged in too recently. Please try again later."
#define E_PROTOCOL_ERROR "protocol error: you're not speaking valid IMAP4"
#define E_TIMEOUT "you've been idle too long. Disconnecting."

// According to RFC2449, this must be 'printable ASCII, excluding "<"'
#define S_SERVER_ID "IMAP4rev1 server ready"
#define S_LIST_FOLLOWS "Scan listing follows"
#define S_MESSAGE_FOLLOWS "message follows"
#define S_MESSAGE_DELETED "message deleted"
#define S_LOGIN_SUCCESS "logged in"
#define S_USER_SUCCESS "please supply a password now"
#define S_NOOP "nothing happens"
#define S_RESET "all messages undeleted"
#define S_QUIT "bye bye"
#define S_CAPA_RESPONSE "capability list follows"

#define S_SERVER_IMPLEMENTATION PACKAGE_NAME " version " PACKAGE_VERSION
