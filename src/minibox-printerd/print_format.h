#ifndef MINIBOX_PRINT_FORMAT_H
#define MINIBOX_PRINT_FORMAT_H
#include <stddef.h>
#include "../minibox-ipp/ipp.h"

enum mb_print_input_kind {
    MB_PRINT_INPUT_REJECT=-1,
    MB_PRINT_INPUT_PWG=1,
    MB_PRINT_INPUT_RAW=2
};

int mb_print_resolve_format(int declared_kind,int inherited_kind,
                            const unsigned char *payload,size_t payload_len);
#endif
