#ifndef LOG_MSGS_H
#define LOG_MSGS_H
#include <stdio.h>

#define LOG_FILE_PATH "/var/log/alarm4pi/"
#define LOG_FILE_NAME LOG_FILE_PATH"daemon.log" // Debug log file. Debug messages will be written into it (and maybe console as well)
#define EVENT_FILE_NAME LOG_FILE_PATH"events.log" // Events log file
#define MAX_PREV_MSG_FILE_SIZE 50*1024*1024

extern FILE *Log_file_handle, *Event_file_handle;

int open_log_files(void);
void close_log_files(void);
int msg_printf(FILE *out_file_handle, const char *format, ...);

// Print event messaages using printf argument format.
// If the event file has been opened (Event_file_handle != NULL), write event messages on it
// If console debug mode is enabled (Debug_messages != 0), write event messages in console as well
#define event_printf(...) msg_printf(Event_file_handle, __VA_ARGS__)

// Print log  messaages using printf argument format.
// If the log file has been opened (Log_file_handle != NULL), write log messages on it
// If console debug mode is enabled (Debug_messages != 0), write log messages in console as well
#define log_printf(...) msg_printf(Log_file_handle, __VA_ARGS__)


#endif
