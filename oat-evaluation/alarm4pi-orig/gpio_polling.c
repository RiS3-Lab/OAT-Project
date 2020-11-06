#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h> //INET6_ADDRSTRLEN

#include "bcm_gpio.h"
#include "log_msgs.h"
#include "public_ip.h"
#include "pushover.h"

#include "cfv_bellman.h"
#include "lib/util.h"

#define PUSHOVER_CONFIG_FILENAME "pushover_conf.txt"

// The sensor value will be checked each (in seconds):
#define PIR_POLLING_PERIOD_SECS 1
// The sensor value will be remembered during (in periods):
#define PIR_PERMAN_PERS 60

// ID of the thread created by init_polling
pthread_t Polling_thread_id;
// General info string which is added to every msg sent
char Msg_info_str[INET6_ADDRSTRLEN+100];

//// for control-flow attestation
//static uint8_t user_data[8] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
//static uint8_t quote_out[128];
//static uint32_t quote_len;
//
//extern btbl_entry_t __btbl_start;
//extern btbl_entry_t __btbl_end;
//extern ltbl_entry_t __ltbl_start;
//extern ltbl_entry_t __ltbl_end;
//

/*
      // Write GPIO value
      ret_err=GPIO_write(RELAY1_GPIO, repeat % 2);
      if (0 != ret_err)
         printf("Error %d: %s\n",ret_err,strerror(ret_err));
*/

int send_info_notif(char *msg_str, char *msg_priority)
  {
   char tot_msg_str[MAX_PUSHOVER_MSG_SIZE];

   snprintf(tot_msg_str, MAX_PUSHOVER_MSG_SIZE, "%s. %s", Msg_info_str, msg_str);

   return(send_notification(tot_msg_str, msg_priority));
  }

// Precondition: Msg_info_str must point to a \0 terminated string
int update_ip_msg(char *msg_info_fmt)
  {
   int ret_err;

   char wan_address[INET6_ADDRSTRLEN];
   char curr_msg_info_str[sizeof(Msg_info_str)];

   ret_err=get_public_ip(wan_address);
   if(ret_err==0)
     {
      // Compose a general info string that will be added to each message sent
      snprintf(curr_msg_info_str, sizeof(curr_msg_info_str), msg_info_fmt, wan_address);
      if(strcmp(curr_msg_info_str, Msg_info_str) != 0) // Public IP address has chanded: Update msg info string and notification
        {
         strcpy(Msg_info_str, curr_msg_info_str);

         log_printf("Public IP address: %s\n", wan_address);

         send_info_notif("Alarm4pi running. Public IP obtained","-2");
        }
     }
   return(ret_err);
  }

void* polling_thread(volatile int *exit_polling)
  {
   int __attribute__((annotate("sensitive"))) ret_err;
   int __attribute__((annotate("sensitive"))) read_err;
   int __attribute__((annotate("sensitive"))) curr_pir_value;
   int __attribute__((annotate("sensitive"))) last_pir_value;
   int __attribute__((annotate("sensitive"))) pir_perman_counter;

    // for time measurement
   //unsigned int ebuf_size;

//   printf("EBUF_SIZE power:\n");
//   scanf("%u",&ebuf_size);
//   printf("EBUF_SIZE:%u\n", (1 << ebuf_size));
//
//   start = usecs();
//   cfa_setup((uint32_t)&__ltbl_start, (uint32_t)&__ltbl_end, (1 << ebuf_size));
//   end = usecs();
//   printf("cfa_setup time %lu us\n", end-start);
//
   event_printf("GPIO server initiated\n");

   read_err = 0; // Default thread return value
   pir_perman_counter = 0; // Sensor not activated
   last_pir_value = 0; // Assume that the sensor if off at the beginning
   int i = 0;
   while(i++ < 10) // While exit signal not triggered
     {

      // Read GPIO value
      ret_err = GPIO_read(PIR_GPIO, &curr_pir_value);
      if(ret_err == 0) // Success reading
        {
         if(curr_pir_value != last_pir_value) // Sensor output changed
           {
            if(curr_pir_value != 0)
              {
               event_printf("GPIO PIR (%i) value: %i\n", PIR_GPIO, curr_pir_value);
               send_info_notif("PIR sensor activated", "2");
              }
            last_pir_value = curr_pir_value;
           }

         if(curr_pir_value != 0) // Sensor output activated, remember its value
            pir_perman_counter = PIR_PERMAN_PERS;

        }
      else
        {
         if(read_err==0) // No error code has been logged yet
           {
            log_printf("Error %i while reading GPIO %i: %s\n", ret_err, PIR_GPIO, strerror(ret_err));
            read_err=ret_err;
           }
        }
      //sleep(PIR_POLLING_PERIOD_SECS);
      if(pir_perman_counter > 0)
         pir_perman_counter--;



     }


   event_printf("GPIO server terminated with error code: %i\n", read_err);
   return((void *)(intptr_t)read_err); // we do not know the sizeof(void *) in principle, so cast to intptr_t which has the same sizer to avoid warning
  }

int init_polling(volatile int *exit_polling, char *msg_info_fmt)
  {
   int __attribute__((annotate("sensitive"))) ret_err;
   unsigned long start, end;

   start = usecs();
   //cfv_init(1024);

   ret_err=export_gpios(); // This function and configure_gpios() will log error messages 
   if(ret_err==0)
     {
      ret_err=configure_gpios();
      if(ret_err==0)
        { 
         //ret_err=pushover_init(PUSHOVER_CONFIG_FILENAME);
         if(ret_err == 0)
           {
            Msg_info_str[0]='\0'; // Clear message info string so that update_ip_msg can compare it, detect a change and update it with the public IP
            //update_ip_msg(msg_info_fmt);



            // Create joinable thread
            //ret_err = pthread_create(&Polling_thread_id, NULL, (void *(*)(void *))&polling_thread, (void *)exit_polling);
            polling_thread(exit_polling);
            if(ret_err == 0) // If success
               log_printf("Polling thread initiated\n");
            else
               log_printf("Error %i creating polling thread: %s\n", ret_err, strerror(ret_err));

           }
        }
     }

    //cfv_quote();
    end = usecs();
    printf("round with attestation time usecs: %lu\n", end - start);

   return(ret_err);
  }

int wait_polling_end(void)
  {
   int ret_err;
   ret_err = pthread_join(Polling_thread_id, NULL);
   if(ret_err == 0) // If success
      log_printf("Polling thread terminated correctly\n");
   else
      log_printf("Error waiting for the polling thread to finish\n");
   unexport_gpios();
   return(ret_err);
  }
