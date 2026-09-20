#include "../src/minibox-escl/escl.h"
#include <assert.h>
#include <string.h>
int main(void){const char*x="<scan:ScanSettings><scan:InputSource>Adf</scan:InputSource><scan:XResolution>300</scan:XResolution><scan:ColorMode>RGB24</scan:ColorMode></scan:ScanSettings>";struct escl_job j;assert(escl_parse_scan_settings(x,strlen(x),&j)==0);assert(j.source==ESCL_SOURCE_ADF&&j.dpi==300&&j.color);return 0;}
