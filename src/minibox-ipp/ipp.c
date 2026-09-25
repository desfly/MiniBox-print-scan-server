#include "ipp.h"
#include <string.h>
int ipp_parse_header(const unsigned char*b,size_t n,struct ipp_request*r){if(!b||!r||n<8)return-1;r->major=b[0];r->minor=b[1];r->operation=(uint16_t)(((uint16_t)b[2]<<8)|b[3]);r->request_id=((uint32_t)b[4]<<24)|((uint32_t)b[5]<<16)|((uint32_t)b[6]<<8)|b[7];if(r->major!=1&&r->major!=2)return-2;return 0;}
int ipp_document_offset(const unsigned char*b,size_t n,size_t*off){size_t p=8;if(!b||!off||n<9)return-1;while(p<n){unsigned char tag=b[p++];if(tag==0x03){*off=p;return 0;}if(tag>=0x01&&tag<=0x05)continue;if(p+2>n)return-2;{size_t nl=((size_t)b[p]<<8)|b[p+1];p+=2;if(nl>n-p||n-p-nl<2)return-2;p+=nl;{size_t vl=((size_t)b[p]<<8)|b[p+1];p+=2;if(vl>n-p)return-2;p+=vl;}}}return-3;}

/* Identify the document data that follows the IPP attribute section.
 * Raw PCL remains supported for the proven Windows path. PWG Raster is
 * converted by printerd to bounded monochrome PCL5 before USB. */
int ipp_document_format_kind(const unsigned char *buf,size_t len) {
    static const char key[]="document-format";
    static const char raw[]="application/octet-stream";
    static const char pwg[]="image/pwg-raster";
    size_t p=8;int seen=0,kind=IPP_DOCUMENT_RAW;
    if(!buf||len<9)return IPP_DOCUMENT_MALFORMED;
    while(p<len) {
        unsigned char tag=buf[p++];size_t nl,vl;
        if(tag==0x03)return kind;
        if(tag>=0x01&&tag<=0x05)continue;
        if(p>len||len-p<2)return IPP_DOCUMENT_MALFORMED;
        nl=((size_t)buf[p]<<8)|buf[p+1];p+=2;
        if(nl>len-p||len-p-nl<2)return IPP_DOCUMENT_MALFORMED;
        if(nl==sizeof(key)-1&&!memcmp(buf+p,key,nl)) {
            if(seen++||tag!=0x49)return IPP_DOCUMENT_UNSUPPORTED;
            p+=nl;vl=((size_t)buf[p]<<8)|buf[p+1];p+=2;
            if(vl>len-p)return IPP_DOCUMENT_MALFORMED;
            if(vl==sizeof(raw)-1&&!memcmp(buf+p,raw,vl))kind=IPP_DOCUMENT_RAW;
            else if(vl==sizeof(pwg)-1&&!memcmp(buf+p,pwg,vl))kind=IPP_DOCUMENT_PWG_RASTER;
            else return IPP_DOCUMENT_UNSUPPORTED;
        } else {
            p+=nl;vl=((size_t)buf[p]<<8)|buf[p+1];p+=2;
            if(vl>len-p)return IPP_DOCUMENT_MALFORMED;
        }
        p+=vl;
    }
    return IPP_DOCUMENT_MALFORMED;
}
int ipp_check_document_format(const unsigned char *buf,size_t len) {
    int k=ipp_document_format_kind(buf,len);
    return k==IPP_DOCUMENT_MALFORMED?-1:k==IPP_DOCUMENT_UNSUPPORTED?1:0;
}

const char*ipp_operation_name(uint16_t op){switch(op){case IPP_OP_PRINT_JOB:return"Print-Job";case IPP_OP_VALIDATE_JOB:return"Validate-Job";case IPP_OP_GET_PRINTER_ATTRIBUTES:return"Get-Printer-Attributes";default:return"Unknown";}}

