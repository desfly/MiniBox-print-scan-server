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
    int fail_cancel_response;
    int malformed_create_response;
    int fail_retrieve_status;
    int malformed_dime;
    int adf_mode;
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

static void make_dime(struct mock *m, unsigned char first, unsigned char second)
{
    unsigned char dime[128], *p=dime;
    memset(p,0,12); p[0]=0x0c; put32(p+8,4); p+=12;
    memcpy(p,"meta",4); p+=4;
    memset(p,0,12); p[0]=0x09; put16(p+6,10); put32(p+8,3); p+=12;
    memcpy(p,"image/jpeg",10); p+=10; *p++=0; *p++=0;
    *p++=0xff; *p++=0xd8; *p++=first; *p++=0;
    memset(p,0,12); p[0]=0x0a; put32(p+8,3); p+=12;
    *p++=second; *p++=0xff; *p++=0xd9; *p++=0;
    set_http(m,dime,(size_t)(p-dime),200,"application/dime");
}

static void stage_response(struct mock *m)
{
    static const unsigned char elements[] = "<ScanElements/>";
    static const unsigned char adf_more[] = "<ScanElements><PaperInADF>true</PaperInADF></ScanElements>";
    static const unsigned char adf_done[] = "<ScanElements><PaperInADF>false</PaperInADF></ScanElements>";
    static const unsigned char created[] = "<CreateScanJobResponseType><JobId>2</JobId></CreateScanJobResponseType>";
    static const unsigned char cancelled[] = "<CancelJobResponse/>";
    if (!m->adf_mode) {
        if (m->request_no == 1) {
            set_http(m,elements,sizeof(elements)-1,202,"application/soap+xml");
            if (m->truncate_first_response) m->response_len -= 7;
        } else if (m->request_no == 2) {
            static const unsigned char malformed[] = "<CreateScanJobResponseType/>";
            if (m->malformed_create_response)
                set_http(m,malformed,sizeof(malformed)-1,202,"application/soap+xml");
            else
                set_http(m,created,sizeof(created)-1,202,"application/soap+xml");
        }
        else if (m->request_no == 3) {
            if (m->fail_retrieve_status) {
                static const unsigned char failed[] = "<Fault/>";
                set_http(m,failed,sizeof(failed)-1,503,"application/soap+xml");
            } else {
                make_dime(m,'A','B');
                if (m->malformed_dime) {
                    const char *h = strstr((char *)m->response,"\r\n\r\n");
                    if (h) {
                        unsigned char *p=(unsigned char *)h+4;
                        while (*p && *p!='\r') p++;
                        if (p[0]=='\r' && p[1]=='\n') p+=2;
                        *p=0;
                    }
                }
            }
        }
        else {
            set_http(m,cancelled,sizeof(cancelled)-1,202,"application/soap+xml");
            if (m->fail_cancel_response) m->response_len = 0;
        }
        return;
    }
    switch (m->request_no) {
    case 1: set_http(m,elements,sizeof(elements)-1,202,"application/soap+xml"); break;
    case 2: set_http(m,created,sizeof(created)-1,202,"application/soap+xml"); break;
    case 3: make_dime(m,'A','1'); break;
    case 4: set_http(m,adf_more,sizeof(adf_more)-1,202,"application/soap+xml"); break;
    case 5: make_dime(m,'B','2'); break;
    case 6: set_http(m,adf_done,sizeof(adf_done)-1,202,"application/soap+xml"); break;
    default: set_http(m,cancelled,sizeof(cancelled)-1,202,"application/soap+xml"); break;
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
    struct escl_job job={ESCL_SOURCE_PLATEN,200,1,0,0,2550,3507};
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
    assert(strstr(m.requests,"<ImagesToTransfer>0</ImagesToTransfer>"));
    assert(strstr(m.requests,"<InputMediaSize><Width>8500</Width><Height>11690</Height></InputMediaSize>"));
    assert(strstr(m.requests,"<ScanRegionWidth>8500</ScanRegionWidth><ScanRegionHeight>11690</ScanRegionHeight>"));
    assert(strstr(m.requests,"<ColorProcessing>RGB24</ColorProcessing>"));
    assert(strstr(m.requests,"<Resolution><Width>200</Width><Height>200</Height></Resolution>"));
    assert(strstr(m.requests,"RetrieveImageRequest"));
    assert(strstr(m.requests,"<JobId>2</JobId>"));
    assert(!minibox_soapht_codec->finish(&s));
    assert(strstr(m.requests,"CancelJobRequest"));
    assert(strstr(m.requests,"<JobId>2</JobId>"));
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
    {
        struct mock adf={0};
        struct soapht_session as;
        struct escl_job ajob={ESCL_SOURCE_ADF,200,1,0,0,2550,3507};
        size_t page, n;
        int mp=-1;
        adf.adf_mode=1;
        assert(!soapht_open(&as,&io,&adf));
        assert(!minibox_soapht_codec->start(&as,&ajob));
        for(page=0;page<2;page++){
            off=0;
            do {
                assert(!minibox_soapht_codec->read_image(&as,image+off,2,&got));
                off+=got;
            } while(got);
            assert(off==6);
            if(page==0) assert(!memcmp(image,"\xff\xd8" "A1\xff\xd9",6));
            else assert(!memcmp(image,"\xff\xd8" "B2\xff\xd9",6));
            assert(!minibox_soapht_codec->end_page(&as,&mp));
            assert(mp==(page==0));
        }
        assert(!minibox_soapht_codec->finish(&as));
        soapht_close(&as);
        n=0; { const char *p=adf.requests; while((p=strstr(p,"<wscn:CreateScanJobRequest>"))){n++;p+=27;} } assert(n==1);
        n=0; { const char *p=adf.requests; while((p=strstr(p,"<wscn:RetrieveImageRequest>"))){n++;p+=26;} } assert(n==2);
        n=0; { const char *p=adf.requests; while((p=strstr(p,"<JobId>2</JobId>"))){n++;p+=16;} } assert(n==3);
        n=0; { const char *p=adf.requests; while((p=strstr(p,"<wscn:CancelJobRequest>"))){n++;p+=22;} } assert(n==1);
        assert(strstr(adf.requests,"<InputSource>ADF</InputSource>"));
    }
    {
        struct mock retrieve_fail={0};
        struct soapht_session rs;
        size_t n;
        retrieve_fail.fail_retrieve_status=1;
        assert(!soapht_open(&rs,&io,&retrieve_fail));
        assert(!minibox_soapht_codec->start(&rs,&job));
        assert(minibox_soapht_codec->read_image(&rs,image,2,&got)==-2);
        assert(!minibox_soapht_codec->finish(&rs));
        n=0; { const char *p=retrieve_fail.requests; while((p=strstr(p,"<wscn:CancelJobRequest>"))){n++;p+=22;} }
        assert(n==1);
        soapht_close(&rs);
    }
    {
        struct mock bad_dime={0};
        struct soapht_session ds;
        size_t n;
        bad_dime.malformed_dime=1;
        assert(!soapht_open(&ds,&io,&bad_dime));
        assert(!minibox_soapht_codec->start(&ds,&job));
        assert(minibox_soapht_codec->read_image(&ds,image,2,&got)==-4);
        assert(!minibox_soapht_codec->finish(&ds));
        n=0; { const char *p=bad_dime.requests; while((p=strstr(p,"<wscn:CancelJobRequest>"))){n++;p+=22;} }
        assert(n==1);
        soapht_close(&ds);
    }
    {
        struct mock malformed={0};
        struct soapht_session ms;
        size_t before;
        malformed.malformed_create_response=1;
        assert(!soapht_open(&ms,&io,&malformed));
        assert(minibox_soapht_codec->start(&ms,&job)==-4);
        before=malformed.requests_len;
        assert(!minibox_soapht_codec->finish(&ms));
        assert(malformed.requests_len==before);
        assert(!strstr(malformed.requests,"<wscn:CancelJobRequest>"));
        soapht_close(&ms);
    }
    {
        struct mock cancel_fail={0};
        struct soapht_session fs;
        size_t n, before;
        assert(!soapht_open(&fs,&io,&cancel_fail));
        assert(!minibox_soapht_codec->start(&fs,&job));
        do {
            assert(!minibox_soapht_codec->read_image(&fs,image,2,&got));
        } while(got);
        assert(!minibox_soapht_codec->end_page(&fs,&more));
        cancel_fail.fail_cancel_response=1;
        assert(minibox_soapht_codec->finish(&fs)==-3);
        n=0; { const char *p=cancel_fail.requests; while((p=strstr(p,"<wscn:CancelJobRequest>"))){n++;p+=22;} }
        assert(n==1);
        before=cancel_fail.requests_len;
        assert(!minibox_soapht_codec->finish(&fs));
        assert(cancel_fail.requests_len==before);
        soapht_close(&fs);
    }
    puts("verified M1522 SOAPHT codec and truncated-response diagnostics: OK");
    return 0;
}
