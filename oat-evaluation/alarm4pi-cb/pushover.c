#include <stdio.h> // printf, sprintf
#include <stdlib.h> // exit, atoi, malloc, free
#include <unistd.h> // read, write, close
#include <string.h> // memcpy, memset
#include <errno.h> // for errno var and value definitions
#include <limits.h> // max hostname length
#include <linux/limits.h> // For PATH_MAX
#include <sys/socket.h> // socket, connect
#include <netinet/in.h> // struct sockaddr_in, struct sockaddr
#include <arpa/inet.h> // for inet_ntoa (deprecated)
#include <netdb.h> // struct hostent, gethostbyname

#include "log_msgs.h"
#include "proc_helper.h"
#include "public_ip.h"
#include "pushover.h"

// The macro TOSTRING allows us to convert a literal number to a string containg that number (used to set fscanf string limits)
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define MAX_CONF_STR_LEN 80 // Max. length of variables in configuration file
#define MAX_URL_LEN 2083 // De facto URL max. length
#define SERVER_URL_START "http://" // Expected URL start (secure http not suported)
#define DEFAULT_SERVER_PORT 3000 // Port to send POST request if no port is specified in server URL
#define MAX_HTTP_HEADER_LINES 1024 // In order not to wait too long

// Variable names for the configuration file and POST body: the first character of all of these must be different due to the current conf. file parser implementtion
#define SERVER_URL "server_url="
#define TOKEN_ID "token="
#define USER_ID "user="
// Variables only for POST
#define MESSAGE_ID "message="
#define PRIORITY_ID "priority="
#define RETRY_ID "retry="
#define EXPIRE_ID "expire="
// If message priority is set to 2, these parameters set the time period at which the message will be repeated
// and the time at which the message expires (in seconds)
#define RETRY_TIME_SEC 31
#define EXPIRE_TIME_SEC 120

// Global variables obatined or derived from config file data by init fn
char Token_id[MAX_CONF_STR_LEN+1];
char User_id[MAX_CONF_STR_LEN+1];
char Server_name[HOST_NAME_MAX+1];
char Server_path[HOST_NAME_MAX+1];
struct in_addr Server_ip;
int Server_port;

