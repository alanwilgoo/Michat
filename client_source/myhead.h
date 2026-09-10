#ifndef _MYHEAD_H
#define _MYHEAD_H

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
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int send_all(int sock, const void *buf, size_t len)
{
    const char *p = buf;
    while(len > 0)
    {
        ssize_t n = send(sock, p, len, 0);
        if(n <= 0) return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static int recv_line(int sock, char *buf, size_t size)
{
    size_t used = 0;
    while(used + 1 < size)
    {
        char ch;
        ssize_t n = recv(sock, &ch, 1, 0);
        if(n <= 0) return -1;
        if(ch == '\n')
        {
            buf[used] = '\0';
            return 0;
        }
        buf[used++] = ch;
    }
    buf[used] = '\0';
    return -2;
}

#endif
