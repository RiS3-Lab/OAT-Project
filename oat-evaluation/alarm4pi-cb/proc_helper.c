#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> // for readlink
#include <errno.h> // for errno var and value definitions
#include <libgen.h> // For basename
#include <linux/limits.h> // For MAXPATH
#include <sys/time.h>
#include <fcntl.h>

// for daemonize()
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "log_msgs.h"

int get_current_exec_path(char *exec_path, size_t path_buff_len)
  {
   int ret_error;
   if(path_buff_len > 0)
     {
      char exec_path_buff[PATH_MAX+1];
      size_t chars_written;

      chars_written=readlink("/proc/self/exe", exec_path_buff, PATH_MAX);
      if(chars_written != -1) // Success
        {
         char *exec_dir;
         exec_path_buff[chars_written]='\0';
         exec_dir=dirname(exec_path_buff);
         if(path_buff_len > strlen(exec_dir)+1) // If there is enough space in supplied buffer:
           {
            strcpy(exec_path,exec_dir);
            strcat(exec_path,"/");
            ret_error=0;
           }
         else
           {
            exec_path[0]='\0';
            ret_error=EINVAL; // Invalid input parameter (input buffer size)
           }
        }
      else
        {
         exec_path[0]='\0';
         ret_error=errno;
        }
     }
   else
      ret_error=EINVAL;
   return(ret_error);
  }

void kill_processes(pid_t *process_ids, size_t n_processes)
  {
   int n_child;
   for(n_child=0;n_child<n_processes;n_child++)
      if(process_ids[n_child] != -1)
         kill(process_ids[n_child], SIGTERM);   
  }

int wait_processes(pid_t *process_ids, size_t n_processes, int wait_timeout)
  {
   int ret_error;
   int n_remaining_procs;

   ret_error=0;
   do
     {
      int wait_ret;

      n_remaining_procs=0;
      alarm(wait_timeout);
      wait_ret=waitpid(0, NULL, 0); // wait for any child process whose process group ID is equal to that of the calling process.
      if(wait_ret != -1) // Valid PID returned
        {
         int n_child;

         for(n_child=0;n_child<n_processes;n_child++)
            if(process_ids[n_child] != -1) // A process is remaining in the list
              {
               if(process_ids[n_child] == wait_ret) // Is the process that has just died?
                 {
                  log_printf("Child process with PID: %i terminated.\n", wait_ret);
                  process_ids[n_child] = -1; // Mark process as dead
                }
               else
                  n_remaining_procs++; // We still have to wait for his process
              }
        }
      else
        {
         ret_error=errno; // Error: exit loop
         log_printf("Error waiting for child process to finish. errno %i: %s\n", errno, strerror(errno));
        }
     }
   while(n_remaining_procs > 0);
   return(ret_error);
  }

int run_background_command(pid_t *new_proc_id, const char *exec_filename, char *const exec_argv[])
  {
   int ret;

   *new_proc_id = fork(); // Fork off the parent process
   
   if(*new_proc_id == 0) // Fork off child
     {
      int null_fd_rd;
      if(Log_file_handle != NULL)
        {
         if(dup2(fileno(Log_file_handle), STDOUT_FILENO) == -1)
            log_printf("Creating process %s: failed redirect standard output. errno=%d\n",exec_filename,errno);
         if(dup2(fileno(Log_file_handle), STDERR_FILENO) == -1)
            log_printf("Creating process %s: failed redirect standard error output. errno=%d\n",exec_filename,errno);
         fclose(Log_file_handle);
        }
      if(Event_file_handle != NULL)
         fclose(Event_file_handle);
      null_fd_rd=open ("/dev/null", O_RDONLY);
      if(null_fd_rd != -1)
        {
         if(dup2(null_fd_rd, STDIN_FILENO) == -1)
            log_printf("Creating process %s: failed redirect standard input. errno=%d\n",exec_filename,errno);
         close(null_fd_rd);
        }
      else
         log_printf("Creating process %s: could not open null device for reading. errno=%d\n",exec_filename,errno);

      close(STDIN_FILENO);
      execvp(exec_filename, exec_argv);
      log_printf("Creating process %s: failed to execute capture program. errno=%d\n",exec_filename,errno);
      exit(errno); // exec failed, exit child
     }
   else
     {
      if(*new_proc_id > 0)
         ret=0; // success
      else // < 0: An error occurred
        {
         ret=errno;
         log_printf("Creating process %s: first fork failed. errno=%d\n",exec_filename,errno);
        }
     }
   return(ret);
  }

