// Raspberry Pi GPIO library using sysfs interface.
// Based in source exaple of Guillermo A. Amaral B. <g@maral.me>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "bcm_gpio.h"
#include "log_msgs.h"
 
int GPIO_export(int pin)
  {
   char name_buffer[PIN_NAME_MAX_BUFF_LEN];
   ssize_t bytes_written;
   int fd;
   int ret_err;

   fd = open("/sys/class/gpio/export", O_WRONLY);
   if(-1 != fd)
     {
      char path[PIN_DIRECTION_PATH_MAX_LEN];
      int fs_struct_created;
      int n_wait_cycle;

      bytes_written = snprintf(name_buffer, PIN_NAME_MAX_BUFF_LEN, "%d", pin);
      write(fd, name_buffer, bytes_written);
      close(fd);

      // Check (and wait) that the FS structure correspondng to the specified pin is created
      snprintf(path, PIN_DIRECTION_PATH_MAX_LEN, "/sys/class/gpio/gpio%d/direction", pin);
      fs_struct_created=0;
      n_wait_cycle=0;
      do
        {
         usleep(50000); // pause 50ms to allow the OS create the /sys/class/gpio/gpio... FS structure

         fd = open(path, O_WRONLY);
         if(-1 != fd)
           {
            fs_struct_created=1; // FS ready: exit loop
            close(fd);
           }
         else
            fs_struct_created=0; // open failed: keep looping
        }
      while(!fs_struct_created && n_wait_cycle++ < GPIO_EXPORT_MAX_WAIT_CYCLES);
      if(fs_struct_created)
         ret_err=0;
      else
         ret_err=errno;
     }
   else
      ret_err=errno;
   return(ret_err);
  }
 
int GPIO_unexport(int pin)
  {
   char name_buffer[PIN_NAME_MAX_BUFF_LEN];
   ssize_t bytes_written;
   int fd;
   int ret_err;

   fd = open("/sys/class/gpio/unexport", O_WRONLY);
   if(-1 != fd)
     {
      bytes_written = snprintf(name_buffer, PIN_NAME_MAX_BUFF_LEN, "%d", pin);
      write(fd, name_buffer, bytes_written);
      close(fd);
      ret_err=0;
     }
   else
       ret_err=errno;
   return(ret_err);
  }
 
int GPIO_direction(int pin, int dir)
  {
   const char *s_directions_str[2] = {"in","out"};
   char path[PIN_DIRECTION_PATH_MAX_LEN];
   int fd;
   int ret_err;
 
   snprintf(path, PIN_DIRECTION_PATH_MAX_LEN, "/sys/class/gpio/gpio%d/direction", pin);
   fd = open(path, O_WRONLY);
   if(-1 != fd)
     {
      const char *curr_dir_str;

      curr_dir_str=s_directions_str[PIN_IN_DIR != dir];
      if (-1 != write(fd, curr_dir_str, strlen(curr_dir_str)))
         ret_err=0;
      else
         ret_err=errno;
      close(fd);
     }
   else
       ret_err=errno;
   return(ret_err);
  }
 
int GPIO_read(int pin, int *value)
  {
   char path[PIN_VALUE_PATH_MAX_LEN];
   char value_str[PIN_VALUE_STR_LEN];
   int fd;
   int ret_err;

   if(value != NULL)
     {
      snprintf(path, PIN_VALUE_PATH_MAX_LEN, "/sys/class/gpio/gpio%d/value", pin);
      fd = open(path, O_RDONLY);
      if(-1 != fd)
        {
         if (-1 != read(fd, value_str, PIN_VALUE_STR_LEN-1))
           {
            value_str[PIN_VALUE_STR_LEN-1]='\0'; // just in case
            *value=atoi(value_str);
            ret_err=0;
           }
         else
            ret_err=errno;
         close(fd);
        }
      else
         ret_err=errno;
     }
   else
      ret_err=EINVAL;
   return(ret_err);
  }
 
int GPIO_write(int pin, int value)
  {
   const char *s_values_str[2] = {"0","1"};
   char path[PIN_VALUE_PATH_MAX_LEN];
   int fd;
   int ret_err;

   snprintf(path, PIN_VALUE_PATH_MAX_LEN, "/sys/class/gpio/gpio%d/value", pin);
   fd = open(path, O_WRONLY);
   if(-1 != fd)
     {
      const char *curr_dir_str;

      curr_dir_str=s_values_str[PIN_LOW_VAL != value];
      if(-1 != write(fd, curr_dir_str, strlen(curr_dir_str)))
         ret_err=0;
      else
         ret_err=errno;
      close(fd);
     }
   else
       ret_err=errno;
   return(ret_err);
  }

