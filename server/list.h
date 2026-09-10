#ifndef _LIST_H_
#define _LIST_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

typedef struct singlelist
{
    int sock;
    char ip[20];
    char username[64];
    unsigned short port;
    bool authenticated;
    struct singlelist *next;
} list;

extern list *list_init(void);
extern int list_insert_tail(list *newnode, list *head);
extern int list_delete(char *delip, unsigned short delport, list *head);
extern int list_delete_node(list *node, list *head);

#endif
