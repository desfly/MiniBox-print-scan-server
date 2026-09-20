#pragma once
#include <stddef.h>
#include "service.h"

int mb_mdns_publish_once(const mb_service_t *services, size_t count, const char *hostname);
