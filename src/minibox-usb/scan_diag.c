#include "scan_m1522.h"
#include "usb_backend.h"
#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <string.h>

static const char *dir(unsigned char a){return (a&LIBUSB_ENDPOINT_IN)?"IN":"OUT";}
static const char *xtype(unsigned char a){switch(a&LIBUSB_TRANSFER_TYPE_MASK){case LIBUSB_TRANSFER_TYPE_CONTROL:return "control";case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:return "iso";case LIBUSB_TRANSFER_TYPE_BULK:return "bulk";case LIBUSB_TRANSFER_TYPE_INTERRUPT:return "interrupt";default:return "unknown";}}

static int claim_test(void)
{
    struct m1522_scan_handle h;
    int r=m1522_scan_open(&h);
    if(r){fprintf(stderr,"SOAPHT claim failed: %d\n",r);return 10;}
    printf("SOAPHT claim OK: if=%d bulk_out=0x%02x bulk_in=0x%02x\n",h.iface,h.bulk_out,h.bulk_in);
    m1522_scan_close(&h);
    puts("SOAPHT release OK; no scan command or payload was sent.");
    return 0;
}

int main(int argc,char **argv)
{
    libusb_context *ctx=NULL; libusb_device_handle *dev=NULL;
    struct libusb_config_descriptor *cfg=NULL; int found=0,valid=0,r;

    if(argc==2 && strcmp(argv[1],"--claim")==0) return claim_test();
    if(argc>1){fprintf(stderr,"usage: %s [--claim]\n",argv[0]);return 1;}

    r=libusb_init(&ctx); if(r){fprintf(stderr,"libusb_init: %d\n",r);return 2;}
    dev=libusb_open_device_with_vid_pid(ctx,MINIBOX_HP_VID,MINIBOX_M1522_PID);
    if(!dev){fprintf(stderr,"M1522 03f0:4517 not found\n");libusb_exit(ctx);return 3;}
    r=libusb_get_active_config_descriptor(libusb_get_device(dev),&cfg);
    if(r){fprintf(stderr,"active config: %d\n",r);libusb_close(dev);libusb_exit(ctx);return 4;}
    printf("HP LaserJet M1522n 03f0:4517 USB interfaces\n");
    for(int i=0;i<cfg->bNumInterfaces;i++){
        const struct libusb_interface *in=&cfg->interface[i];
        for(int a=0;a<in->num_altsetting;a++){
            const struct libusb_interface_descriptor *x=&in->altsetting[a];
            int soap=x->bInterfaceClass==M1522_SOAPHT_CLASS&&x->bInterfaceSubClass==M1522_SOAPHT_SUBCLASS&&x->bInterfaceProtocol==M1522_SOAPHT_PROTOCOL;
            unsigned char bulk_in=0,bulk_out=0;
            printf("if=%u alt=%u class=%02x/%02x/%02x%s\n",x->bInterfaceNumber,x->bAlternateSetting,x->bInterfaceClass,x->bInterfaceSubClass,x->bInterfaceProtocol,soap?" SOAPHT":"");
            for(int e=0;e<x->bNumEndpoints;e++){
                const struct libusb_endpoint_descriptor *ep=&x->endpoint[e];
                printf("  ep=0x%02x %s %s maxpacket=%u\n",ep->bEndpointAddress,dir(ep->bEndpointAddress),xtype(ep->bmAttributes),ep->wMaxPacketSize);
                if(soap && (ep->bmAttributes&LIBUSB_TRANSFER_TYPE_MASK)==LIBUSB_TRANSFER_TYPE_BULK){
                    if(ep->bEndpointAddress&LIBUSB_ENDPOINT_IN) bulk_in=ep->bEndpointAddress;
                    else bulk_out=ep->bEndpointAddress;
                }
            }
            if(soap){
                found=1;
                if(bulk_in&&bulk_out){
                    valid=1;
                    printf("SOAPHT transport candidate: if=%u bulk_out=0x%02x bulk_in=0x%02x\n",x->bInterfaceNumber,bulk_out,bulk_in);
                } else {
                    fprintf(stderr,"SOAPHT interface %u lacks required bulk IN/OUT pair\n",x->bInterfaceNumber);
                }
            }
        }
    }
    libusb_free_config_descriptor(cfg); libusb_close(dev); libusb_exit(ctx);
    if(!found){fprintf(stderr,"SOAPHT ff/02/01 interface not found\n");return 5;}
    if(!valid){fprintf(stderr,"SOAPHT interface found but no usable bulk IN/OUT transport exists\n");return 6;}
    puts("SOAPHT ff/02/01 with bulk IN/OUT present; no scan command or payload was sent.");
    puts("Run with --claim to test safe claim/release without sending any scan payload.");
    return 0;
}
