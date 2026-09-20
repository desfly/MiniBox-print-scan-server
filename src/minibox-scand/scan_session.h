#ifndef MINIBOX_SCAN_SESSION_H
#define MINIBOX_SCAN_SESSION_H

#include "../minibox-escl/escl.h"

enum minibox_scan_state {
    MINIBOX_SCAN_IDLE = 0,
    MINIBOX_SCAN_CREATED,
    MINIBOX_SCAN_READING,
    MINIBOX_SCAN_PAGE_DONE,
    MINIBOX_SCAN_DONE,
    MINIBOX_SCAN_FAILED
};

struct minibox_scan_session {
    unsigned id;
    struct escl_job settings;
    enum minibox_scan_state state;
    unsigned page;
    int adf;
};

void minibox_scan_session_reset(struct minibox_scan_session *s);
int minibox_scan_session_create(struct minibox_scan_session *s, unsigned id,
                                const struct escl_job *settings);
int minibox_scan_session_begin_page(struct minibox_scan_session *s, unsigned id);
int minibox_scan_session_end_page(struct minibox_scan_session *s, int more_pages);
void minibox_scan_session_fail(struct minibox_scan_session *s);
int minibox_scan_session_busy(const struct minibox_scan_session *s);

#endif
