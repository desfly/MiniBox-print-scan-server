#define _POSIX_C_SOURCE 200809L
#include "service.h"
#include "mdns.h"
#include "wsd_identity.h"
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MB_SERVICES_DIR "/etc/minibox/services.d"
#define MB_MAX_SERVICES 8

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
    struct mb_wsd_identity identity;

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

    /*
     * The human service label must also be a stable per-device DNS-SD
     * instance. Otherwise two MiniBoxes (or stale Android/Windows cache
     * entries after an address change) are indistinguishable.
     *
     * eSCL discovery also needs the same stable UUID and a reachable admin
     * URL in its DNS-SD TXT record. Keep those values tied to the WSD
     * identity so Windows/Android do not see two unrelated logical devices.
     */
    rc = mb_wsd_get_identity(&identity);
    if (!rc) {
        const char *uuid = !strncmp(identity.endpoint, "urn:uuid:", 9) ?
                           identity.endpoint + 9 : identity.endpoint;
        unsigned i;
        for (i = 0; i < count; ++i) {
            char unique[MB_SERVICE_NAME_MAX];
            if (!mb_wsd_service_instance(services[i].name, identity.serial,
                                         unique, sizeof(unique)))
                strcpy(services[i].name, unique);
            rc = mb_service_add_escl_identity(&services[i], uuid, hostname);
            if (rc) {
                fprintf(stderr, "minibox-discoveryd: eSCL identity failed: %d\n", rc);
                return 2;
            }
        }
    } else {
        fprintf(stderr, "minibox-discoveryd: stable device identity unavailable: %d\n", rc);
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (once) {
        rc = mb_mdns_publish_once(services, count, hostname);
        if (rc) {
            fprintf(stderr, "minibox-discoveryd: mDNS publish failed: %d\n", rc);
            return 4;
        }
        printf("minibox-discoveryd: published %u service(s) as %s.local\n", count, hostname);
        return 0;
    }
    rc = mb_mdns_run(services, count, hostname, &stop);
    if (rc) fprintf(stderr, "minibox-discoveryd: mDNS listener failed: %d\n", rc);
    return rc ? 4 : 0;
}
