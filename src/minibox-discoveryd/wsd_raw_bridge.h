#ifndef MINIBOX_WSD_RAW_BRIDGE_H
#define MINIBOX_WSD_RAW_BRIDGE_H
#include <stddef.h>
/* Returns zero only when every payload byte is delivered to the local RAW
 * print listener. The caller must first authenticate/validate its WSD job.
 * Does not parse or alter the PCL XL stream. */
int mb_wsd_raw_submit(const unsigned char *data,size_t length,unsigned short port);
#endif
