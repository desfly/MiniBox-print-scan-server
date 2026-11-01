#ifndef MINIBOX_WSD_MTOM_H
#define MINIBOX_WSD_MTOM_H
#include <stddef.h>
/* Binary-safe in-memory MIME frame locator. This does not send a print job.
 * On success returns the exact attachment bytes as a slice of the input.
 * Callers must validate SOAP/xop references and stream larger documents
 * using a bounded reader before treating them as authenticated job data. */
int mb_wsd_mtom_attachment(const unsigned char *buf,size_t len,
                           const char *boundary,const char *content_id,
                           const unsigned char **out,size_t *out_len);
#endif
