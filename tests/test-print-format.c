#include "../src/minibox-printerd/print_format.h"
#include <assert.h>
#include <stddef.h>

int main(void){
    static const unsigned char pwg[]={'R','a','S','2',0,0,0,0};
    static const unsigned char pcl[]={0x1b,'E','H','i'};
    static const unsigned char xl[]={0x1b,'%', '-', '1','2','3','4','5','X',')',' ','H','P','-','P','C','L',' ','X','L',';'};
    static const unsigned char junk[]={'N','O','T','-','P','R','I','N','T'};

    assert(mb_print_resolve_format(IPP_DOCUMENT_PWG_RASTER,IPP_DOCUMENT_UNSPECIFIED,pwg,sizeof pwg)==MB_PRINT_INPUT_PWG);
    assert(mb_print_resolve_format(IPP_DOCUMENT_UNSPECIFIED,IPP_DOCUMENT_UNSPECIFIED,pwg,sizeof pwg)==MB_PRINT_INPUT_PWG);
    assert(mb_print_resolve_format(IPP_DOCUMENT_RAW,IPP_DOCUMENT_UNSPECIFIED,pwg,sizeof pwg)==MB_PRINT_INPUT_PWG);

    assert(mb_print_resolve_format(IPP_DOCUMENT_UNSPECIFIED,IPP_DOCUMENT_PWG_RASTER,pwg,sizeof pwg)==MB_PRINT_INPUT_PWG);
    assert(mb_print_resolve_format(IPP_DOCUMENT_UNSPECIFIED,IPP_DOCUMENT_UNSPECIFIED,junk,sizeof junk)==MB_PRINT_INPUT_PWG);

    assert(mb_print_resolve_format(IPP_DOCUMENT_RAW,IPP_DOCUMENT_UNSPECIFIED,pcl,sizeof pcl)==MB_PRINT_INPUT_RAW);
    assert(mb_print_resolve_format(IPP_DOCUMENT_RAW,IPP_DOCUMENT_UNSPECIFIED,xl,sizeof xl)==MB_PRINT_INPUT_RAW);
    assert(mb_print_resolve_format(IPP_DOCUMENT_RAW,IPP_DOCUMENT_UNSPECIFIED,junk,sizeof junk)==MB_PRINT_INPUT_REJECT);

    return 0;
}
