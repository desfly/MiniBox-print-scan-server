#include "ipp.h"
#include "../minibox-identity/m1522_identity.h"
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

const char*ipp_operation_name(uint16_t op){switch(op){case IPP_OP_PRINT_JOB:return"Print-Job";case IPP_OP_VALIDATE_JOB:return"Validate-Job";case IPP_OP_CREATE_JOB:return"Create-Job";case IPP_OP_SEND_DOCUMENT:return"Send-Document";case IPP_OP_CANCEL_JOB:return"Cancel-Job";case IPP_OP_GET_JOB_ATTRIBUTES:return"Get-Job-Attributes";case IPP_OP_GET_JOBS:return"Get-Jobs";case IPP_OP_GET_PRINTER_ATTRIBUTES:return"Get-Printer-Attributes";default:return"Unknown";}}

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

size_t ipp_build_printer_attributes(unsigned char*o,size_t c,const struct ipp_request*r,const char*uri,int mfp_online){
    size_t p=0;
    const unsigned char group=0x04,end=0x03;
    unsigned char state[4]={0,0,0,3};
    unsigned char accepting=1;
    const char *state_reason="none";
    const unsigned char color=0;
    const unsigned char copies_one[8]={0,0,0,1,0,0,0,1};
    const unsigned char op_print[4]={0,0,0,IPP_OP_PRINT_JOB};
    const unsigned char op_validate[4]={0,0,0,IPP_OP_VALIDATE_JOB};
    const unsigned char op_create[4]={0,0,0,IPP_OP_CREATE_JOB};
    const unsigned char op_send[4]={0,0,0,IPP_OP_SEND_DOCUMENT};
    const unsigned char op_cancel[4]={0,0,0,IPP_OP_CANCEL_JOB};
    const unsigned char op_job_attrs[4]={0,0,0,IPP_OP_GET_JOB_ATTRIBUTES};
    const unsigned char op_jobs[4]={0,0,0,IPP_OP_GET_JOBS};
    const unsigned char op_attrs[4]={0,0,0,IPP_OP_GET_PRINTER_ATTRIBUTES};
    const char *printer_name="M1522n NET PRINTER";
    const char *model="HP LaserJet M1522n";
    const char *info="MiniBox network print server";
    const char *format="application/octet-stream";
    const char *pwg="image/pwg-raster";
    const char *printer_uuid=MINIBOX_MFP_URN_UUID;
    const char *device_id="MFG:HP;MDL:HP LaserJet M1522n MFP;CMD:PCLXL,PCL;CLS:PRINTER;";
    if(!o||!r||!uri||c<9)return 0;
    if(!mfp_online){state[3]=5;accepting=0;state_reason="offline";}
    if(!(p=ipp_build_status(o,c,r,0)))return 0;
    p--; /* Replace the empty response's end-of-attributes tag with an attributes group. */
    if(put(o,c,&p,&group,1))return 0;
    /* RFC 8011: nameWithoutLanguage=0x42, textWithoutLanguage=0x41,
       enum=0x23. A URI tag (0x45) cannot describe a printer name. */
    if(attr(o,c,&p,0x42,"printer-name",printer_name,strlen(printer_name)))return 0;
    if(attr(o,c,&p,0x41,"printer-make-and-model",model,strlen(model)))return 0;
    if(attr(o,c,&p,0x41,"printer-info",info,strlen(info)))return 0;
    /* Keep the IPP identity aligned with the UUID advertised over DNS-SD.
     * Windows uses printer-device-id as the hardware identity when it
     * associates an IPP queue with a model-specific print driver. */
    if(attr(o,c,&p,0x45,"printer-uuid",printer_uuid,strlen(printer_uuid)))return 0;
    if(attr(o,c,&p,0x41,"printer-device-id",device_id,strlen(device_id)))return 0;
    if(attr(o,c,&p,0x45,"printer-uri-supported",uri,strlen(uri)))return 0;
    /* These REQUIRED companion values correspond to this one non-TLS,
     * unauthenticated IPP URI. Never claim TLS or authentication here. */
    if(attr(o,c,&p,0x44,"uri-authentication-supported","none",4))return 0;
    if(attr(o,c,&p,0x44,"uri-security-supported","none",4))return 0;
    if(attr(o,c,&p,0x23,"printer-state",state,4))return 0;
    if(attr(o,c,&p,0x22,"printer-is-accepting-jobs",&accepting,1))return 0;
    if(attr(o,c,&p,0x44,"printer-state-reasons",state_reason,strlen(state_reason)))return 0;
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
    if(attr_more(o,c,&p,0x23,op_create,4))return 0;
    if(attr_more(o,c,&p,0x23,op_send,4))return 0;
    if(attr_more(o,c,&p,0x23,op_cancel,4))return 0;
    if(attr_more(o,c,&p,0x23,op_job_attrs,4))return 0;
    if(attr_more(o,c,&p,0x23,op_jobs,4))return 0;
    if(attr_more(o,c,&p,0x23,op_attrs,4))return 0;
    if(attr(o,c,&p,0x44,"job-creation-attributes-supported","copies",6))return 0;
    if(attr_more(o,c,&p,0x44,"media",5))return 0;
    if(attr_more(o,c,&p,0x44,"printer-resolution",18))return 0;
    if(attr_more(o,c,&p,0x44,"sides",5))return 0;
    if(attr_more(o,c,&p,0x44,"print-color-mode",16))return 0;
    if(attr(o,c,&p,0x49,"document-format-supported",format,strlen(format)))return 0;
    if(attr_more(o,c,&p,0x49,pwg,strlen(pwg)))return 0;
    if(attr(o,c,&p,0x49,"document-format-default",pwg,strlen(pwg)))return 0;
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
    if(attr_resolution(o,c,&p,"printer-resolution-supported",600,600))return 0;
    if(attr_resolution(o,c,&p,"printer-resolution-default",600,600))return 0;
    if(attr_resolution(o,c,&p,"pwg-raster-document-resolution-supported",600,600))return 0;
    /* Do not advertise black_1 to Windows IPP Class Driver: capture pwg600.pcap
     * proves it then selects a 1-bit sGray raster, destroying grayscale before
     * MiniBox can hand tonal data to the M1522.  Advertise the continuous-tone
     * monochrome format only. */
    if(attr(o,c,&p,0x44,"pwg-raster-document-type-supported","sgray_8",7))return 0;
    /* All print requests are processed synchronously; there is no
     * persistent asynchronous queue in this implementation. */
    {
        const unsigned char queued[4]={0,0,0,0};
        if(attr(o,c,&p,0x21,"queued-job-count",queued,4))return 0;
    }
    if(put(o,c,&p,&end,1))return 0;
    return p;
}

