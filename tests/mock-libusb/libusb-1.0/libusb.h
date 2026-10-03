#ifndef LIBUSB_H
#define LIBUSB_H
#include <stddef.h>
typedef struct libusb_context libusb_context;
typedef struct libusb_device_handle libusb_device_handle;
typedef struct libusb_device libusb_device;
struct libusb_endpoint_descriptor { unsigned char bEndpointAddress,bmAttributes; };
struct libusb_interface_descriptor { unsigned char bInterfaceNumber,bInterfaceClass,bInterfaceSubClass,bInterfaceProtocol,bNumEndpoints; const struct libusb_endpoint_descriptor *endpoint; };
struct libusb_interface { int num_altsetting; const struct libusb_interface_descriptor *altsetting; };
struct libusb_config_descriptor { unsigned char bNumInterfaces; const struct libusb_interface *interface; };
#define LIBUSB_ERROR_IO (-1)
#define LIBUSB_ERROR_TIMEOUT (-7)
#define LIBUSB_ERROR_NOT_SUPPORTED (-12)
#define LIBUSB_TRANSFER_TYPE_MASK 3
#define LIBUSB_TRANSFER_TYPE_BULK 2
#define LIBUSB_ENDPOINT_IN 0x80
int libusb_bulk_transfer(libusb_device_handle*,unsigned char,unsigned char*,int,int*,unsigned int);
int libusb_get_active_config_descriptor(libusb_device*,struct libusb_config_descriptor**);
void libusb_free_config_descriptor(struct libusb_config_descriptor*);
int libusb_init(libusb_context**);
libusb_device_handle *libusb_open_device_with_vid_pid(libusb_context*,unsigned short,unsigned short);
libusb_device *libusb_get_device(libusb_device_handle*);
int libusb_kernel_driver_active(libusb_device_handle*,int);
int libusb_detach_kernel_driver(libusb_device_handle*,int);
int libusb_claim_interface(libusb_device_handle*,int);
int libusb_release_interface(libusb_device_handle*,int);
void libusb_close(libusb_device_handle*);
void libusb_exit(libusb_context*);
#endif