int export_gpios(void)
  {
   int ret_err;
   int fn_err_num;

   // Enable GPIO pins
   fn_err_num=GPIO_export(PIR_GPIO);
   if (0 == fn_err_num)
     {
      fn_err_num=GPIO_export(RELAY1_GPIO);
      if (0 == fn_err_num)
        {
         fn_err_num=GPIO_export(RELAY2_GPIO);
         if (0 == fn_err_num)
           {
            fn_err_num=GPIO_export(RELAY3_GPIO);
            if (0 == fn_err_num)
              {
               fn_err_num=GPIO_export(RELAY4_GPIO);
               if (0 == fn_err_num)
                 {
                  ret_err=0;
                 }
               else
                 {
                  ret_err=fn_err_num;
                  log_printf("While exporting output pin %d (relay 4) error %d: %s\n",RELAY4_GPIO,fn_err_num,strerror(fn_err_num));
                  GPIO_unexport(PIR_GPIO);
                  GPIO_unexport(RELAY1_GPIO);
                  GPIO_unexport(RELAY2_GPIO);
                  GPIO_unexport(RELAY3_GPIO);
                 }  
              }
            else
              {
               ret_err=fn_err_num;
               log_printf("While exporting output pin %d (relay 3) error %d: %s\n",RELAY3_GPIO,fn_err_num,strerror(fn_err_num));
               GPIO_unexport(PIR_GPIO);
               GPIO_unexport(RELAY1_GPIO);
               GPIO_unexport(RELAY2_GPIO);
              }  
           }
         else
           {
            ret_err=fn_err_num;
            log_printf("While exporting output pin %d (relay 2) error %d: %s\n",RELAY2_GPIO,fn_err_num,strerror(fn_err_num));
            GPIO_unexport(PIR_GPIO);
            GPIO_unexport(RELAY1_GPIO);
           }
        }
      else
        {
         ret_err=fn_err_num;
         log_printf("While exporting output pin %d (relay 1) error %d: %s\n",RELAY1_GPIO,fn_err_num,strerror(fn_err_num));
         GPIO_unexport(PIR_GPIO);
        }
     }
   else
     {
      ret_err=fn_err_num;
      log_printf("While exporting input pin %d (PIR) error %d: %s\n",PIR_GPIO,fn_err_num,strerror(fn_err_num));
     }

   return(ret_err);
  } 

int configure_gpios(void)
  {
   int ret_err;
   int curr_gpio;

   // Set GPIO directions
   curr_gpio=PIR_GPIO;
   ret_err=GPIO_direction(curr_gpio, PIN_IN_DIR);
   if (0 == ret_err)
     {
      curr_gpio=RELAY1_GPIO;
      ret_err=GPIO_direction(curr_gpio, PIN_OUT_DIR);
      if (0 == ret_err)
        {
         GPIO_write(curr_gpio, PIN_HIGH_VAL);
         curr_gpio=RELAY2_GPIO;
         ret_err=GPIO_direction(curr_gpio, PIN_OUT_DIR);
         if (0 == ret_err)
           {
            GPIO_write(curr_gpio, PIN_HIGH_VAL);
            curr_gpio=RELAY3_GPIO;
            ret_err=GPIO_direction(curr_gpio, PIN_OUT_DIR);
            if (0 == ret_err)
              {
               GPIO_write(curr_gpio, PIN_HIGH_VAL);
               curr_gpio=RELAY4_GPIO;
               ret_err=GPIO_direction(curr_gpio, PIN_OUT_DIR);
               if (0 == ret_err)
                  GPIO_write(curr_gpio, PIN_HIGH_VAL);
              } 
           } 
        }
     }
   if(ret_err != 0)
      log_printf("While configuring direcction of pin %d error %d: %s\n",curr_gpio,ret_err,strerror(ret_err));

   return(ret_err);
  }

int unexport_gpios(void)
  {
   int ret_err;

   ret_err=0;
   // Disable GPIO pins
   ret_err|= GPIO_unexport(PIR_GPIO);
   ret_err|= GPIO_unexport(RELAY1_GPIO);
   ret_err|= GPIO_unexport(RELAY2_GPIO);
   ret_err|= GPIO_unexport(RELAY3_GPIO);
   ret_err|= GPIO_unexport(RELAY4_GPIO);
   if(ret_err != 0)
      log_printf("While unexporting GPIO pins error %d: %s\n",ret_err,strerror(ret_err));

   return(ret_err);
  }
