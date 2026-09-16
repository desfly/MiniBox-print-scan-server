/* MiniBox MFP - guarded HP M1522n scan-session state machine.
 * Protocol frames are supplied from a verified capture; no guessed vendor bytes.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum scan_state { SCAN_IDLE, SCAN_OPEN, SCAN_HANDSHAKE, SCAN_STREAM, SCAN_DONE, SCAN_ERROR };
struct scan_session { enum scan_state state; unsigned long bytes; unsigned frames; int last_error; };

static const char *state_name(enum scan_state s) {
    switch(s){case SCAN_IDLE:return "idle";case SCAN_OPEN:return "open";case SCAN_HANDSHAKE:return "handshake";
    case SCAN_STREAM:return "stream";case SCAN_DONE:return "done";default:return "error";}
}
static void fail(struct scan_session *s,int err){s->last_error=err;s->state=SCAN_ERROR;}
static int begin(struct scan_session *s){if(s->state!=SCAN_IDLE)return -1;s->state=SCAN_OPEN;return 0;}
static int transport_ready(struct scan_session *s){if(s->state!=SCAN_OPEN)return -1;s->state=SCAN_HANDSHAKE;return 0;}
static int handshake_verified(struct scan_session *s){if(s->state!=SCAN_HANDSHAKE)return -1;s->state=SCAN_STREAM;return 0;}
static int consume(struct scan_session *s,const uint8_t *p,size_t n,FILE *out){
    if(s->state!=SCAN_STREAM||!p||!n)return -1;
    if(fwrite(p,1,n,out)!=n){fail(s,-2);return -2;} s->bytes+=(unsigned long)n;s->frames++;return 0;
}
static int finish(struct scan_session *s){if(s->state!=SCAN_STREAM)return -1;s->state=SCAN_DONE;return 0;}
static void status(const struct scan_session *s){
    printf("SCAN_STATE=%s\nSCAN_BYTES=%lu\nSCAN_FRAMES=%u\nSCAN_ERROR=%d\n",state_name(s->state),s->bytes,s->frames,s->last_error);
}

/* Test harness now; transport will call these transitions after verified M1522 capture is decoded. */
int main(int argc,char **argv){
    struct scan_session s={SCAN_IDLE,0,0,0}; FILE *out=NULL; uint8_t sample[16]={0};
    if(begin(&s)||transport_ready(&s)){fail(&s,-10);status(&s);return 2;}
    if(argc<2||strcmp(argv[1],"verified-test")!=0){
        /* Critical guard: do not enter STREAM until handshake has been verified. */
        status(&s); fprintf(stderr,"handshake capture not verified; stream disabled\n"); return 3;
    }
    if(handshake_verified(&s)){fail(&s,-11);status(&s);return 4;}
    out=tmpfile(); if(!out){fail(&s,-12);status(&s);return 5;}
    if(consume(&s,sample,sizeof(sample),out)||finish(&s)){fclose(out);status(&s);return 6;}
    fclose(out);status(&s);return 0;
}
