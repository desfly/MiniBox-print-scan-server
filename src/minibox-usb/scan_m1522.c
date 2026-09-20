#include "scan_m1522.h"
#include "usb_backend.h"
#include <limits.h>
#include <string.h>

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
    if(libusb_init(&h->ctx))return -2;
    h->dev=libusb_open_device_with_vid_pid(h->ctx,MINIBOX_HP_VID,MINIBOX_M1522_PID);
    if(!h->dev){m1522_scan_close(h);return -3;}
    if(find_soapht(libusb_get_device(h->dev),h)){m1522_scan_close(h);return -4;}
    if(libusb_kernel_driver_active(h->dev,h->iface)==1)libusb_detach_kernel_driver(h->dev,h->iface);
    if(libusb_claim_interface(h->dev,h->iface)){m1522_scan_close(h);return -5;}
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
    return r?-r:done;
}

int m1522_scan_read(struct m1522_scan_handle *h,unsigned char *buf,size_t cap,size_t *got,int timeout_ms){
    int done=0,r;
    if(!h||!h->dev||!h->bulk_in||!buf||!got||!cap||cap>(size_t)INT_MAX)return -1;
    *got=0;
    r=libusb_bulk_transfer(h->dev,h->bulk_in,buf,(int)cap,&done,timeout_ms);
    if(r)return -r;
    *got=(size_t)done;
    return 0;
}
