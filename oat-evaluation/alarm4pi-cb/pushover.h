#ifndef PUSHOVER_H
#define PUSHOVER_H

// Pushover messages are currently limited to 1024 4-byte UTF-8 characters.
#define MAX_PUSHOVER_MSG_SIZE (1024*4)

// This function initializes the pushover client library
// conf_filename is a pointer to a \0-terminated string containing the filename (and path) of a text
// configuration text file containing the URL of the Pushover server to which the notifications must
// be sent, the application ID (token) associated to the messages sent and the user ID.
// The file must has the format of the following example:
//server_url=http://api.pushover.net/1/messages.json
//token=a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1
//user=b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2
// The fn returns 0 on success or a errno error code
int pushover_init(char *conf_filename);

// This function connect to pushover server and send a notification
// msg_str is a pointer to a \0-terminated string containing the notification message
// msg_priority is pointer to a \0-terminated string containing a priority number from "-2" (min) to "2" (max)
// The fn returns 0 on success or a errno error code
int send_notification(char *msg_str, char *msg_priority);

#endif // PUSHOVER_H