static int put(unsigned char*o,size_t c,size_t*p,const void*v,size_t n){if(*p>c||n>c-*p)return-1;memcpy(o+*p,v,n);*p+=n;return 0;}
static int u16(unsigned char*o,size_t c,size_t*p,unsigned v){unsigned char b[2]={(unsigned char)(v>>8),(unsigned char)v};return put(o,c,p,b,2);}
static int attr(unsigned char*o,size_t c,size_t*p,unsigned tag,const char*n,const void*v,size_t z){size_t nl=strlen(n);unsigned char t=(unsigned char)tag;if(nl>65535||z>65535||put(o,c,p,&t,1)||u16(o,c,p,(unsigned)nl)||put(o,c,p,n,nl)||u16(o,c,p,(unsigned)z)||put(o,c,p,v,z))return-1;return 0;}
static int attr_more(unsigned char*o,size_t c,size_t*p,unsigned tag,const void*v,size_t z){unsigned char t=(unsigned char)tag;if(z>65535||put(o,c,p,&t,1)||u16(o,c,p,0)||u16(o,c,p,(unsigned)z)||put(o,c,p,v,z))return-1;return 0;}
static int attr_resolution(unsigned char*o,size_t c,size_t*p,const char*n,unsigned x,unsigned y){
    unsigned char v[9]={(unsigned char)(x>>24),(unsigned char)(x>>16),(unsigned char)(x>>8),(unsigned char)x,
                        (unsigned char)(y>>24),(unsigned char)(y>>16),(unsigned char)(y>>8),(unsigned char)y,3};
    return attr(o,c,p,0x32,n,v,sizeof v);
}
int ipp_raw_format_supported(const unsigned char *b,size_t n){int r=ipp_check_document_format(b,n);return r<0?-1:!r;}


/* RFC 8011 section 4.1.4: every IPP response starts with an Operation
 * Attributes group, charset first and natural-language second. */
size_t ipp_build_status(unsigned char*o,size_t c,const struct ipp_request*r,uint16_t status){
    size_t p=0;
    const unsigned char group=0x01,end=0x03;
    unsigned char req_id[4];
    if(!o||!r||c<9)return 0;
    o[p++]=r->major;o[p++]=r->minor;
    req_id[0]=(unsigned char)(r->request_id>>24);
    req_id[1]=(unsigned char)(r->request_id>>16);
    req_id[2]=(unsigned char)(r->request_id>>8);
    req_id[3]=(unsigned char)r->request_id;
    if(u16(o,c,&p,status)||put(o,c,&p,req_id,sizeof req_id)||
       put(o,c,&p,&group,1)||
       attr(o,c,&p,0x47,"attributes-charset","utf-8",5)||
       attr(o,c,&p,0x48,"attributes-natural-language","en",2)||
       put(o,c,&p,&end,1))return 0;
    return p;
}

