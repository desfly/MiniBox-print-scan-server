/* MiniBox MFP - parser for verified M1522 USB capture records.
 * Text format: OUT <hex bytes>, IN <hex bytes>. Comments start with #.
 * It validates and converts captured traffic; it never invents protocol bytes.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_FRAME 65536
struct frame { int dir_in; size_t len; uint8_t data[MAX_FRAME]; };

static int hexval(int c){if(c>='0'&&c<='9')return c-'0';c=tolower(c);if(c>='a'&&c<='f')return c-'a'+10;return -1;}
static int parse_hex(const char *p,uint8_t *out,size_t *len){size_t n=0;while(*p){int a,b;while(*p==' '||*p=='\t')p++;if(!*p||*p=='\n'||*p=='#')break;a=hexval((unsigned char)*p++);if(a<0)return -1;b=hexval((unsigned char)*p++);if(b<0)return -1;if(n>=MAX_FRAME)return -2;out[n++]=(uint8_t)((a<<4)|b);while(*p==' '||*p=='\t')p++;}*len=n;return n?0:-3;}
static int parse_line(char *line,struct frame *f){char *p=line;while(*p==' '||*p=='\t')p++;if(!*p||*p=='#'||*p=='\n')return 1;if(!strncmp(p,"IN ",3)){f->dir_in=1;p+=3;}else if(!strncmp(p,"OUT ",4)){f->dir_in=0;p+=4;}else return -1;return parse_hex(p,f->data,&f->len);}
static uint32_t fnv1a(const uint8_t *p,size_t n){uint32_t h=2166136261u;while(n--){h^=*p++;h*=16777619u;}return h;}

int main(int argc,char **argv){FILE *fp;char line[131200];unsigned ln=0,frames=0,in=0,out=0;unsigned long bytes=0;if(argc!=2){fprintf(stderr,"usage: %s capture.txt\n",argv[0]);return 64;}fp=fopen(argv[1],"r");if(!fp){perror(argv[1]);return 2;}while(fgets(line,sizeof(line),fp)){struct frame f;int rc;ln++;rc=parse_line(line,&f);if(rc==1)continue;if(rc<0){fprintf(stderr,"invalid capture line %u rc=%d\n",ln,rc);fclose(fp);return 3;}frames++;bytes+=f.len;if(f.dir_in)in++;else out++;printf("FRAME=%u DIR=%s LEN=%lu FNV1A=%08x\n",frames,f.dir_in?"IN":"OUT",(unsigned long)f.len,fnv1a(f.data,f.len));}fclose(fp);if(!frames||!in||!out){fprintf(stderr,"capture must contain both OUT and IN frames\n");return 4;}printf("CAPTURE=valid\nFRAMES=%u\nIN_FRAMES=%u\nOUT_FRAMES=%u\nBYTES=%lu\n",frames,in,out,bytes);return 0;}
