#ifndef MINIBOX_ESCL_H
#define MINIBOX_ESCL_H
#include <stddef.h>

enum escl_source { ESCL_SOURCE_PLATEN, ESCL_SOURCE_ADF };
struct escl_job { enum escl_source source; unsigned dpi; int color; };

int escl_parse_scan_settings(const char *xml,size_t len,struct escl_job *job);
const char *escl_scanner_capabilities_xml(void);
const char *escl_scanner_status_xml(int busy);

#endif
