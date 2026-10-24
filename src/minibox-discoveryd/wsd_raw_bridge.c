#define _POSIX_C_SOURCE 200809L
#include "wsd_raw_bridge.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
int mb_wsd_raw_submit(const unsigned char *data,size_t length,unsigned short port){
    int fd;size_t pos=0;struct sockaddr_in dst;
    if(!data||!length||!port)return -1;
    fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd<0)return -2;
    memset(&dst,0,sizeof dst);
    dst.sin_family=AF_INET;
    dst.sin_port=htons(port);
    dst.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(connect(fd,(struct sockaddr *)&dst,sizeof dst)<0){close(fd);return -3;}
    while(pos<length){
        ssize_t written=send(fd,data+pos,length-pos,0);
        if(written<0&&errno==EINTR)continue;
        if(written<=0){close(fd);return -4;}
        pos+=(size_t)written;
    }
    if(shutdown(fd,SHUT_WR)<0){close(fd);return -5;}
    close(fd);return 0;
}
