#pragma once
#include <stddef.h>

#define MB_SERVICE_NAME_MAX 96
#define MB_SERVICE_TYPE_MAX 32
#define MB_SERVICE_PATH_MAX 64
#define MB_SERVICE_TXT_MAX 768

typedef struct {
    char name[MB_SERVICE_NAME_MAX];
    char type[MB_SERVICE_TYPE_MAX];
    unsigned short port;
    char path[MB_SERVICE_PATH_MAX];
    char txt[MB_SERVICE_TXT_MAX];
} mb_service_t;

int mb_service_load(const char *path, mb_service_t *out);
int mb_service_validate(const mb_service_t *svc);
int mb_service_add_escl_identity(mb_service_t *svc,
                                 const char *uuid,
                                 const char *hostname);
