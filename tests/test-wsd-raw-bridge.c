#define _POSIX_C_SOURCE 200809L
#include "../src/minibox-discoveryd/wsd_raw_bridge.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
int main(void){
    int fd=socket(AF_INET,SOCK_STREAM,0),s;struct sockaddr_in addr;
    socklen_t alen=sizeof addr;pid_t pid;
    static const unsigned char raw[]={0x1b,0x25,0x2d,0x31,0x32,0x33,0x34,0x35,0x58,0x00,0xff,0x21};
    unsigned char got[sizeof raw];size_t n=0;int status;
    assert(fd>=0);
    memset(&addr,0,sizeof addr);addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);addr.sin_port=0;
    assert(bind(fd,(struct sockaddr *)&addr,sizeof addr)==0);
    assert(getsockname(fd,(struct sockaddr *)&addr,&alen)==0);
    assert(listen(fd,1)==0);
    pid=fork();assert(pid>=0);
    if(!pid){
        close(fd);
        _exit(mb_wsd_raw_submit(raw,sizeof raw,ntohs(addr.sin_port))==0?0:1);
    }
    s=accept(fd,0,0);assert(s>=0);
    while(n<sizeof got){
        ssize_t k=recv(s,got+n,sizeof got-n,0);
        assert(k>0);n+=(size_t)k;
    }
    assert(!memcmp(raw,got,sizeof raw));
    assert(recv(s,got,sizeof got,0)==0);
    close(s);close(fd);
    assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    assert(mb_wsd_raw_submit(raw,0,9100)!=0);
    assert(mb_wsd_raw_submit(raw,sizeof raw,0)!=0);
    return 0;
}
