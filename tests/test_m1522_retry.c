#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>
int usleep(unsigned int);

static int seq_rc[8],seq_done[8],seq_len,calls;
int libusb_bulk_transfer(libusb_device_handle*d,unsigned char ep,unsigned char*b,int n,int*done,unsigned int t)
{ (void)d;(void)ep;(void)b;(void)n;(void)t; assert(calls<seq_len); *done=seq_done[calls]; return seq_rc[calls++]; }
int libusb_get_active_config_descriptor(libusb_device*d,struct libusb_config_descriptor**c){(void)d;(void)c;return -1;}
void libusb_free_config_descriptor(struct libusb_config_descriptor*c){(void)c;}
int libusb_init(libusb_context **c){(void)c;return -1;}
libusb_device_handle *libusb_open_device_with_vid_pid(libusb_context*c,unsigned short v,unsigned short p){(void)c;(void)v;(void)p;return 0;}
libusb_device *libusb_get_device(libusb_device_handle*d){(void)d;return 0;}
int libusb_kernel_driver_active(libusb_device_handle*d,int i){(void)d;(void)i;return 0;}
int libusb_detach_kernel_driver(libusb_device_handle*d,int i){(void)d;(void)i;return 0;}
int libusb_claim_interface(libusb_device_handle*d,int i){(void)d;(void)i;return 0;}
int libusb_release_interface(libusb_device_handle*d,int i){(void)d;(void)i;return 0;}
void libusb_close(libusb_device_handle*d){(void)d;}
void libusb_exit(libusb_context*c){(void)c;}

#include "../src/minibox-usb/scan_m1522.c"

static void setup(int rc,int done,int n)
{ int i; calls=0; seq_len=n; for(i=0;i<n;i++){seq_rc[i]=rc;seq_done[i]=done;} }
int main(void){
 struct m1522_scan_handle h={0}; unsigned char b[8]; size_t got=99; int rc;
 h.dev=(libusb_device_handle*)1; h.bulk_in=0x83;
 setup(0,0,5); rc=m1522_scan_read(&h,b,sizeof b,&got,1); assert(rc==LIBUSB_ERROR_TIMEOUT&&got==0&&calls==5);
 setup(LIBUSB_ERROR_TIMEOUT,0,4); rc=m1522_scan_read(&h,b,sizeof b,&got,1); assert(rc==LIBUSB_ERROR_TIMEOUT&&got==0&&calls==4);
 setup(LIBUSB_ERROR_IO,0,4); rc=m1522_scan_read(&h,b,sizeof b,&got,1); assert(rc==LIBUSB_ERROR_IO&&got==0&&calls==4);
 calls=0; seq_len=3; seq_rc[0]=0;seq_done[0]=0; seq_rc[1]=LIBUSB_ERROR_TIMEOUT;seq_done[1]=0; seq_rc[2]=0;seq_done[2]=3;
 rc=m1522_scan_read(&h,b,sizeof b,&got,1); assert(rc==0&&got==3&&calls==3);
 puts("verified M1522 USB retry semantics: OK");
 return 0;
}
