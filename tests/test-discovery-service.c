#include "../src/minibox-discoveryd/service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check(const char *path, const char *type, unsigned port, const char *resource) {
    mb_service_t s;
    assert(mb_service_load(path, &s) == 0);
    assert(strcmp(s.type, type) == 0);
    assert(s.port == port);
    assert(strcmp(s.path, resource) == 0);
    assert(strstr(s.name, "M1522n") != NULL);
}

int main(void) {
    mb_service_t p;
    check("overlay/etc/minibox/services.d/ipp-printer.service", "_ipp._tcp", 631, "/ipp/print");
    check("package/minibox-mfp/files/etc/minibox/services.d/ipp-print-subtype.service", "_print._sub._ipp._tcp", 631, "/ipp/print");
    check("overlay/etc/minibox/services.d/scanner.service", "_uscan._tcp", 8080, "/eSCL");
    assert(mb_service_load("overlay/etc/minibox/services.d/ipp-printer.service", &p) == 0);
    assert(strstr(p.txt, "rp=ipp/print") != NULL);
    assert(strstr(p.txt, "pdl=application/octet-stream,image/pwg-raster") != NULL);
    assert(strstr(p.txt, "Duplex=F") != NULL);
    assert(strstr(p.txt, "application/pdf") == NULL);
    assert(strstr(p.txt, "image/urf") == NULL);
    assert(strstr(p.txt, "URF=") == NULL);
    assert(mb_service_add_escl_identity(&p,
        "4d424f58-0000-4000-8000-0cefafcfc53d", "OpenWrt") == 0);
    assert(strstr(p.txt, "UUID=4d424f58-0000-4000-8000-0cefafcfc53d") != NULL);
    assert(strstr(p.txt, "adminurl=http://OpenWrt.local/") != NULL);

    assert(mb_service_load("package/minibox-mfp/files/etc/minibox/services.d/ipp-print-subtype.service", &p) == 0);
    assert(strstr(p.txt, "rp=ipp/print") != NULL);
    assert(mb_service_add_escl_identity(&p,
        "4d424f58-0000-4000-8000-0cefafcfc53d", "OpenWrt") == 0);
    assert(strstr(p.txt, "UUID=4d424f58-0000-4000-8000-0cefafcfc53d") != NULL);

    assert(mb_service_load("package/minibox-mfp/files/etc/minibox/services.d/scanner.service", &p) == 0);
    assert(strstr(p.txt, "duplex=F") != NULL);
    assert(strstr(p.txt, "application/pdf") == NULL);
    assert(strstr(p.txt, "pdl=image/jpeg") != NULL);
    assert(strstr(p.txt, "is=platen,adf") != NULL);
    assert(strstr(p.txt, "cs=grayscale,color") != NULL);
    assert(strstr(p.txt, "representation=images") == NULL);
    assert(mb_service_add_escl_identity(&p,
        "4d424f58-0000-4000-8000-0cefafcfc53d", "OpenWrt") == 0);
    assert(strstr(p.txt, "UUID=4d424f58-0000-4000-8000-0cefafcfc53d") != NULL);
    assert(strstr(p.txt, "adminurl=http://OpenWrt.local/") != NULL);
    assert(mb_service_add_escl_identity(&p,
        "4d424f58-0000-4000-8000-0cefafcfc53d", "OpenWrt") == 0);
    assert(strstr(strstr(p.txt, "UUID=") + 1, "UUID=") == NULL);

    puts("discovery service contracts: OK");
    return 0;
}
