#pragma once
#include <stddef.h>
#include <signal.h>
#include "service.h"

/* Send a one-shot announcement; also used by the --once smoke test. */
int mb_mdns_publish_once(const mb_service_t *services, size_t count, const char *hostname);

/* Pure DNS query-to-answer encoder; returns 0 for an irrelevant query,
 * -1 for malformed packets, otherwise the byte count. */
int mb_mdns_build_reply(const unsigned char *query, size_t query_len,
                        const mb_service_t *services, size_t count,
                        const char *hostname, const unsigned char ipv4[4],
                        unsigned char *out, size_t out_cap);

/* Own UDP/5353; answer questions and announce on a bounded interval. */
int mb_mdns_run(const mb_service_t *services, size_t count,
                const char *hostname, volatile sig_atomic_t *stop);
