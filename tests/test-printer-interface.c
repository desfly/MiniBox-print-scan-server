#include "../src/minibox-usb/libusb_m1522.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 assert(m1522_printer_interface_rank(0xff,0x02,0x01,0x02)==0);
 assert(m1522_printer_interface_rank(0x07,0x01,0x03,0x02)==0);
 assert(m1522_printer_interface_rank(0x07,0x01,0x02,0x00)==0);
 assert(m1522_printer_interface_rank(0x07,0x01,0x01,0x02)==1);
 assert(m1522_printer_interface_rank(0x07,0x01,0x02,0x02)==2);
 puts("printer interface policy: OK");
 return 0;
}
