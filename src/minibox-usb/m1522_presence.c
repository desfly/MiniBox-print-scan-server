#define _POSIX_C_SOURCE 200809L
#include "m1522_presence.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_id(const char *path,char *out,size_t cap){
    FILE *f=fopen(path,"r");size_t n;
    if(!f)return 0;
    if(!fgets(out,(int)cap,f)){fclose(f);return 0;}
    fclose(f);
    n=strcspn(out,"\r\n");out[n]=0;
    return 1;
}

int minibox_m1522_present(void){
    const char *root=getenv("MINIBOX_USB_SYSFS_ROOT");
    DIR *d;struct dirent *de;
    if(!root||!*root)root="/sys/bus/usb/devices";
    d=opendir(root);if(!d)return 0;
    while((de=readdir(d))!=NULL){
        char vp[512],pp[512],v[32],p[32];
        if(de->d_name[0]=='.')continue;
        if(snprintf(vp,sizeof vp,"%s/%s/idVendor",root,de->d_name)>=(int)sizeof vp)continue;
        if(snprintf(pp,sizeof pp,"%s/%s/idProduct",root,de->d_name)>=(int)sizeof pp)continue;
        if(!read_id(vp,v,sizeof v)||!read_id(pp,p,sizeof p))continue;
        if(!strcasecmp(v,"03f0")&&!strcasecmp(p,"4517")){closedir(d);return 1;}
    }
    closedir(d);return 0;
}
