#include "../src/minibox-scand/scan_session.h"
#include <assert.h>
#include <stdio.h>

static struct escl_job job(enum escl_source source)
{
    struct escl_job j;
    j.source = source;
    j.dpi = 300;
    j.color = 1;
    return j;
}

static void test_platen(void)
{
    struct minibox_scan_session s;
    struct escl_job j = job(ESCL_SOURCE_PLATEN);
    minibox_scan_session_reset(&s);
    assert(s.state == MINIBOX_SCAN_IDLE);
    assert(!minibox_scan_session_busy(&s));
    assert(minibox_scan_session_create(&s, 7, &j) == 0);
    assert(minibox_scan_session_busy(&s));
    assert(minibox_scan_session_begin_page(&s, 8) == -1);
    assert(minibox_scan_session_begin_page(&s, 7) == 0);
    assert(s.page == 1 && s.state == MINIBOX_SCAN_READING);
    assert(minibox_scan_session_end_page(&s, 0) == 0);
    assert(s.state == MINIBOX_SCAN_DONE);
    assert(!minibox_scan_session_busy(&s));
    assert(minibox_scan_session_begin_page(&s, 7) == -2);
}

static void test_adf(void)
{
    struct minibox_scan_session s;
    struct escl_job j = job(ESCL_SOURCE_ADF);
    minibox_scan_session_reset(&s);
    assert(minibox_scan_session_create(&s, 9, &j) == 0);
    assert(s.adf);
    assert(minibox_scan_session_begin_page(&s, 9) == 0);
    assert(minibox_scan_session_end_page(&s, 1) == 0);
    assert(s.state == MINIBOX_SCAN_PAGE_DONE);
    assert(minibox_scan_session_busy(&s));
    assert(minibox_scan_session_begin_page(&s, 9) == 0);
    assert(s.page == 2);
    assert(minibox_scan_session_end_page(&s, 0) == 0);
    assert(s.state == MINIBOX_SCAN_DONE);
}

static void test_busy_and_failure(void)
{
    struct minibox_scan_session s;
    struct escl_job j = job(ESCL_SOURCE_PLATEN);
    minibox_scan_session_reset(&s);
    assert(minibox_scan_session_create(&s, 1, &j) == 0);
    assert(minibox_scan_session_create(&s, 2, &j) == -1);
    minibox_scan_session_fail(&s);
    assert(s.state == MINIBOX_SCAN_FAILED);
    assert(!minibox_scan_session_busy(&s));
    assert(minibox_scan_session_create(&s, 2, &j) == 0);
    assert(s.id == 2 && s.page == 0);
}

int main(void)
{
    test_platen();
    test_adf();
    test_busy_and_failure();
    puts("scan session tests: OK");
    return 0;
}
