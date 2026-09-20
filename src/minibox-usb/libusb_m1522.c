#include "libusb_m1522.h"
#include "usb_backend.h"
#include <string.h>

#define USB_CLASS_PRINTER 0x07
#define USB_SUBCLASS_PRINTER 0x01
#define USB_PROTOCOL_PRINTER_UNIDIR 0x01
#define USB_PROTOCOL_PRINTER_BIDIR 0x02

int m1522_printer_interface_rank(unsigned char cls,unsigned char sub,unsigned char proto,unsigned char bulk_out)
{
    if(cls!=USB_CLASS_PRINTER||sub!=USB_SUBCLASS_PRINTER||!bulk_out)return 0;
    if(proto==USB_PROTOCOL_PRINTER_BIDIR)return 2;
    if(proto==USB_PROTOCOL_PRINTER_UNIDIR)return 1;
    return 0;
}

static int endpoints(libusb_device*d,struct m1522_handle*h)
{
    struct libusb_config_descriptor*c=0;
    int best=0;
    if(libusb_get_active_config_descriptor(d,&c))return-1;
    for(int i=0;i<c->bNumInterfaces;i++){
        const struct libusb_interface*in=&c->interface[i];
        for(int a=0;a<in->num_altsetting;a++){
            const struct libusb_interface_descriptor*x=&in->altsetting[a];
            unsigned char bo=0,bi=0;
            int rank;
            for(int e=0;e<x->bNumEndpoints;e++){
                const struct libusb_endpoint_descriptor*ep=&x->endpoint[e];
                if((ep->bmAttributes&LIBUSB_TRANSFER_TYPE_MASK)!=LIBUSB_TRANSFER_TYPE_BULK)continue;
                if(ep->bEndpointAddress&LIBUSB_ENDPOINT_IN)bi=ep->bEndpointAddress;
                else bo=ep->bEndpointAddress;
            }
            rank=m1522_printer_interface_rank(x->bInterfaceClass,x->bInterfaceSubClass,x->bInterfaceProtocol,bo);
            if(rank>best){
                h->iface=x->bInterfaceNumber;
                h->bulk_out=bo;
                h->bulk_in=bi;
                best=rank;
            }
        }
    }
    libusb_free_config_descriptor(c);
    return best?0:-1;
}

int m1522_open(struct m1522_handle*h){if(!h)return-1;memset(h,0,sizeof *h);h->iface=-1;if(libusb_init(&h->ctx))return-2;h->dev=libusb_open_device_with_vid_pid(h->ctx,MINIBOX_HP_VID,MINIBOX_M1522_PID);if(!h->dev){m1522_close(h);return-3;}if(endpoints(libusb_get_device(h->dev),h)){m1522_close(h);return-4;}if(libusb_kernel_driver_active(h->dev,h->iface)==1)libusb_detach_kernel_driver(h->dev,h->iface);if(libusb_claim_interface(h->dev,h->iface)){m1522_close(h);return-5;}return 0;}
void m1522_close(struct m1522_handle*h){if(!h)return;if(h->dev){if(h->iface>=0)libusb_release_interface(h->dev,h->iface);libusb_close(h->dev);}if(h->ctx)libusb_exit(h->ctx);memset(h,0,sizeof *h);h->iface=-1;}
int m1522_bulk_write(struct m1522_handle*h,const unsigned char*b,int n,int t){int done=0;if(!h||!h->dev||!h->bulk_out||!b||n<0)return-1;int r=libusb_bulk_transfer(h->dev,h->bulk_out,(unsigned char*)b,n,&done,t);return r?-r:done;}
