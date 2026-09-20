#include "stream.h"
#include <limits.h>
int minibox_stream_write(minibox_write_fn fn,void*ctx,const unsigned char*d,size_t n,size_t chunk,int t){if(!fn||(!d&&n)||!chunk)return-1;size_t off=0;while(off<n){size_t want=n-off;if(want>chunk)want=chunk;if(want>(size_t)INT_MAX)return-2;int w=fn(ctx,d+off,(int)want,t);if(w<=0)return-3;if((size_t)w>want)return-4;off+=(size_t)w;}return 0;}
