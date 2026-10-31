#ifndef MINIBOX_SOAPHT_CODEC_H
#define MINIBOX_SOAPHT_CODEC_H

#include <stddef.h>
#include "../minibox-escl/escl.h"
#include "soapht_transport.h"

struct soapht_codec {
    int (*start)(struct soapht_session *transport, const struct escl_job *job);
    int (*read_image)(struct soapht_session *transport,
                      unsigned char *buf, size_t cap, size_t *got);
    int (*end_page)(struct soapht_session *transport, int *more_pages);
    int (*finish)(struct soapht_session *transport);
};

/*
 * M1522 SOAPHT command encoding is implemented and regression-checked against
 * the preserved 2026-09-16 USB transcript. Transport and command encoding stay
 * separate contracts so USB retry/framing and SOAP/DIME parsing can be tested
 * independently.
 */
extern const struct soapht_codec *minibox_soapht_codec;

#endif
