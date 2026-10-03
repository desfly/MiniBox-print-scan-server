#include "scan_m1522.h"
#include "usb_backend.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

static int find_soapht(libusb_device *d,struct m1522_scan_handle *h){
    struct libusb_config_descriptor *c=NULL;
    int found=-1;
    if(libusb_get_active_config_descriptor(d,&c))return -1;
    for(int i=0;i<c->bNumInterfaces&&found;i++){
        const struct libusb_interface *in=&c->interface[i];
        for(int a=0;a<in->num_altsetting&&found;a++){
            const struct libusb_interface_descriptor *x=&in->altsetting[a];
            unsigned char bo=0,bi=0;
            if(x->bInterfaceClass!=M1522_SOAPHT_CLASS||x->bInterfaceSubClass!=M1522_SOAPHT_SUBCLASS||x->bInterfaceProtocol!=M1522_SOAPHT_PROTOCOL)continue;
            for(int e=0;e<x->bNumEndpoints;e++){
                const struct libusb_endpoint_descriptor *ep=&x->endpoint[e];
                if((ep->bmAttributes&LIBUSB_TRANSFER_TYPE_MASK)!=LIBUSB_TRANSFER_TYPE_BULK)continue;
                if(ep->bEndpointAddress&LIBUSB_ENDPOINT_IN)bi=ep->bEndpointAddress;else bo=ep->bEndpointAddress;
            }
            if(bo&&bi){h->iface=x->bInterfaceNumber;h->bulk_out=bo;h->bulk_in=bi;found=0;}
        }
    }
    libusb_free_config_descriptor(c);
    return found;
}

int m1522_scan_open(struct m1522_scan_handle *h){
    if(!h)return -1;
    memset(h,0,sizeof *h);h->iface=-1;
    {
        int rc=libusb_init(&h->ctx);
        if(rc){fprintf(stderr,"minibox-scand: stage=libusb-init rc=%d\n",rc);return -2;}
    }
    h->dev=libusb_open_device_with_vid_pid(h->ctx,MINIBOX_HP_VID,MINIBOX_M1522_PID);
    if(!h->dev){fprintf(stderr,"minibox-scand: stage=libusb-open-device rc=-3\n");m1522_scan_close(h);return -3;}
    if(find_soapht(libusb_get_device(h->dev),h)){fprintf(stderr,"minibox-scand: stage=libusb-find-interface rc=-4\n");m1522_scan_close(h);return -4;}
    {
        int rc=libusb_kernel_driver_active(h->dev,h->iface);
        if(rc==1){
            rc=libusb_detach_kernel_driver(h->dev,h->iface);
            if(rc)fprintf(stderr,"minibox-scand: stage=libusb-detach rc=%d\n",rc);
        } else if(rc<0 && rc!=LIBUSB_ERROR_NOT_SUPPORTED){
            fprintf(stderr,"minibox-scand: stage=libusb-driver-check rc=%d\n",rc);
        }
    }
    {
        int rc=libusb_claim_interface(h->dev,h->iface);
        if(rc){fprintf(stderr,"minibox-scand: stage=libusb-claim rc=%d\n",rc);m1522_scan_close(h);return -5;}
    }
    return 0;
}

void m1522_scan_close(struct m1522_scan_handle *h){
    if(!h)return;
    if(h->dev){if(h->iface>=0)libusb_release_interface(h->dev,h->iface);libusb_close(h->dev);}
    if(h->ctx)libusb_exit(h->ctx);
    memset(h,0,sizeof *h);h->iface=-1;
}

int m1522_scan_write(struct m1522_scan_handle *h,const unsigned char *buf,size_t len,int timeout_ms){
    int done=0,r;
    if(!h||!h->dev||!h->bulk_out||(!buf&&len)||len>(size_t)INT_MAX)return -1;
    r=libusb_bulk_transfer(h->dev,h->bulk_out,(unsigned char *)buf,(int)len,&done,timeout_ms);
    if (r || (len && !done))
        fprintf(stderr, "minibox-scand: stage=libusb-bulk-out rc=%d bytes=%d requested=%zu ep=0x%02x timeout_ms=%d\n",
                r, done, len, h->bulk_out, timeout_ms);
    return r?r:done;
}

static void retry_delay_100ms(void){
    struct timeval tv;
    tv.tv_sec=0;
    tv.tv_usec=100000;
    (void)select(0,NULL,NULL,NULL,&tv);
}

int m1522_scan_read(struct m1522_scan_handle *h,unsigned char *buf,size_t cap,size_t *got,int timeout_ms){
    int done=0,r=0,error_attempt=0;
    unsigned long zero_reads=0;
    struct timeval started,now;
    long elapsed_ms;
    if(!h||!h->dev||!h->bulk_in||!buf||!got||!cap||cap>(size_t)INT_MAX)return -1;
    *got=0;
    /*
     * M1522 uses successful zero-length bulk-IN completions as flow-control
     * while scan data is being produced.  The verified USBPcap contains
     * tens of thousands of these between non-empty packets (up to ~11 s
     * before RetrieveImage data and ~2 s inside the image).  Sleeping 100 ms
     * after every few zero completions throttles the stream so severely that
     * Windows waits forever after the physical scan has completed.
     *
     * Drain zero-length successes immediately for one caller timeout window.
     * Real libusb timeout/I/O errors keep the bounded retry/backoff path.
     */
    gettimeofday(&started,NULL);
    for(;;){
        done=0;
        r=libusb_bulk_transfer(h->dev,h->bulk_in,buf,(int)cap,&done,timeout_ms);
        if(!r && done>0){
            *got=(size_t)done;
            return 0;
        }
        if(!r && done==0){
            ++zero_reads;
            gettimeofday(&now,NULL);
            elapsed_ms=(long)(now.tv_sec-started.tv_sec)*1000L+
                       (long)(now.tv_usec-started.tv_usec)/1000L;
            if(elapsed_ms < timeout_ms) continue;
            fprintf(stderr,
                    "minibox-scand: stage=libusb-bulk-in rc=0 bytes=0 requested=%zu ep=0x%02x timeout_ms=%d zero_reads=%lu elapsed_ms=%ld\n",
                    cap,h->bulk_in,timeout_ms,zero_reads,elapsed_ms);
            return LIBUSB_ERROR_TIMEOUT;
        }
        if(r==LIBUSB_ERROR_TIMEOUT || r==LIBUSB_ERROR_IO){
            ++error_attempt;
            fprintf(stderr,
                    "minibox-scand: stage=libusb-bulk-in rc=%d bytes=%d requested=%zu ep=0x%02x timeout_ms=%d attempt=%d/4\n",
                    r,done,cap,h->bulk_in,timeout_ms,error_attempt);
            if(error_attempt>=4)return r;
            retry_delay_100ms();
            continue;
        }
        return r;
    }
}
