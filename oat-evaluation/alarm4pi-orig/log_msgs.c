#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/file.h>
#include <stdarg.h>
#include "log_msgs.h"


int Console_messages=1; // Indicate whether info messages should be printed in console

FILE *Log_file_handle=NULL, *Event_file_handle=NULL; // File handle to write debug messages

//////////////////////////////////////////////////////// DEBUG FUNCTIONS ////////////////////////////////
// Get date and time string and store it in the specified buffer of specified max lentgh
void get_localtime_str(char *cur_time_str, size_t cur_time_str_len)
  {
   time_t cur_time;
   struct tm *cur_time_struct;

   cur_time = time(NULL);  // Get current time
   if(cur_time != (time_t)-1)
     {
      cur_time_struct = localtime(&cur_time); // Use local time (not UTC)
      if(strftime(cur_time_str, cur_time_str_len, "%Y-%m-%d %H:%M:%S", cur_time_struct)==0) // No output 
         if(cur_time_str_len>0) // if strftime() returns 0, the contents of the array may be undefined
            cur_time_str[0]='\0'; // Terminate string 
     }
   else
     {
      if(cur_time_str_len>0) // Error getting time, terminate string 
         cur_time_str[0]='\0';
     }

  }

// Print debug messaages (event or log) using printf argument format.
// If the specified file has been opened (!= NULL), write messages on it
// If console debug mode is enabled (Debug_messages != 0), write messages in console as well
int msg_printf(FILE *out_file_handle, const char *format, ...)
  {
   int ret;
   if(Console_messages || out_file_handle != NULL)
     {
      int printf_ret=0,fprintf_ret=0;
      va_list arglist;
      char cur_time_str[20];

	    get_localtime_str(cur_time_str, sizeof(cur_time_str));
      va_start(arglist, format);
      if(Console_messages)
         printf_ret=vprintf(format, arglist);
      if(out_file_handle != NULL)
        {
         fprintf(out_file_handle, "[%s] ",cur_time_str);
         fprintf_ret=vfprintf(out_file_handle, format, arglist);      
        }
      va_end(arglist);
      ret=(printf_ret!=0)?printf_ret:fprintf_ret;
     }
   else
      ret=0;
   return(ret);
  }

// Open (or creates) a out-message text file for writing info messages
// If max_file_len<>0, truncates the file to prevent if from becoming huge
FILE *open_msg_file(const char *file_name, long max_file_len)
  {
   FILE *file_handle;
   file_handle=fopen(file_name,"a+t"); // create a new file if it does not exist
   if(file_handle)
     {
      long log_size;
      size_t log_size_loaded;
      char cur_time_str[20];
    
      flock(fileno(file_handle), LOCK_UN); // Remove existing file lock held by this process
      setbuf(file_handle, NULL); // Disable file buffer: Otherwise several processes may write simultaneously to this same file using different buffers

      fseek(file_handle, 0, SEEK_END); // Move read file pointer to the end for ftell
      log_size = ftell(file_handle); // Log file size
      // Truncate file
      if (log_size > max_file_len)
        {
         char *log_file_buf;
         
         log_file_buf=(char *)malloc(max_file_len*sizeof(char));
         if(log_file_buf)
           {         
            fseek(file_handle, -max_file_len, SEEK_END);
            log_size_loaded = fread(log_file_buf, sizeof(char), max_file_len, file_handle);
            fclose(file_handle);
            free(log_file_buf);
            file_handle = fopen(file_name,"wt"); // Delete previous log file content
            if (file_handle)
              {
               get_localtime_str(cur_time_str, sizeof(cur_time_str));

               fprintf(file_handle,"\n[%s] <Old messages deleted>\n\n", cur_time_str);
               fwrite(log_file_buf, sizeof(char), log_size_loaded, file_handle);
              }
           }
        }

      if(file_handle)
        {
         get_localtime_str(cur_time_str, sizeof(cur_time_str));
         fprintf(file_handle, "[%s] --------------------- Log initiated ---------------------\n", cur_time_str);     
         fprintf(file_handle, "[%s] iAlarm daemon running\n", cur_time_str);
        }
     }
   return(file_handle);
  }

// Closes log file if file_handle is not NULL
void close_log_file(FILE *file_handle)
  {
   if(file_handle)
     {
      char cur_time_str[20];

      get_localtime_str(cur_time_str, sizeof(cur_time_str));
      fprintf(file_handle,"[%s] iAlarm daemon terminated\n\n", cur_time_str);
      fclose(file_handle);
     } 
  }

int open_log_files(void)
  {
   Log_file_handle=open_msg_file(LOG_FILE_NAME, MAX_PREV_MSG_FILE_SIZE);
   Event_file_handle=open_msg_file(EVENT_FILE_NAME, MAX_PREV_MSG_FILE_SIZE);
   return(Log_file_handle == NULL || Event_file_handle == NULL);
  }
 
void close_log_files(void)
  {
   close_log_file(Event_file_handle);
   close_log_file(Log_file_handle);
  }
