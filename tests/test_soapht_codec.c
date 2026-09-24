#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/minibox-scan/soapht_codec.h"

struct mock {
    unsigned char response[8192];
    size_t response_len, response_pos;
    char requests[16384];
    size_t requests_len;
    int request_no;
    int truncate_first_response;
};

static void put16(unsigned char *p, unsigned v)
{ p[0]=(unsigned char)(v>>8); p[1]=(unsigned char)v; }
static void put32(unsigned char *p, unsigned long v)
{ p[0]=(unsigned char)(v>>24); p[1]=(unsigned char)(v>>16); p[2]=(unsigned char)(v>>8); p[3]=(unsigned char)v; }

static void set_http(struct mock *m, const unsigned char *body, size_t body_len,
                     int status, const char *type)
{
    int n = snprintf((char *)m->response, sizeof(m->response),
        "HTTP/1.1 %d OK\r\nContent-Type: %s\r\nTransfer-Encoding: chunked\r\n\r\n%lX\r\n",
        status, type, (unsigned long)body_len);
    assert(n > 0);
    memcpy(m->response + n, body, body_len); n += (int)body_len;
    memcpy(m->response + n, "\r\n0\r\n\r\n", 7); n += 7;
    m->response_len = (size_t)n;
    m->response_pos = 0;
}

static void stage_response(struct mock *m)
{
    static const unsigned char elements[] = "<ScanElements/>";
    static const unsigned char created[] = "<CreateScanJobResponseType><JobId>2</JobId></CreateScanJobResponseType>";
    unsigned char dime[128], *p = dime;
    if (m->request_no == 1) {
        set_http(m, elements, sizeof(elements)-1, 202, "application/soap+xml");
        /* Simulate the physical 503: USB response ends before the terminating
         * chunk, so control_request must log the read/framing failure. */
        if (m->truncate_first_response) m->response_len -= 7;
    } else if (m->request_no == 2) {
        set_http(m, created, sizeof(created)-1, 202, "application/soap+xml");
    } else {
        memset(p, 0, 12); p[0]=0x0c; put32(p+8,4); p+=12;
        memcpy(p,"meta",4); p+=4;
        memset(p,0,12); p[0]=0x09; put16(p+6,10); put32(p+8,3); p+=12;
        memcpy(p,"image/jpeg",10); p+=10; *p++=0; *p++=0;
        *p++=0xff; *p++=0xd8; *p++='A'; *p++=0;
        memset(p,0,12); p[0]=0x0a; put32(p+8,3); p+=12;
        *p++='B'; *p++=0xff; *p++=0xd9; *p++=0;
        set_http(m, dime, (size_t)(p-dime), 200, "application/dime");
    }
}

static int op(void *v,const char *channel)
{ (void)v; return strcmp(channel,MINIBOX_SOAPHT_CHANNEL); }
static int wr(void *v,const unsigned char *b,size_t n)
{
    struct mock *m=v;
    assert(m->requests_len+n < sizeof(m->requests));
    memcpy(m->requests+m->requests_len,b,n); m->requests_len+=n;
    m->requests[m->requests_len]=0;
    if (n==7 && !memcmp(b,"\r\n0\r\n\r\n",7)) { ++m->request_no; stage_response(m); }
    return 0;
}
static int rd(void *v,unsigned char *b,size_t cap,size_t *got)
{
    struct mock *m=v;
    size_t left=m->response_len-m->response_pos,n=left<7?left:7;
    if(n>cap)n=cap;
    memcpy(b,m->response+m->response_pos,n);m->response_pos+=n;*got=n;
    return n?0:-1;
}
static void cl(void *v){(void)v;}

int main(void)
{
    struct mock m={0};
    struct soapht_io io={op,wr,rd,cl};
    struct soapht_session s;
    struct escl_job job={ESCL_SOURCE_PLATEN,200,1};
    unsigned char image[32]; size_t off=0,got; int more=-1;
    assert(!soapht_open(&s,&io,&m));
    assert(minibox_soapht_codec);
    assert(!minibox_soapht_codec->start(&s,&job));
    do {
        assert(!minibox_soapht_codec->read_image(&s,image+off,2,&got));
        off+=got;
    } while(got);
    assert(off==6 && !memcmp(image,"\xff\xd8" "AB\xff\xd9",6));
    assert(!minibox_soapht_codec->end_page(&s,&more) && more==0);
    assert(strstr(m.requests,"GetScannerElements"));
    assert(strstr(m.requests,"CreateScanJobRequest"));
    assert(strstr(m.requests,"<InputSource>Platen</InputSource>"));
    assert(strstr(m.requests,"<Width>200</Width><Height>200</Height>"));
    assert(strstr(m.requests,"RetrieveImageRequest"));
    assert(strstr(m.requests,"<JobId>2</JobId>"));
    assert(!minibox_soapht_codec->finish(&s));
    soapht_close(&s);
    {
        struct mock truncated={0};
        struct soapht_session failing;
        truncated.truncate_first_response=1;
        assert(!soapht_open(&failing,&io,&truncated));
        assert(minibox_soapht_codec->start(&failing,&job)==-2);
        soapht_close(&failing);
        assert(truncated.request_no==1);
    }
    puts("verified M1522 SOAPHT codec and truncated-response diagnostics: OK");
    return 0;
}
