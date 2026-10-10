#include "../src/minibox-discoveryd/wsd_identity.h"
#include "../src/minibox-identity/m1522_identity.h"
#include <assert.h>
#include <string.h>

int main(void){
    struct mb_wsd_identity id;
    assert(!strcmp(MINIBOX_MFP_SERIAL,"0cefafcfc53d"));
    assert(!strcmp(MINIBOX_MFP_URN_UUID,
                   "urn:uuid:4d424f58-0000-4000-8000-0cefafcfc53d"));
    assert(sizeof id.serial > strlen(MINIBOX_MFP_SERIAL));
    assert(sizeof id.endpoint > strlen(MINIBOX_MFP_URN_UUID));
    return 0;
}
