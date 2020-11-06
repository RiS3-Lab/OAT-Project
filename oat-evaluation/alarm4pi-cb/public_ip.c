// Functions to obtain to public IP address using DNS
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include "log_msgs.h"
#include "public_ip.h"


// This function return an error message for a value of h_errno
// from a failure looking up NS records.
// res_query() converts the DNS message return code to a smaller
// list of errors and places the error value in h_errno.
char *herror_msg(int herror_cod)
  {
   char *error_str;
   switch(herror_cod)
     {
      case HOST_NOT_FOUND:
         error_str="Unknown specified host";
         break;
      case NO_DATA:
         error_str="No NS records for specified domain";
         break;
      case TRY_AGAIN:
         error_str="No response for NS query";
         break;
      default:
         error_str="Unexpected error";
         break;
     }
   return(error_str);
  }


 // return a pointer to a string describing the error code 
 // from the specified DNS response return code.
char *resp_code_msg(ns_rcode rcode)
  {
   char *code_str;
   switch(rcode)
     {
      case ns_r_formerr:
         code_str="FORMERR response";
         break;
      case ns_r_servfail:
         code_str="SERVFAIL response";
         break;
      case ns_r_nxdomain:
         code_str="NXDOMAIN response";
         break;
      case ns_r_notimpl:
         code_str="NOTIMP response";
         break;
      case ns_r_refused:
         code_str="REFUSED response";
         break;
      default:
         code_str="unexpected return code";
         break;
     }
   return(code_str);
  }

int hostname_to_ip(char *hostname, struct in_addr *ip_addr)
  {
   int ret;  
   struct addrinfo hints, *res_addr;
   int res_error;
 
   memset(&hints, 0, sizeof hints);
   hints.ai_family = AF_INET; // AF_UNSPEC
   hints.ai_socktype = 0;
 
   res_error = getaddrinfo(hostname, NULL, &hints, &res_addr);
   if(res_error == 0)
     {
      struct addrinfo *res_addr_next;

      ret=-1; // Default ret value
      // loop through all the results and connect to the first we can
      for(res_addr_next = res_addr; res_addr_next != NULL; res_addr_next = res_addr_next->ai_next) 
        {
       struct sockaddr_in *addr;

         addr = (struct sockaddr_in *)res_addr_next->ai_addr;
         *ip_addr=addr->sin_addr;
         if(ip_addr->s_addr != 0UL)
           {
            // log_printf("Obtained host IP: %s\n", inet_ntoa(addr->sin_addr));
            ret=0;
            break; // Assume a valid IP address: return it
           }
        }
     
      freeaddrinfo(res_addr); // all done with this structure
     }
   else
     {
      log_printf("Error resolving IP of hostname %s. error: %s\n", hostname, gai_strerror(res_error));
      ret=res_error;
     }
 
   return(ret);
  }

