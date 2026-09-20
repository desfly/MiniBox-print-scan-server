#include "usb_backend.h"
#include <stdio.h>
int minibox_usb_match(unsigned vid,unsigned pid){return vid==MINIBOX_HP_VID&&pid==MINIBOX_M1522_PID;}
int minibox_usb_parse_lsusb(const char *line,struct minibox_usb_device*out){unsigned v,p;int b,a;if(!line||!out)return-1;if(sscanf(line,"Bus %d Device %d: ID %x:%x",&b,&a,&v,&p)!=4)return-1;out->vid=v;out->pid=p;out->bus=b;out->address=a;return minibox_usb_match(v,p)?1:0;}