int configure_timer(float interval_sec)
  {
   int ret_error;
   struct itimerval timer_conf;

   if(interval_sec < 0) // Disable the timer
     {
      // Set the timer to zero: A timer whose it_value is zero or the timer expires
      // and it_interval is zero stops
      timer_conf.it_value.tv_sec = 0;
      timer_conf.it_value.tv_usec = 0;
      timer_conf.it_interval.tv_sec = 0;
      timer_conf.it_interval.tv_usec = 0;
     }
   else
     {
      // Configure the timer to expire (ring) after 250 ms the first time
      timer_conf.it_value.tv_sec = 0;
      timer_conf.it_value.tv_usec = 250000;
      // and to expire again every interval_sec seconds
      timer_conf.it_interval.tv_sec = (time_t)interval_sec;
      timer_conf.it_interval.tv_usec = (suseconds_t)((interval_sec-timer_conf.it_interval.tv_sec)*1.0e6);
     }

   // Start a real time timer, which deliver a SIGALARM
   if(setitimer (ITIMER_REAL, &timer_conf, NULL) == 0)
     {
      log_printf("Sensor polling (timer) set to %lis and %lius\n", timer_conf.it_interval.tv_sec, timer_conf.it_interval.tv_usec);
      ret_error=0; // Timer set successfully
     }
   else
     {
      ret_error=errno;
      log_printf("Error setting timer: errno %i: %s\n", errno, strerror(errno));
     }
   return(ret_error);
  }

// This fn is not used since daemon() is preferred
int daemonize(char *working_dir)
  {
   int ret_error;
   pid_t child_pid;
   int null_fd_rd, null_fd_wr;

   child_pid = fork(); // Fork off the parent process
   if(child_pid != -1) // If no error occurred
     {
      if(child_pid > 0) // Success: terminate parent
         exit(EXIT_SUCCESS);

      // the the child is running here
      if(setsid() != -1) // creates a session and sets the process group ID
        {
         // Catch, ignore and handle signals
         // TODO: Implement a working signal handler
         signal(SIGCHLD, SIG_IGN);
         signal(SIGHUP, SIG_IGN);

         child_pid = fork(); // Fork off the parent process again
         if(child_pid != -1) // If no error occurred
           {
            if(child_pid > 0) // Success: terminate parent
               exit(EXIT_SUCCESS);

            umask(0); // Set new file permissions

            chdir(working_dir); // Change the working directory to an appropriated directory

            null_fd_rd=open ("/dev/null", O_RDONLY);
            if(null_fd_rd != -1)
              {
               dup2(null_fd_rd, STDIN_FILENO);
               close(null_fd_rd);
              }
            else
               perror("iAlarm daemon init error: could not open null device for reading");
            null_fd_wr=open ("/dev/null", O_WRONLY);
            if(null_fd_wr != -1)
              {
               dup2(null_fd_wr, STDERR_FILENO);
               dup2(null_fd_wr, STDOUT_FILENO);
               close(null_fd_wr);
              }
            else
               perror("iAlarm daemon init error: could not open null device for writing");

           }
         else
           {
            ret_error=errno;
            fprintf(stderr,"iAlarm daemon init error: second fork failed. errno=%d\n",errno);
           }


        }
      else
        {
         ret_error=errno;
         fprintf(stderr,"iAlarm daemon init error: child process could become session leader. errno=%d\n",errno);
        }

     }
   else
     {
      ret_error=errno;
      fprintf(stderr,"iAlarm daemon init error: first fork failed. errno=%d\n",errno);
     }

      
   return(ret_error);
  }
