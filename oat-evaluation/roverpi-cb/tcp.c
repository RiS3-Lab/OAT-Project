#ifndef TCP_C
#define TCP_C
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "tcp.h"

void tcpError(const char *msg) {
    perror(msg);
    exit(1);
}

void *tcpListener(void *arg){
	char buffer[5];
	int sockfd;
	socklen_t clilen;
    int count;
	struct sockaddr_in serv_addr, cli_addr;
       printf("%s %d\n",__func__, __LINE__);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		tcpError("ERROR opening socket");
       printf("%s %d\n",__func__, __LINE__);
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
       printf("%s %d\n",__func__, __LINE__);
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		tcpError("ERROR on binding");
       printf("%s %d\n",__func__, __LINE__);
	listen(sockfd,5);
       printf("%s %d\n",__func__, __LINE__);
	clilen = sizeof(cli_addr);
	delay(2000);
	newsockfd = accept(sockfd,(struct sockaddr *) &cli_addr,&clilen);
       printf("%s %d\n",__func__, __LINE__);
	if (newsockfd < 0)
		tcpError("ERROR on accept");
       printf("%s %d\n",__func__, __LINE__);
	while(count++ < 5){
		bzero(buffer,5);
		n = read(newsockfd,buffer,4);
		mode = buffer[0];
		//if(mode)
			printf("Recieved 0x%x\n",buffer[0]);
	}
	close(newsockfd);
	close(sockfd);
	return NULL;
}
#endif
