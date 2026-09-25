#include "../src/minibox-discoveryd/wsd_identity.h"
#include <assert.h>
#include <string.h>

int main(void){
    char out[96];
    assert(mb_wsd_service_instance("HP LaserJet M1522n @ MiniBox",
                                   "001122aabbcc",out,sizeof out)==0);
    assert(strcmp(out,"HP LaserJet M1522n @ MiniBox [aabbcc]")==0);
    assert(mb_wsd_service_instance("x","123456",out,sizeof out)==0);
    assert(strcmp(out,"x [123456]")==0);
    assert(mb_wsd_service_instance("x","12345",out,sizeof out)<0);
    {
        char tiny[8];
        assert(mb_wsd_service_instance("HP LaserJet","001122aabbcc",
                                       tiny,sizeof tiny)<0);
    }
    return 0;
}
