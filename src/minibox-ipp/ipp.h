#ifndef MINIBOX_IPP_H
#define MINIBOX_IPP_H
#include <stddef.h>
#include <stdint.h>
enum { IPP_OP_PRINT_JOB=0x0002, IPP_OP_VALIDATE_JOB=0x0004, IPP_OP_GET_PRINTER_ATTRIBUTES=0x000b };
struct ipp_request { uint8_t major,minor; uint16_t operation; uint32_t request_id; };
enum ipp_document_kind {
    IPP_DOCUMENT_MALFORMED=-1,
    IPP_DOCUMENT_RAW=0,
    IPP_DOCUMENT_UNSUPPORTED=1,
    IPP_DOCUMENT_PWG_RASTER=2
};
int ipp_parse_header(const unsigned char *buf,size_t len,struct ipp_request *r);
int ipp_raw_format_supported(const unsigned char *buf,size_t len);
int ipp_document_offset(const unsigned char *buf,size_t len,size_t *offset);
int ipp_document_format_kind(const unsigned char *buf,size_t len);
/* Inspect a complete IPP attribute section. 0: supported (raw or PWG Raster); 1: explicit unsupported format; -1: malformed/incomplete. */
int ipp_check_document_format(const unsigned char *buf,size_t len);
const char *ipp_operation_name(uint16_t op);
size_t ipp_build_status(unsigned char *out,size_t cap,const struct ipp_request *r,uint16_t status);
size_t ipp_build_printer_attributes(unsigned char *out,size_t cap,const struct ipp_request *r,const char *printer_uri);
#endif
