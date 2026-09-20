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
 * HorseThief command encoding is intentionally not implemented here until a
 * verified M1522 transcript/spec is available.  The open HPLIP frontend uses
 * a restricted bb_soapht plugin for these operations; transport and command
 * encoding therefore remain separate contracts.
 */
extern const struct soapht_codec *minibox_soapht_codec;

#endif
