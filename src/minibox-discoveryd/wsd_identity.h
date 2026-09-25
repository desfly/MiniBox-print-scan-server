#ifndef MINIBOX_WSD_IDENTITY_H
#define MINIBOX_WSD_IDENTITY_H
#include <net/if.h>
#include <netinet/in.h>

struct mb_wsd_identity {
    char ifname[IF_NAMESIZE];
    struct in_addr ipv4;
    char endpoint[96];
    char xaddr[160];
    char presentation[128];
    char serial[32];
};

int mb_wsd_get_identity(struct mb_wsd_identity *out);
int mb_wsd_service_instance(const char *base,const char *serial,
                            char *out,size_t cap);
#endif
