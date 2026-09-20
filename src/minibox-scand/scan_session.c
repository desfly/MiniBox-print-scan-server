#include "scan_session.h"
#include <string.h>

void minibox_scan_session_reset(struct minibox_scan_session *s)
{
    if (s) memset(s, 0, sizeof(*s));
}

int minibox_scan_session_create(struct minibox_scan_session *s, unsigned id,
                                const struct escl_job *settings)
{
    if (!s || !settings || !id || minibox_scan_session_busy(s)) return -1;
    memset(s, 0, sizeof(*s));
    s->id = id;
    s->settings = *settings;
    s->adf = settings->source == ESCL_SOURCE_ADF;
    s->state = MINIBOX_SCAN_CREATED;
    return 0;
}

int minibox_scan_session_begin_page(struct minibox_scan_session *s, unsigned id)
{
    if (!s || s->id != id) return -1;
    if (s->state != MINIBOX_SCAN_CREATED && s->state != MINIBOX_SCAN_PAGE_DONE)
        return -2;
    if (!s->adf && s->page) return -3;
    s->state = MINIBOX_SCAN_READING;
    ++s->page;
    return 0;
}

int minibox_scan_session_end_page(struct minibox_scan_session *s, int more_pages)
{
    if (!s || s->state != MINIBOX_SCAN_READING) return -1;
    if (s->adf && more_pages) s->state = MINIBOX_SCAN_PAGE_DONE;
    else s->state = MINIBOX_SCAN_DONE;
    return 0;
}

void minibox_scan_session_fail(struct minibox_scan_session *s)
{
    if (s) s->state = MINIBOX_SCAN_FAILED;
}

int minibox_scan_session_busy(const struct minibox_scan_session *s)
{
    if (!s) return 0;
    return s->state == MINIBOX_SCAN_CREATED ||
           s->state == MINIBOX_SCAN_READING ||
           s->state == MINIBOX_SCAN_PAGE_DONE;
}
