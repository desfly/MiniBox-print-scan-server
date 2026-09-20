#ifndef MINIBOX_IPP_H
#define MINIBOX_IPP_H
#include <stddef.h>
#include <stdint.h>
enum { IPP_OP_PRINT_JOB=0x0002, IPP_OP_VALIDATE_JOB=0x0004, IPP_OP_GET_PRINTER_ATTRIBUTES=0x000b };
struct ipp_request { uint8_t major,minor; uint16_t operation; uint32_t request_id; };
int ipp_parse_header(const unsigned char *buf,size_t len,struct ipp_request *r);
int ipp_document_offset(const unsigned char *buf,size_t len,size_t *offset);
const char *ipp_operation_name(uint16_t op);
size_t ipp_build_status(unsigned char *out,size_t cap,const struct ipp_request *r,uint16_t status);
size_t ipp_build_printer_attributes(unsigned char *out,size_t cap,const struct ipp_request *r,const char *printer_uri);
#endif