int hostname_to_ip_at_dns(char *dns_server, char *domain_name, struct in_addr *ip_addr)
  {
   int fn_ret;
   struct __res_state res_stat;

   memset(&res_stat, '\0', sizeof(res_stat)); // or res_stat.options &= ~ (RES_INIT | RES_XINIT);
   fn_ret=res_ninit(&res_stat);

   if(fn_ret == 0)
     {
      struct in_addr dns_ip;

      fn_ret=hostname_to_ip(dns_server, &dns_ip);
      if(fn_ret==0)
        {
         union
           {
            HEADER hdr;               // defined in resolv.h
            u_char buf[NS_PACKETSZ];  // defined in arpa/nameser.h
           } dns_response;            // query buffer
         int dns_response_len;  // buffer lengths

         struct in_addr saved_dns_addr[MAXNS];  // addrs saved from _res
         int saved_dns_count;   // count of addresses saved from _res
         long saved_res_options;// option field of _res befored being modified
 
         int n_dns_addr; // Saved DNS address counter

         // Save the _res name server list since
         // we will need to restore it later.
         saved_dns_count = res_stat.nscount;
         for(n_dns_addr = 0;n_dns_addr < saved_dns_count;n_dns_addr++)
            saved_dns_addr[n_dns_addr] = res_stat.nsaddr_list[n_dns_addr].sin_addr;
         saved_res_options=res_stat.options;

         // Modify _res structure to use specific DNS and search
         res_stat.options &= ~RES_DEFNAMES; // the search will append the default domain name to component names that do not contain a dot
         //_res.options &= ~RES_RECURSE; // Turn off recursion: We don't want the name server querying another server for the SOA record; this name
         // _res.retry = 2; // Reduce the number of retries: we don't want to wait too long for any one server: at most 15 seconds.
         res_stat.nsaddr_list[0].sin_addr=dns_ip; // Store the first address for host in the _res structure
         res_stat.nscount = 1;
    
         
         // Send the query message.  If there is no name server
         // running on the target host, res_send(  ) returns -1
         // and errno is ECONNREFUSED.  First, clear out errno.
         dns_response_len = res_nquery(&res_stat, domain_name, ns_c_in, ns_t_a, (u_char *)&dns_response, sizeof(dns_response));
         if(dns_response_len != -1)
           {
            ns_msg resp_handle;  // handle for response message
         
            // Initialize a handle to this response.  The handle will
            // be used later to extract information from the response.
            fn_ret=ns_initparse(dns_response.buf, dns_response_len, &resp_handle);
            if (fn_ret >= 0)
              {
               int resp_error_code;
               // Check i the response reports an error
               resp_error_code=ns_msg_getflag(resp_handle, ns_f_rcode);
               if(resp_error_code == ns_r_noerror)
                 {
                  uint16_t answer_count;
 
                   // The response should only contain one answer; if more,
                   // report the error, and proceed to the next server.
                  answer_count=ns_msg_count(resp_handle, ns_s_an);
                  if(answer_count == 1)
                    {
                     ns_rr resp_record; // expanded resource record
     
                     // Expand the answer section record number 0 into rr.
                     fn_ret=ns_parserr(&resp_handle, ns_s_an, 0, &resp_record);
                     if (fn_ret==0) // Success
                       {
                        u_int16_t resp_type; // Record type obatined

                        resp_type = ns_rr_type(resp_record);
                        // We asked for an A record; if we got something else,
                        // report the error and proceed to the next server.
                        if (resp_type == ns_t_a)
                          {
                           u_char *record_data; // character pointer to parse DNS message
                           char rec_disp_buf[256]; // For displaying the record as text

                           ns_sprintrr(&resp_handle, &resp_record, NULL, NULL, rec_disp_buf, 256);
                           log_printf("> %s\n", rec_disp_buf);    

                           record_data = (u_char *)ns_rr_rdata(resp_record); // Set record_data to point the IP address of record.

                           *ip_addr = *(struct in_addr *)record_data;
                           fn_ret=0;
                          }
                        else
                          {
                           log_printf("%s: expected answer type %d, got %d\n",inet_ntoa(dns_ip), ns_t_a, resp_type);
                           fn_ret=-2;
                          }
                       }
                     else
                       {
                        log_printf("ns_parserr: %s\n", strerror(errno));
                       }
                    }
                  else
                    {
                     log_printf("%s: expected 1 answer, got %d\n",inet_ntoa(dns_ip), answer_count);
                     fn_ret=-1;
                    }
                 }
               else
                 {
                  log_printf("DNS response reported an error (domain: %s): %s\n", inet_ntoa(dns_ip), resp_code_msg(resp_error_code));
                  fn_ret=-4;
                 }
              }
            else
              {
               log_printf("ns_initparse: %s\n", strerror(errno));
              }
           }
         else
           { // buf size: error
            if(errno == ECONNREFUSED) // no server on the host
               log_printf("Connection refused: There is no name server running on %s\n", inet_ntoa(dns_ip));
            else // anything else: no response
               log_printf("There was no response from %s (h_errno: %i: %s)\n", inet_ntoa(dns_ip), h_errno, herror_msg(h_errno));
            fn_ret=-3;;
           }

         res_stat.options = saved_res_options; // Restore options field
         res_stat.nscount = saved_dns_count; // Restore original number of name servers
         for(n_dns_addr = 0;n_dns_addr < saved_dns_count;n_dns_addr++) // Restore addresses
            res_stat.nsaddr_list[n_dns_addr].sin_addr = saved_dns_addr[n_dns_addr];
        }
     }
   else
     {
      log_printf("res_init error. errno:%i\n", errno);
     }
   return(fn_ret);
  }


int get_public_ip(char *public_ip)
  {
   int fn_ret;
   struct in_addr public_ip_addr;

   fn_ret=hostname_to_ip_at_dns(OPENDNS_NAME, PUBLIC_IP_RECORD, &public_ip_addr);
   if(fn_ret==0)
       strcpy(public_ip, inet_ntoa(public_ip_addr));

    return(fn_ret);
  }
