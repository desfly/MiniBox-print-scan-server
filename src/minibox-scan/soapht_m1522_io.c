#include "soapht_m1522_io.h"
#include "../minibox-usb/scan_m1522.h"
#include <string.h>

static int io_open(void *ctx,const char *channel)
{
    struct m1522_scan_handle *h=(struct m1522_scan_handle *)ctx;
    if(!h||!channel||strcmp(channel,MINIBOX_SOAPHT_CHANNEL))return -1;
    return m1522_scan_open(h);
}

static int io_write(void *ctx,const unsigned char *buf,size_t len)
{
    struct m1522_scan_handle *h=(struct m1522_scan_handle *)ctx;
    size_t off=0;
    while(off<len){
        int n=m1522_scan_write(h,buf+off,len-off,5000);
        if(n<=0)return -1;
        off+=(size_t)n;
    }
    return 0;
}

static int io_read(void *ctx,unsigned char *buf,size_t cap,size_t *got)
{
    return m1522_scan_read((struct m1522_scan_handle *)ctx,buf,cap,got,5000);
}

static void io_close(void *ctx)
{
    m1522_scan_close((struct m1522_scan_handle *)ctx);
}

const struct soapht_io minibox_m1522_soapht_io={io_open,io_write,io_read,io_close};
