#include "../src/minibox-discoveryd/wsd_identity.h"
#include <assert.h>
#include <stddef.h>

int main(void){
    struct mb_wsd_identity id;
    /* Human-visible names are intentionally not derived from this identity.
     * Keep this unit test focused on the identity structure contract so a
     * MAC-derived display suffix cannot silently return. */
    assert(sizeof id.serial >= 13);
    assert(sizeof id.endpoint >= 64);
    assert(sizeof id.xaddr >= 128);
    return 0;
}