int pushover_init(char *conf_filename)
  {
   int ret_error;
   FILE *conf_fd;
   char full_conf_filename[PATH_MAX+1];

   if(strlen(conf_filename)>PATH_MAX)
      return(EINVAL);

   if(conf_filename[0] != '/') // Relative path specified: obtain executable directory
     {
      ret_error = get_current_exec_path(full_conf_filename, PATH_MAX);
      if(ret_error == 0) // Directory of current executable successfully obtained
        {
         if(strlen(full_conf_filename)+strlen(conf_filename) <= PATH_MAX) // total path of conf file name is not too long
            strcat(full_conf_filename, conf_filename); // Success on getting the complete conf file path 
         else // Error path too long: try to open file with relative path
            strcpy(full_conf_filename, conf_filename);
        }
      else // Error getting executable dir: try to open file with relative path
        {
         log_printf("Error obtaining the directory of the current-process executable file: errno=%d\n", ret_error);
         strcpy(full_conf_filename, conf_filename);
        }
     }
   else // Absolute path specified: use it directly with fopen()
      strcpy(full_conf_filename, conf_filename);

   conf_fd=fopen(full_conf_filename, "rt");
   if(conf_fd != NULL)
     {
      char server_url[MAX_URL_LEN+1];

      // Init variables to empty strings.
      // If they are not empty after loading, we assume that they have been correctly loaded
      server_url[0]='\0';
      // Delete global string variables
      Token_id[0]='\0';
      User_id[0]='\0';
      strcpy(Server_path,"/");

      ret_error = 0;
      while(!feof(conf_fd) && ret_error == 0)
        {
         // Try to read any of the recognized variables
         // It is necessary that all the variables names start with a different letter, so that
         // fscanf does not get chars from file buffer if the corresponding variable is not readed
         if(fscanf(conf_fd, " "SERVER_URL" %" TOSTRING(MAX_URL_LEN) "s\n", server_url) == 0 &&
            fscanf(conf_fd, " "TOKEN_ID" %" TOSTRING(MAX_CONF_STR_LEN) "s\n", Token_id) == 0 &&
            fscanf(conf_fd, " "USER_ID" %" TOSTRING(MAX_CONF_STR_LEN) "s\n", User_id) == 0)
           {
            log_printf("Error loading Pushover config file: unknown variable name found in file\n");
            ret_error = EINVAL; // Exit loop
           }
        }
      if(ret_error == 0) // No error so far
        {
         if(strlen(server_url) > 0) // If Pushover server URL could be loaded
           {
            if(strlen(Token_id) > 0) // If token ID loaded
              {
               if(strlen(User_id) > 0) // If user ID loaded
                 {
                  if(strncmp(server_url, SERVER_URL_START, strlen(SERVER_URL_START)) == 0) // URL seems to be correct
                    {
                     char *hostname_start_ptr, *hostname_end_ptr, *path_start_prt;
                     size_t server_name_len;

                     // Parse URL to get host name and port
                     // Look for the server hostname start in the URL 
                     hostname_start_ptr=strchr(server_url+strlen(SERVER_URL_START),'@');
                     if(hostname_start_ptr == NULL) // If not user name found, assume it is the end of the URL
                        hostname_start_ptr=server_url+strlen(SERVER_URL_START);
                     else // @ sign found, skip it to get hostname start
                        hostname_start_ptr++;

                     // Look for ':' (end of domain name and beginning of port)
                     hostname_end_ptr=strchr(hostname_start_ptr,':');
                     if(hostname_end_ptr == NULL) // If port not found, search for path start (/)
                       {
                        Server_port=DEFAULT_SERVER_PORT;
                        // Look for '/' (end of domain name)
                        hostname_end_ptr=strchr(hostname_start_ptr,'/');
                        if(hostname_end_ptr == NULL) // If path not found, assume hostname end is the end of the URL
                           hostname_end_ptr=hostname_start_ptr+strlen(hostname_start_ptr);
                       }
                     else // Port number specified
                       {
                        if(sscanf(hostname_end_ptr+1,"%i",&Server_port) == 0) // Port could not be obtained
                           Server_port=DEFAULT_SERVER_PORT;
                       }

                     // Search for path start after hostname
                     path_start_prt=strchr(hostname_end_ptr,'/');
                     if(path_start_prt != NULL) // Some path found
                       {
                        size_t path_len;

                        path_len = strlen(path_start_prt);
                        if(path_len <= MAX_URL_LEN)
                          {
                           memcpy(Server_path, path_start_prt, path_len); // Replace default path (/)
                           Server_path[path_len]='\0';
                          }                        
                       }

                     server_name_len=hostname_end_ptr-hostname_start_ptr;
                     if(server_name_len <= HOST_NAME_MAX)
                       {
                        memcpy(Server_name, hostname_start_ptr, server_name_len);
                        Server_name[server_name_len]='\0';

                        // Resolve Pushover hostname
                        ret_error=hostname_to_ip(Server_name, &Server_ip);
                        if(ret_error==0)
                          {
                           log_printf("Using Pushover server %s for notifications\n",inet_ntoa(Server_ip));
                          }
                       }
                     else
                       {
                        log_printf("Error loading Pushover config file: server URL is too long (more than " TOSTRING(HOST_NAME_MAX) " characters)\n");
                        ret_error = EINVAL;
                       }
                    }
                  else
                    {
                     log_printf("Error loading Pushover config file: server URL start is not "SERVER_URL_START"\n");
                     ret_error = EINVAL;
                    }
                 }
               else
                 {
                  log_printf("Error loading Pushover config file: user id not found\n");
                  ret_error = EINVAL;
                 }
              }
            else
              {
               log_printf("Error loading Pushover config file: token id not found\n");
               ret_error = EINVAL;
              }
           }
         else
           {
            log_printf("Error loading Pushover config file: server URL not found\n");
            ret_error = EINVAL;
           }
        }
      fclose(conf_fd);
     }
   else
     {
      ret_error=errno;
      log_printf("Error opening Pushover config file %s: errno=%d\n", full_conf_filename, errno);
     }

   return(ret_error);
  }

