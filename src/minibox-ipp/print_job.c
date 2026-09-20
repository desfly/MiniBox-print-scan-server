#include "print_job.h"
#include "../minibox-usb/stream.h"
int minibox_print_document(print_sink_fn sink,void*ctx,const unsigned char*doc,size_t len,size_t chunk,int timeout_ms){if(!doc||!len)return-1;return minibox_stream_write(sink,ctx,doc,len,chunk,timeout_ms);}
