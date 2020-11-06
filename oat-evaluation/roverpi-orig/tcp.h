#ifndef TCP_H
#define TCP_H
void tcpError(const char *msg);
void *tcpListener(void *arg);


int __attribute__((annotate("sensitive"))) n;
int __attribute__((annotate("sensitive"))) newsockfd;
int __attribute__((annotate("sensitive"))) portno;
char __attribute__((annotate("sensitive"))) mode;
#endif
