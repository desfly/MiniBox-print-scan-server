#include "escl.h"
#include <string.h>
#include <stdlib.h>
static int has(const char*s,const char*x){return strstr(s,x)!=0;}
int escl_parse_scan_settings(const char*x,size_t n,struct escl_job*j){if(!x||!j||!n)return-1;char *b=malloc(n+1);if(!b)return-2;memcpy(b,x,n);b[n]=0;j->source=has(b,"Adf")||has(b,"ADF")?ESCL_SOURCE_ADF:ESCL_SOURCE_PLATEN;j->color=has(b,"RGB24")||has(b,"Color");j->dpi=300;const char*p=strstr(b,"<scan:XResolution>");if(p){p+=strlen("<scan:XResolution>");unsigned v=(unsigned)strtoul(p,0,10);if(v>=75&&v<=1200)j->dpi=v;}free(b);return 0;}
