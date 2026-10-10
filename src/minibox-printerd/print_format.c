#include "print_format.h"
#include <string.h>

static int looks_pwg(const unsigned char *p,size_t n){
    return p&&n>=4&&!memcmp(p,"RaS2",4);
}
static int looks_printer_language(const unsigned char *p,size_t n){
    static const unsigned char uel[]={0x1b,'%', '-', '1','2','3','4','5','X'};
    if(!p||!n)return 0;
    if(n>=sizeof uel&&!memcmp(p,uel,sizeof uel))return 1;
    /* PCL reset is a valid legacy PCL start. */
    if(n>=2&&p[0]==0x1b&&p[1]=='E')return 1;
    /* Some captured/test streams start directly at the PCL XL binding line. */
    if(n>=12&&!memcmp(p,") HP-PCL XL;",12))return 1;
    return 0;
}

int mb_print_resolve_format(int declared_kind,int inherited_kind,
                            const unsigned char *payload,size_t payload_len){
    int effective=declared_kind;

    /* Payload signature wins over a missing/mislabelled attribute.  This
     * prevents RaS2 data from ever being passed raw to the USB printer. */
    if(looks_pwg(payload,payload_len))return MB_PRINT_INPUT_PWG;

    if(effective==IPP_DOCUMENT_UNSPECIFIED)effective=inherited_kind;
    if(effective==IPP_DOCUMENT_UNSPECIFIED)effective=IPP_DOCUMENT_PWG_RASTER;

    if(effective==IPP_DOCUMENT_PWG_RASTER)return MB_PRINT_INPUT_PWG;
    if(effective==IPP_DOCUMENT_RAW&&looks_printer_language(payload,payload_len))
        return MB_PRINT_INPUT_RAW;
    return MB_PRINT_INPUT_REJECT;
}
