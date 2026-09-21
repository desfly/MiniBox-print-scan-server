#include "ipp.h"
#include <string.h>
int ipp_parse_header(const unsigned char*b,size_t n,struct ipp_request*r){if(!b||!r||n<8)return-1;r->major=b[0];r->minor=b[1];r->operation=(uint16_t)(((uint16_t)b[2]<<8)|b[3]);r->request_id=((uint32_t)b[4]<<24)|((uint32_t)b[5]<<16)|((uint32_t)b[6]<<8)|b[7];if(r->major!=1&&r->major!=2)return-2;return 0;}
int ipp_document_offset(const unsigned char*b,size_t n,size_t*off){size_t p=8;if(!b||!off||n<9)return-1;while(p<n){unsigned char tag=b[p++];if(tag==0x03){*off=p;return 0;}if(tag>=0x01&&tag<=0x05)continue;if(p+2>n)return-2;{size_t nl=((size_t)b[p]<<8)|b[p+1];p+=2;if(p+nl+2>n)return-2;p+=nl;{size_t vl=((size_t)b[p]<<8)|b[p+1];p+=2;if(p+vl>n)return-2;p+=vl;}}}return-3;}
const char*ipp_operation_name(uint16_t op){switch(op){case IPP_OP_PRINT_JOB:return"Print-Job";case IPP_OP_VALIDATE_JOB:return"Validate-Job";case IPP_OP_GET_PRINTER_ATTRIBUTES:return"Get-Printer-Attributes";default:return"Unknown";}}
size_t ipp_build_status(unsigned char*o,size_t c,const struct ipp_request*r,uint16_t s){if(!o||!r||c<9)return 0;o[0]=r->major;o[1]=r->minor;o[2]=(unsigned char)(s>>8);o[3]=(unsigned char)s;o[4]=(unsigned char)(r->request_id>>24);o[5]=(unsigned char)(r->request_id>>16);o[6]=(unsigned char)(r->request_id>>8);o[7]=(unsigned char)r->request_id;o[8]=0x03;return 9;}
static int put(unsigned char*o,size_t c,size_t*p,const void*v,size_t n){if(*p+n>c)return-1;memcpy(o+*p,v,n);*p+=n;return 0;}
static int u16(unsigned char*o,size_t c,size_t*p,unsigned v){unsigned char b[2]={(unsigned char)(v>>8),(unsigned char)v};return put(o,c,p,b,2);}
static int attr(unsigned char*o,size_t c,size_t*p,unsigned tag,const char*n,const void*v,size_t z){size_t nl=strlen(n);unsigned char t=(unsigned char)tag;if(nl>65535||z>65535||put(o,c,p,&t,1)||u16(o,c,p,(unsigned)nl)||put(o,c,p,n,nl)||u16(o,c,p,(unsigned)z)||put(o,c,p,v,z))return-1;return 0;}
static int attr_more(unsigned char*o,size_t c,size_t*p,unsigned tag,const void*v,size_t z){unsigned char t=(unsigned char)tag;if(z>65535||put(o,c,p,&t,1)||u16(o,c,p,0)||u16(o,c,p,(unsigned)z)||put(o,c,p,v,z))return-1;return 0;}
size_t ipp_build_printer_attributes(unsigned char*o,size_t c,const struct ipp_request*r,const char*uri){
    size_t p=0;
    const unsigned char group=0x04,end=0x03;
    const unsigned char state[4]={0,0,0,3};
    const unsigned char accepting=1;
    const unsigned char op_print[4]={0,0,0,IPP_OP_PRINT_JOB};
    const unsigned char op_validate[4]={0,0,0,IPP_OP_VALIDATE_JOB};
    const unsigned char op_attrs[4]={0,0,0,IPP_OP_GET_PRINTER_ATTRIBUTES};
    const char *printer_name="HP LaserJet M1522n @ MiniBox";
    const char *model="HP LaserJet M1522n";
    const char *info="MiniBox network print server";
    const char *format="application/octet-stream";
    if(!o||!r||!uri||c<9)return 0;
    if(!(p=ipp_build_status(o,c,r,0)))return 0;
    p--; /* Replace the empty response's end-of-attributes tag with an attributes group. */
    if(put(o,c,&p,&group,1))return 0;
    /* RFC 8011: nameWithoutLanguage=0x42, textWithoutLanguage=0x41,
       enum=0x23. A URI tag (0x45) cannot describe a printer name. */
    if(attr(o,c,&p,0x42,"printer-name",printer_name,strlen(printer_name)))return 0;
    if(attr(o,c,&p,0x41,"printer-make-and-model",model,strlen(model)))return 0;
    if(attr(o,c,&p,0x41,"printer-info",info,strlen(info)))return 0;
    if(attr(o,c,&p,0x45,"printer-uri-supported",uri,strlen(uri)))return 0;
    if(attr(o,c,&p,0x23,"printer-state",state,4))return 0;
    if(attr(o,c,&p,0x22,"printer-is-accepting-jobs",&accepting,1))return 0;
    if(attr(o,c,&p,0x44,"printer-state-reasons","none",4))return 0;
    if(attr(o,c,&p,0x47,"charset-configured","utf-8",5))return 0;
    if(attr(o,c,&p,0x48,"natural-language-configured","en",2))return 0;
    /* A limited IPP implementation must not claim a fully supported IPP 2.0 feature set. */
    if(attr(o,c,&p,0x44,"ipp-versions-supported","1.1",3))return 0;
    if(attr(o,c,&p,0x23,"operations-supported",op_print,4))return 0;
    if(attr_more(o,c,&p,0x23,op_validate,4))return 0;
    if(attr_more(o,c,&p,0x23,op_attrs,4))return 0;
    if(attr(o,c,&p,0x49,"document-format-supported",format,strlen(format)))return 0;
    if(attr(o,c,&p,0x49,"document-format-default",format,strlen(format)))return 0;
    if(put(o,c,&p,&end,1))return 0;
    return p;
}
