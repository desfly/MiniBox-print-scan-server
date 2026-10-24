#include "../src/minibox-discoveryd/wsd_mtom.h"
#include <assert.h>
#include <string.h>
int main(void){
    static const unsigned char body[]=
       "--MIX\r\nContent-Type: application/xop+xml\r\nContent-ID: <soap>\r\n\r\n<s:Envelope/>\r\n"
       "--MIX\r\nContent-Type: application/octet-stream\r\nContent-ID: <pcl>\r\n\r\n"
       "\033%-12345X\000\377PCLXL\r\n--MIX--\r\n";
    static const unsigned char expected[]="\033%-12345X\000\377PCLXL";
    static const unsigned char embedded[]=
       "--MIX\r\nContent-ID: <pcl>\r\n\r\n"
       "BEGIN\r\n--MIXnot-a-boundary\000\377END\r\n--MIX--\r\n";
    static const unsigned char embedded_expected[]="BEGIN\r\n--MIXnot-a-boundary\000\377END";
    static const unsigned char trailing[]=
       "--MIX\r\nContent-ID: <pcl>\r\n\r\nDATA\r\n--MIX--BAD";
    const unsigned char *data=0;size_t n=0;
    assert(mb_wsd_mtom_attachment(body,sizeof body-1,"MIX","pcl",&data,&n)==0);
    assert(n==sizeof expected-1&&!memcmp(data,expected,n));
    assert(mb_wsd_mtom_attachment(body,sizeof body-1,"MIX","unknown",&data,&n)!=0);
    assert(mb_wsd_mtom_attachment(embedded,sizeof embedded-1,"MIX","pcl",&data,&n)==0);
    assert(n==sizeof embedded_expected-1&&!memcmp(data,embedded_expected,n));
    assert(mb_wsd_mtom_attachment(trailing,sizeof trailing-1,"MIX","pcl",&data,&n)!=0);
    return 0;
}
