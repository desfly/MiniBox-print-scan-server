#ifndef MINIBOX_ESCL_H
#define MINIBOX_ESCL_H
#include <stddef.h>

enum escl_source { ESCL_SOURCE_PLATEN, ESCL_SOURCE_ADF };
struct escl_job {
    enum escl_source source;
    unsigned dpi;
    int color;
    /* eSCL scan region units: 1/300 inch. */
    unsigned x_300, y_300, width_300, height_300;
};

int escl_parse_scan_settings(const char *xml,size_t len,struct escl_job *job);
const char *escl_scanner_capabilities_xml(void);
const char *escl_scanner_capabilities_xml_identity(const char *uuid,const char *serial,const char *admin_uri);
const char *escl_scanner_status_xml(int busy,int mfp_online);

#endif
