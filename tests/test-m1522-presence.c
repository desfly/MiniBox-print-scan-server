#define _POSIX_C_SOURCE 200809L
#include "../src/minibox-usb/m1522_presence.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_text(const char *path,const char *text){FILE *f=fopen(path,"w");assert(f);fputs(text,f);fclose(f);}
int main(void){
    char root[]="/tmp/minibox-usb-XXXXXX",dev[256],p[256];
    assert(mkdtemp(root));
    assert(setenv("MINIBOX_USB_SYSFS_ROOT",root,1)==0);
    assert(minibox_m1522_present()==0);
    snprintf(dev,sizeof dev,"%s/1-1",root);assert(mkdir(dev,0700)==0);
    snprintf(p,sizeof p,"%s/idVendor",dev);write_text(p,"03f0\n");
    snprintf(p,sizeof p,"%s/idProduct",dev);write_text(p,"4517\n");
    assert(minibox_m1522_present()==1);
    snprintf(p,sizeof p,"%s/idProduct",dev);write_text(p,"0001\n");
    assert(minibox_m1522_present()==0);
    return 0;
}
