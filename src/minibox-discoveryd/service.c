#include "service.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim(char *s) {
    char *p = s;
    size_t n;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    n = strlen(s);
    while (n && (s[n-1] == '\r' || s[n-1] == '\n' || s[n-1] == ' ' || s[n-1] == '\t')) s[--n] = 0;
}

static int copy_value(char *dst, size_t cap, const char *src) {
    size_t n = strlen(src);
    if (!cap || n >= cap) return -E2BIG;
    memcpy(dst, src, n + 1);
    return 0;
}

static int has_txt_key(const char *txt, const char *key) {
    size_t k;
    const char *p;
    if (!txt || !key || !*key) return 0;
    k = strlen(key);
    for (p = txt; *p; ) {
        const char *end = strchr(p, ';');
        size_t n = end ? (size_t)(end - p) : strlen(p);
        if (n > k && !memcmp(p, key, k) && p[k] == '=') return 1;
        if (!end) break;
        p = end + 1;
    }
    return 0;
}

static int append_txt_kv(char *txt, size_t cap, const char *key, const char *value) {
    size_t used;
    int n;
    if (!txt || !cap || !key || !value || !*key || strchr(key, '=') || strchr(key, ';') ||
        strchr(value, ';')) return -EINVAL;
    if (has_txt_key(txt, key)) return 0;
    used = strlen(txt);
    n = snprintf(txt + used, cap - used, "%s%s=%s", used ? ";" : "", key, value);
    if (n < 0 || (size_t)n >= cap - used) return -E2BIG;
    return 0;
}

int mb_service_validate(const mb_service_t *s) {
    if (!s || !s->name[0] || !s->type[0] || !s->port) return -EINVAL;
    if (s->type[0] != '_' || strstr(s->type, "._tcp") == NULL) return -EINVAL;
    if (s->path[0] && s->path[0] != '/') return -EINVAL;
    return 0;
}

int mb_service_add_escl_identity(mb_service_t *s,
                                 const char *uuid,
                                 const char *hostname) {
    char adminurl[160];
    int rc;
    if (!s || !uuid || !*uuid || !hostname || !*hostname) return -EINVAL;
    /* Keep the same stable UUID/admin URL across eSCL and IPP DNS-SD records.\n     * Windows and other driverless clients use UUID to correlate service\n     * advertisements with the same physical MFP. */\n    if (strcmp(s->type, "_uscan._tcp") && strcmp(s->type, "_uscans._tcp") &&\n        strcmp(s->type, "_ipp._tcp") && strcmp(s->type, "_print._sub._ipp._tcp")) return 0;\n    if (snprintf(adminurl, sizeof(adminurl), "http://%s.local/", hostname) >=
        (int)sizeof(adminurl)) return -E2BIG;
    rc = append_txt_kv(s->txt, sizeof(s->txt), "UUID", uuid);
    if (rc) return rc;
    return append_txt_kv(s->txt, sizeof(s->txt), "adminurl", adminurl);
}

int mb_service_load(const char *path, mb_service_t *out) {
    FILE *f;
    char line[1024];
    int rc = 0;
    if (!path || !out) return -EINVAL;
    memset(out, 0, sizeof(*out));
    f = fopen(path, "r");
    if (!f) return -errno;
    while (fgets(line, sizeof(line), f)) {
        char *eq, *key, *val;
        trim(line);
        if (!line[0] || line[0] == '#' || line[0] == '[') continue;
        eq = strchr(line, '=');
        if (!eq) { rc = -EINVAL; break; }
        *eq = 0; key = line; val = eq + 1; trim(key); trim(val);
        if (!strcmp(key, "name")) rc = copy_value(out->name, sizeof(out->name), val);
        else if (!strcmp(key, "type")) rc = copy_value(out->type, sizeof(out->type), val);
        else if (!strcmp(key, "path")) rc = copy_value(out->path, sizeof(out->path), val);
        else if (!strcmp(key, "txt")) rc = copy_value(out->txt, sizeof(out->txt), val);
        else if (!strcmp(key, "port")) {
            char *end = NULL; unsigned long p;
            errno = 0; p = strtoul(val, &end, 10);
            if (errno || !end || *end || p < 1 || p > 65535) rc = -EINVAL;
            else out->port = (unsigned short)p;
        }
        if (rc) break;
    }
    if (ferror(f) && !rc) rc = -EIO;
    fclose(f);
    return rc ? rc : mb_service_validate(out);
}
