#include "../src/minibox-escl/escl.h"
#include <assert.h>
#include <string.h>

int main(void){
    const char *rgb="<scan:ScanSettings><scan:InputSource>Adf</scan:InputSource><scan:XResolution>300</scan:XResolution><scan:YResolution>300</scan:YResolution><scan:ColorMode>RGB24</scan:ColorMode></scan:ScanSettings>";
    const char *gray="<scan:ScanSettings><scan:InputSource>Platen</scan:InputSource><scan:XResolution>300</scan:XResolution><scan:ColorMode>Grayscale8</scan:ColorMode></scan:ScanSettings>";
    const char *bad_dpi="<scan:ScanSettings><scan:XResolution>600</scan:XResolution><scan:ColorMode>RGB24</scan:ColorMode></scan:ScanSettings>";
    const char *bad_color="<scan:ScanSettings><scan:XResolution>300</scan:XResolution><scan:ColorMode>BlackAndWhite1</scan:ColorMode></scan:ScanSettings>";
    struct escl_job j;const char *caps,*status;

    assert(escl_parse_scan_settings(rgb,strlen(rgb),&j)==0);
    assert(j.source==ESCL_SOURCE_ADF&&j.dpi==300&&j.color==1);
    assert(escl_parse_scan_settings(gray,strlen(gray),&j)==0);
    assert(j.source==ESCL_SOURCE_PLATEN&&j.dpi==300&&j.color==0);
    assert(escl_parse_scan_settings(bad_dpi,strlen(bad_dpi),&j)<0);
    assert(escl_parse_scan_settings(bad_color,strlen(bad_color),&j)<0);

    caps=escl_scanner_capabilities_xml();
    assert(strstr(caps,"xmlns:pwg=\"http://www.pwg.org/schemas/2010/12/sm\""));
    assert(strstr(caps,"<pwg:Version>2.63</pwg:Version>"));
    assert(strstr(caps,"<pwg:MakeAndModel>HP LaserJet M1522n @ MiniBox</pwg:MakeAndModel>"));
    assert(strstr(caps,"<scan:PlatenInputCaps>"));
    assert(strstr(caps,"<scan:AdfSimplexInputCaps>"));
    assert(strstr(caps,"<scan:ColorMode>RGB24</scan:ColorMode>"));
    assert(strstr(caps,"<scan:ColorMode>Grayscale8</scan:ColorMode>"));
    assert(strstr(caps,"<pwg:DocumentFormat>image/jpeg</pwg:DocumentFormat>"));
    assert(strstr(caps,"<scan:XResolution>300</scan:XResolution>"));
    assert(!strstr(caps,"application/pdf"));

    status=escl_scanner_status_xml(0);
    assert(strstr(status,"<pwg:State>Idle</pwg:State>"));
    assert(!strstr(status,"<scan:State>"));
    status=escl_scanner_status_xml(1);
    assert(strstr(status,"<pwg:State>Processing</pwg:State>"));
    return 0;
}
