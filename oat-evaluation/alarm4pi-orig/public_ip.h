#ifndef PUBLIC_IP_H
#define PUBLIC_IP_H

#include <netinet/in.h>

#define OPENDNS_NAME "resolver1.opendns.com"
#define PUBLIC_IP_RECORD "myip.opendns.com"


int hostname_to_ip(char *hostname, struct in_addr *ip_addr);
int hostname_to_ip_at_dns(char *dns_server, char *domain_name, struct in_addr *ip_addr);
int get_public_ip(char *public_ip);


#endif
