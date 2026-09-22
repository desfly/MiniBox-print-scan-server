#define _POSIX_C_SOURCE 200809L
#include "service.h"
#include "mdns.h"
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MB_SERVICES_DIR "/etc/minibox/services.d"
#define MB_MAX_SERVICES 8
#define MB_ANNOUNCE_INTERVAL_SEC 60

static volatile sig_atomic_t stop;

static void on_signal(int sig) {
    (void)sig;
    stop = 1;
}

static int has_suffix(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && strcmp(s + a - b, suffix) == 0;
}

int main(int argc, char **argv) {
    const char *dir = MB_SERVICES_DIR;
    int once = 0;
    int argi = 1;
    mb_service_t services[MB_MAX_SERVICES];
    char hostname[64] = "minibox";
    DIR *d;
    struct dirent *de;
    unsigned count = 0;
    int rc;

    if (argi < argc && !strcmp(argv[argi], "--once")) {
        once = 1;
        argi++;
    }
    if (argi < argc) dir = argv[argi++];
    if (argi != argc) {
        fprintf(stderr, "usage: %s [--once] [services-dir]\n", argv[0]);
        return 2;
    }

    d = opendir(dir);
    if (!d) {
        fprintf(stderr, "minibox-discoveryd: cannot open %s: %s\n", dir, strerror(errno));
        return 1;
    }
    while ((de = readdir(d)) != NULL) {
        char path[512];
        if (de->d_name[0] == '.' || !has_suffix(de->d_name, ".service")) continue;
        if (count == MB_MAX_SERVICES) { closedir(d); return 2; }
        if (snprintf(path, sizeof(path), "%s/%s", dir, de->d_name) >= (int)sizeof(path)) { closedir(d); return 2; }
        rc = mb_service_load(path, &services[count]);
        if (rc) {
            fprintf(stderr, "minibox-discoveryd: invalid %s: %d\n", path, rc);
            closedir(d);
            return 2;
        }
        count++;
    }
    closedir(d);
    if (!count) return 3;

    if (gethostname(hostname, sizeof(hostname) - 1) != 0 || !hostname[0]) strcpy(hostname, "minibox");
    hostname[sizeof(hostname) - 1] = 0;

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (once) return mb_mdns_publish_once(services, count, hostname) ? 4 : 0;
    rc = mb_mdns_serve(services, count, hostname, &stop);
    if (rc) fprintf(stderr, "minibox-discoveryd: responder failed: %d\n", rc);
    return rc ? 4 : 0;
}