int send_notification(char *msg_str, char *msg_priority)
  {
   int ret_error=0;
   int socket_fd;
   struct sockaddr_in server_addr;

   // create the socket
   socket_fd = socket(AF_INET, SOCK_STREAM, 0);
   if (socket_fd != -1)
     {
      // fill in sockaddr_in structure
      memset(&server_addr,0,sizeof(server_addr));
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(Server_port);
      server_addr.sin_addr = Server_ip; // Copy address structure

      // Connect to the server
      ret_error=connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
      if(ret_error == 0) // Success
        {
         FILE *socket_file;
         socket_file = fdopen(socket_fd, "r+b");
         if(socket_file != NULL)
           {
            size_t body_len;
            unsigned int http_error;
            int fscanf_ret;

            body_len = strlen(TOKEN_ID)+strlen(Token_id) + 1+strlen(USER_ID)+strlen(User_id) + 1+strlen(MESSAGE_ID)+strlen(msg_str) + 1+strlen(PRIORITY_ID)+strlen(msg_priority);

            if(strcmp(msg_priority,"2") == 0) // Max. priority selected, include parameters: retry nd expire in the body
               body_len += 1+strlen(RETRY_ID)+strlen(TOSTRING(RETRY_TIME_SEC)) + 1+strlen(EXPIRE_ID)+strlen(TOSTRING(EXPIRE_TIME_SEC));

            // fill in the parameters and send POST method request header 
            fprintf(socket_file, "POST %s HTTP/1.0\r\n", Server_path);
            fprintf(socket_file, "Host: %s\r\n", Server_name);
            fprintf(socket_file, "Content-Type: application/x-www-form-urlencoded\r\n");
            fprintf(socket_file, "Content-Length: %lu\r\n\r\n", (long unsigned int)body_len);
            fprintf(socket_file, TOKEN_ID"%s&"USER_ID"%s&"MESSAGE_ID"%s&"PRIORITY_ID"%s", Token_id, User_id, msg_str, msg_priority);
            if(strcmp(msg_priority,"2") == 0) // If max. priority: include priority-2 specific parameters
               fprintf(socket_file, "&"RETRY_ID""TOSTRING(RETRY_TIME_SEC)"&"EXPIRE_ID""TOSTRING(EXPIRE_TIME_SEC));

            // Receive response
            fscanf_ret=fscanf(socket_file, "HTTP/%*[^ ] %u %*[^\r]\n", &http_error);
            if(fscanf_ret == 1)
              {
               if(http_error == 200)
                 {
                  char http_str[MAX_URL_LEN+1];
                  char *header_line;
                  unsigned int header_line_ind;
                  int header_abort;

                  // Skip header
                  header_abort=0;
                  header_line_ind=0;
                  while((header_line=fgets(http_str, MAX_URL_LEN, socket_file)) != NULL)
                    {
                     if(http_str[0] == '\r') // Empty string mark the end of header: exit header loop
                        break;

                     header_line_ind++;
                     if(header_line_ind > MAX_HTTP_HEADER_LINES)
                       {
                        header_line=NULL;
                        header_abort=1;
                        break; // Too many lines in response
                       }
                    }
                  // Received object body
                  if(header_line != NULL)
                    {
                     int notif_state; // Notification state received from the server
                     int variables_obtined; // Flag for indicating that state variables was successfully parsed 
                     char var_name[MAX_URL_LEN+1], var_value[MAX_URL_LEN+1];;

                     // Parse JSON received object
                     fscanf(socket_file, " { "); // Skip variable block start
                     variables_obtined=0; // Variables not parsed yet \"%[^\"]\" : \"%[^\"]\" , 
                     while(fscanf(socket_file, " \"%[^\"]\" : ", var_name) == 1) // Loop while var names are obtained
                       {
                        fscanf(socket_file, " \""); // Skip quotes if found (in case var value is string)
                        if(fscanf(socket_file, " %[^,}\"]\"", var_value) == 1) // Try to get var value: get all chars until we get , } or "
                          {
                           fscanf(socket_file, " \""); // Skip quotes if value is quoted
                           fscanf(socket_file, " ,"); // Skip variable separator: ,
                           // Tuple "variable name", value received
                           if(strcmp(var_name,"status") == 0) // we have got the notification status code
                             {
                              notif_state=atoi(var_value);
                              variables_obtined=1; // Status var obtained
                             }
                          }
                       }
                     fscanf(socket_file, " }");

                     if(variables_obtined != 0)
                       {
                        if(notif_state == 1) // Server code=success: notification correctly pushed
                          {
                           ret_error=0;
                          }
                        else
                          {
                           ret_error=EBADRQC;
                           log_printf("Error status code %i received from Pushover server\n", notif_state);
                          }                           
                       }
                      else
                       {
                        ret_error=EPROTO;
                        log_printf("Invalid format of response body from Pushover server. Status code could not be obtained\n");
                       }                      
                    }
                  else
                    {
                     if(header_abort != 0) // Header reception aborted puposedly
                       {
                        ret_error=EPROTO;
                        log_printf("Too long response from Pushover server: reception aborted\n");
                       }
                     else
                       {
                        ret_error=EPROTO;
                        log_printf("Error receiving response header from Pushover server: truncated response. errno=%d\n", errno);
                       }
                    }
                 }
               else
                 {
                  ret_error=EBADRQC;
                  log_printf("HTTP error code %u received from Pushover server\n", http_error);
                 }
              }
            else
              {
               ret_error=errno;
               log_printf("Error receiving response from Pushover server: fscanf returned %i. errno=%d\n", fscanf_ret, errno);
              }
            fclose(socket_file); // Closes file and correponding file descriptor
           }
         else
           {
            log_printf("Error opening socket connected to Pushover server as file: errno=%d: %s\n", errno, strerror(errno));
            close(socket_fd); // Close file descriptor
           }
        }
      else
        {
         ret_error=errno;
         log_printf("Error connecting to Pushover server: errno=%d: %s\n", errno, strerror(errno));
         close(socket_fd);
        }
     }
   else
     {
      ret_error=errno;
      log_printf("Error creating socket for connecting to Pushover server: errno=%d\n", errno);
     }
   return(ret_error);
  }
