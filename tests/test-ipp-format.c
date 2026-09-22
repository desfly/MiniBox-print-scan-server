#include "../src/minibox-ipp/ipp.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t make(unsigned char *out,const char *mime,unsigned char tag,int duplicate){
    size_t p=0,k=strlen("document-format"),v=strlen(mime);
    const unsigned char header[]={2,0,0,2,0,0,0,42,1};
    memcpy(out+p,header,sizeof header);p+=sizeof header;
    do {
        out[p++]=tag;out[p++]=0;out[p++]=(unsigned char)k;
        memcpy(out+p,"document-format",k);p+=k;
        out[p++]=(unsigned char)(v>>8);out[p++]=(unsigned char)v;
        memcpy(out+p,mime,v);p+=v;
    } while(duplicate-- >0);
    out[p++]=3;
    return p;
}
int main(void){
    unsigned char req[512];size_t n;
    n=make(req,"application/octet-stream",0x49,0);
    assert(ipp_check_document_format(req,n)==0);
    assert(ipp_check_document_format(req,n-1)<0);
    n=make(req,"application/pdf",0x49,0);
    assert(ipp_check_document_format(req,n)==1);
    n=make(req,"image/pwg-raster",0x49,0);
    assert(ipp_check_document_format(req,n)==1);
    n=make(req,"application/octet-stream",0x49,1);
    assert(ipp_check_document_format(req,n)==1);
    n=make(req,"application/octet-stream",0x44,0);
    assert(ipp_check_document_format(req,n)==1);
    n=make(req,"application/octet-stream",0x49,0);
    assert(ipp_check_document_format(req,8)<0);
    req[9]=0x49;req[10]=255;req[11]=255;
    assert(ipp_check_document_format(req,n)<0);
    puts("IPP format gate OK");
    return 0;
}