size_t ipp_build_printer_attributes(unsigned char*o,size_t c,
                                    const struct ipp_request*r,
                                    const char*uri,const char*printer_uuid){
    size_t p=0;
    const unsigned char group=0x04,end=0x03;
    const unsigned char state[4]={0,0,0,3};
    const unsigned char accepting=1;
    const unsigned char color=0;
    const unsigned char copies_one[8]={0,0,0,1,0,0,0,1};
    const unsigned char op_print[4]={0,0,0,IPP_OP_PRINT_JOB};
    const unsigned char op_validate[4]={0,0,0,IPP_OP_VALIDATE_JOB};
    const unsigned char op_attrs[4]={0,0,0,IPP_OP_GET_PRINTER_ATTRIBUTES};
    const char *printer_name="HP LaserJet M1522n @ MiniBox";
    const char *model="HP LaserJet M1522n";
    const char *info="MiniBox network print server";
    const char *format="application/octet-stream";
    const char *pwg="image/pwg-raster";
    if(!o||!r||!uri||!printer_uuid||!printer_uuid[0]||c<9)return 0;
    if(!(p=ipp_build_status(o,c,r,0)))return 0;
    p--; /* Replace the empty response's end-of-attributes tag with an attributes group. */
    if(put(o,c,&p,&group,1))return 0;
    /* RFC 8011: nameWithoutLanguage=0x42, textWithoutLanguage=0x41,
       enum=0x23. A URI tag (0x45) cannot describe a printer name. */
    if(attr(o,c,&p,0x42,"printer-name",printer_name,strlen(printer_name)))return 0;
    if(attr(o,c,&p,0x41,"printer-make-and-model",model,strlen(model)))return 0;
    if(attr(o,c,&p,0x41,"printer-info",info,strlen(info)))return 0;
    if(attr(o,c,&p,0x45,"printer-uri-supported",uri,strlen(uri)))return 0;
    if(attr(o,c,&p,0x45,"printer-uuid",printer_uuid,strlen(printer_uuid)))return 0;
    /* These REQUIRED companion values correspond to this one non-TLS,
     * unauthenticated IPP URI. Never claim TLS or authentication here. */
    if(attr(o,c,&p,0x44,"uri-authentication-supported","none",4))return 0;
    if(attr(o,c,&p,0x44,"uri-security-supported","none",4))return 0;
    if(attr(o,c,&p,0x23,"printer-state",state,4))return 0;
    if(attr(o,c,&p,0x22,"printer-is-accepting-jobs",&accepting,1))return 0;
    if(attr(o,c,&p,0x44,"printer-state-reasons","none",4))return 0;
    if(attr(o,c,&p,0x47,"charset-configured","utf-8",5))return 0;
    if(attr(o,c,&p,0x47,"charset-supported","utf-8",5))return 0;
    if(attr(o,c,&p,0x48,"natural-language-configured","en",2))return 0;
    if(attr(o,c,&p,0x48,"generated-natural-language-supported","en",2))return 0;
    /* The wire parser and these basic operations accept both IPP/1.1 and
     * IPP/2.0 requests. This does not by itself claim IPP Everywhere
     * certification or any unsupported operation. */
    if(attr(o,c,&p,0x44,"ipp-versions-supported","1.1",3))return 0;
    if(attr_more(o,c,&p,0x44,"2.0",3))return 0;
    if(attr(o,c,&p,0x23,"operations-supported",op_print,4))return 0;
    if(attr_more(o,c,&p,0x23,op_validate,4))return 0;
    if(attr_more(o,c,&p,0x23,op_attrs,4))return 0;
    if(attr(o,c,&p,0x49,"document-format-supported",format,strlen(format)))return 0;
    if(attr_more(o,c,&p,0x49,pwg,strlen(pwg)))return 0;
    if(attr(o,c,&p,0x49,"document-format-default",format,strlen(format)))return 0;
    if(attr(o,c,&p,0x22,"color-supported",&color,1))return 0;
    if(attr(o,c,&p,0x44,"print-color-mode-supported","monochrome",10))return 0;
    if(attr(o,c,&p,0x44,"print-color-mode-default","monochrome",10))return 0;
    if(attr(o,c,&p,0x44,"sides-supported","one-sided",9))return 0;
    if(attr(o,c,&p,0x44,"sides-default","one-sided",9))return 0;
    if(attr(o,c,&p,0x33,"copies-supported",copies_one,sizeof copies_one))return 0;
    if(attr(o,c,&p,0x44,"media-supported","iso_a4_210x297mm",16))return 0;
    if(attr_more(o,c,&p,0x44,"na_letter_8.5x11in",18))return 0;
    if(attr_more(o,c,&p,0x44,"na_legal_8.5x14in",17))return 0;
    if(attr(o,c,&p,0x44,"media-default","iso_a4_210x297mm",16))return 0;
    if(attr_resolution(o,c,&p,"printer-resolution-supported",300,300))return 0;
    if(attr_resolution(o,c,&p,"printer-resolution-default",300,300))return 0;
    if(attr_resolution(o,c,&p,"pwg-raster-document-resolution-supported",300,300))return 0;
    if(attr(o,c,&p,0x44,"pwg-raster-document-type-supported","black_1",7))return 0;
    if(attr_more(o,c,&p,0x44,"sgray_8",7))return 0;
    if(attr_more(o,c,&p,0x44,"srgb_8",6))return 0;
    /* All print requests are processed synchronously; there is no
     * persistent asynchronous queue in this implementation. */
    {
        const unsigned char queued[4]={0,0,0,0};
        if(attr(o,c,&p,0x21,"queued-job-count",queued,4))return 0;
    }
    if(put(o,c,&p,&end,1))return 0;
    return p;
}

size_t ipp_build_print_job_response(unsigned char *o,size_t cap,
                                    const struct ipp_request *r,
                                    uint32_t job_id,const char *job_uri){
    size_t p;
    const unsigned char group=0x02,end=0x03;
    unsigned char id[4],state[4]={0,0,0,9}; /* completed */
    if(!o||!r||!job_id||!job_uri||!job_uri[0])return 0;
    if(!(p=ipp_build_status(o,cap,r,0)))return 0;
    p--; /* Replace end-of-attributes with the required Job Attributes group. */
    id[0]=(unsigned char)(job_id>>24);
    id[1]=(unsigned char)(job_id>>16);
    id[2]=(unsigned char)(job_id>>8);
    id[3]=(unsigned char)job_id;
    if(put(o,cap,&p,&group,1)||
       attr(o,cap,&p,0x45,"job-uri",job_uri,strlen(job_uri))||
       attr(o,cap,&p,0x21,"job-id",id,sizeof id)||
       attr(o,cap,&p,0x23,"job-state",state,sizeof state)||
       attr(o,cap,&p,0x44,"job-state-reasons","none",4)||
       put(o,cap,&p,&end,1))return 0;
    return p;
}