size_t ipp_build_job_response(unsigned char *o,size_t cap,
                              const struct ipp_request *r,
                              uint32_t job_id,const char *job_uri,
                              uint32_t job_state){
    size_t p;
    const unsigned char group=0x02,end=0x03;
    unsigned char id[4],state[4]={(unsigned char)(job_state>>24),(unsigned char)(job_state>>16),(unsigned char)(job_state>>8),(unsigned char)job_state};
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

/* RFC 8011 requested-attributes: a collection of keyword values in the
 * operation group. No value, or "all", retains the complete response.
 * Filter only the printer-attributes group; mandatory operation attributes
 * are always preserved. Reject malformed attribute framing. */
size_t ipp_filter_requested_attributes(unsigned char *out,size_t cap,
    const unsigned char *full,size_t full_len,
    const unsigned char *req,size_t req_len){
    size_t p=8,q=0,group=0,selected_count=0;
    char selected[64][96];
    int all=0,seen=0;
    if(!out||!full||!req||full_len<10||req_len<9||cap<full_len)return 0;
    while(p<req_len){
        unsigned tag=req[p++];size_t nl,vl;
        if(tag==3)break;
        if(tag>=1&&tag<=5){group=tag;continue;}
        if(req_len-p<2)return 0;
        nl=((size_t)req[p]<<8)|req[p+1];p+=2;
        if(nl>req_len-p||req_len-p-nl<2)return 0;
        const unsigned char *name=req+p;p+=nl;
        vl=((size_t)req[p]<<8)|req[p+1];p+=2;
        if(vl>req_len-p)return 0;
        if(group==1&&tag==0x44&&nl==20&&!memcmp(name,"requested-attributes",20)){
            seen=1;
            if(vl==3&&!memcmp(req+p,"all",3))all=1;
            else if(vl&&vl<sizeof selected[0]&&selected_count<64){
                memcpy(selected[selected_count],req+p,vl);
                selected[selected_count][vl]=0;
                selected_count++;
            }else if(!vl||vl>=sizeof selected[0]||selected_count==64)return 0;
        }
        p+=vl;
    }
    if(!seen||all){memcpy(out,full,full_len);return full_len;}
    memcpy(out,full,8);q=8;p=8;group=0;
    int keep_more=0;
    while(p<full_len){
        size_t start=p;unsigned tag=full[p++],nl,vl;int keep=0;
        if(tag==3){out[q++]=3;return q;}
        if(tag>=1&&tag<=5){group=tag;out[q++]=(unsigned char)tag;keep_more=0;continue;}
        if(full_len-p<2)return 0;
        nl=((size_t)full[p]<<8)|full[p+1];p+=2;
        if(nl>full_len-p||full_len-p-nl<2)return 0;
        const unsigned char *name=full+p;p+=nl;
        vl=((size_t)full[p]<<8)|full[p+1];p+=2;
        if(vl>full_len-p)return 0;p+=vl;
        if(group==1)keep=1;
        else if(group==4){
            if(nl){
                keep=0;
                for(size_t i=0;i<selected_count;i++){
                    size_t z=strlen(selected[i]);
                    if(z==nl&&!memcmp(selected[i],name,nl)){keep=1;break;}
                    if(!strcmp(selected[i],"printer-description")||!strcmp(selected[i],"printer-defaults")||!strcmp(selected[i],"printer-state")||!strcmp(selected[i],"printer-config")||!strcmp(selected[i],"document-template")||!strcmp(selected[i],"job-template")){keep=1;break;}
                }
                keep_more=keep;
            }else keep=keep_more;
        }
        if(keep){memcpy(out+q,full+start,p-start);q+=p-start;}
    }
    return 0;
}
