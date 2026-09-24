#ifndef MINIBOX_WSD_H
#define MINIBOX_WSD_H
#include <stddef.h>

enum mb_wsd_kind { MB_WSD_NONE=0, MB_WSD_PROBE=1, MB_WSD_RESOLVE=2 };
struct mb_wsd_request {
    enum mb_wsd_kind kind;
    char message_id[192];
    char types[256];
    char endpoint[192];
};

/* Parse only the bounded discovery fields needed before a response is built.
 * This does not enable WSD advertisement: HTTP metadata/WS-Print must exist first. */
int mb_wsd_parse(const char *xml,size_t len,struct mb_wsd_request *out);
int mb_wsd_is_print_probe(const struct mb_wsd_request *r);
#endif
