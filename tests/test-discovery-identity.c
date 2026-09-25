#include "../src/minibox-discoveryd/wsd_identity.h"
#include <assert.h>
#include <string.h>

int main(void){
    struct mb_wsd_identity id;
    char out[40];
    memset(&id,0,sizeof id);
    strcpy(id.endpoint,"urn:uuid:4d424f58-0000-4000-8000-001122aabbcc");
    assert(mb_wsd_uuid_value(&id,out,sizeof out)==0);
    assert(strcmp(out,"4d424f58-0000-4000-8000-001122aabbcc")==0);
    strcpy(id.endpoint,"http://not-a-uuid");
    assert(mb_wsd_uuid_value(&id,out,sizeof out)<0);
    strcpy(id.endpoint,"urn:uuid:short");
    assert(mb_wsd_uuid_value(&id,out,sizeof out)<0);
    return 0;
}
