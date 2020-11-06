// Alarm deamon

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>

#include "log_msgs.h"
#include "gpio_polling.h"
#include "proc_helper.h"

#define WSERVER_ENV_VAR_NAME "LD_LIBRARY_PATH"
#define WSERVER_ENV_VAR_VAL "/usr/local/lib"
#define WEB_SERVER_PORT "8008"

/*
Hello,

On this weekend I have been hacked a preliminary CGI support. Now it is  included in the experimental branch's rev 164.
Only the following variables had been passed to the script:
  "SERVER_SOFTWARE=\"mjpg-streamer\" "
  "SERVER_PROTOCOL=\"HTTP/1.1\" "
  "SERVER_PORT=\"%d\" "  // OK
  "GATEWAY_INTERFACE=\"CGI/1.1\" "
  "REQUEST_METHOD=\"GET\" "
  "SCRIPT_NAME=\"%s\" " // OK
  "QUERY_STRING=\"%s\" " //OK

Adding another server/client related informations (such as SERVER_NAME, REMOTE_HOST, REMOTE_PORT) would make the current code much more difficult. If I guess well the current implementation would statisfy the most of the use cases.

Regards,
Miklós
sudo modprobe bcm2835-v4l2
*/
//#define SENSOR_POLLING_PERIOD_SEC 1
// configure_timer(SENSOR_POLLING_PERIOD_SEC); // Activate timer


pid_t Child_process_id[2] = {-1, -1}; // Initialize to -1 in order not to send signals if no child process created
char * const Capture_exec_args[]={"nc", "-l", "-p", "8080", "-v", "-v", NULL};
char * const Web_server_exec_args[]={"nc", "-l", "-p", "8008", "-v", "-v", NULL};

//char * const Web_server_exec_args[]={"mjpg_streamer", "-i", "input_file.so -f /tmp_ram -n webcam_pic.jpg", "-o", "output_http.so -w /usr/local/www -p "WEB_SERVER_PORT, NULL}; // WEB_SERVER_PORT is defined in port_mapping.h
//char * const Capture_exec_args[]={"raspistill", "-n", "-w", "640", "-h", "480", "-q", "10", "-o", "/tmp_ram/webcam_pic.jpg", "-bm", "-tl", "700", "-t", "0", "-th", "none", NULL};

// When Break is pressed (or SIGTERM recevied) this var is set to 1 by the signal handler fn to exit loops
volatile int Exit_daemon_loop=0; // We mau use sig_atomic_t in the declaration instead of int, but this is not needed


// This callback function should be called when the main process receives a SIGINT or
// SIGTERM signal.
// Function set_signal_handler() should be called to set this function as the handler of
// these signals
static void exit_deamon_handler(int sig)
  {
   log_printf("Signal %i received: Sending TERM signal to children.\n", sig);
   kill_processes(Child_process_id, sizeof(Child_process_id)/sizeof(pid_t));
   Exit_daemon_loop = 1;
  }

// This callback function is the handler of the SIGALRM signal
// Function set_signal_handler() should be called to set this function as the handler of
// these signals 
static void timer_handler(int signum)
  {
   static int count = 0;
   log_printf ("timer expired %d times\n", ++count); 
  }

// Sets the signal handler functions of SIGALRM, SIGINT and SIGTERM
int set_signal_handler(void)
  {
   int ret;
   struct sigaction act;

   memset (&act, '\0', sizeof(act));

   act.sa_handler = timer_handler;
   sigaction(SIGALRM, &act, NULL);

   act.sa_handler = exit_deamon_handler;
   // If the signal handler is invoked while a system call or library function call is blocked,
   // then the we want the call to be automatically restarted after the signal handler returns
   // instead of making the call fail with the error EINTR.
   act.sa_flags=SA_RESTART;
   sigaction(SIGINT, &act, NULL);
   if(sigaction(SIGTERM, &act, NULL) == 0)
      ret=0;
   else
     {
      ret=errno;
      log_printf("Error setting termination signal handler. errno=%d\n",errno);
     }
   return(ret);
  }


int main(int argc, char *argv[])
  {
   int main_err;
   pid_t capture_proc, web_server_proc;

   // main_err=daemonize("/"); // Custom fn, but it causes problems when waiting for child processes
  // main_err=daemon(0,0);
   main_err = 0;
   if(main_err == 0)
     {
      syslog(LOG_NOTICE, "iAlarm daemon started.");
      
      if(open_log_files())
         syslog(LOG_WARNING, "Error creating log files.");
      
      set_signal_handler();

      //config_UPNP(NULL);

      if(setenv(WSERVER_ENV_VAR_NAME, WSERVER_ENV_VAR_VAL, 0) != 0)
         log_printf("Error setting envoronment variable for child process. Errno=%i\n", errno);

     // if(run_background_command(&capture_proc, Capture_exec_args[0], Capture_exec_args)==0)
     //   {
     //    Child_process_id[0]=capture_proc;
     //    log_printf("Child process %s executed\n", Capture_exec_args[0]);
     //    if(run_background_command(&web_server_proc, Web_server_exec_args[0], Web_server_exec_args)==0)
     //      {
     //       Child_process_id[1]=web_server_proc;

     //       log_printf("Child process %s executed\n", Web_server_exec_args[0]);
     //     }
     //   }
      main_err = init_polling(&Exit_daemon_loop, "Server: http://%s:"WEB_SERVER_PORT);
      if(main_err == 0) // Success
        {
         wait_polling_end();
        }
      else
         log_printf("Polling thread has not been created.\n");



      sleep(1);

      log_printf("Waiting for child processes to finish\n");

      configure_timer(-1); // Stop timer

      // Wait until created process terminate or time out
      // The system timer (used for polling) is stopped by this function
      // 5
      wait_processes(Child_process_id, sizeof(Child_process_id)/sizeof(pid_t), 0);


      close_log_files();
     }
   return(main_err);
  }
