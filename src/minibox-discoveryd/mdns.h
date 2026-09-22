#pragma once
#include <stddef.h>
#include <signal.h>
#include "service.h"

int mb_mdns_publish_once(const mb_service_t *services, size_t count, const char *hostname);

int mb_mdns_serve(const mb_service_t *services, size_t count, const char *hostname,
                  volatile sig_atomic_t *stop);
