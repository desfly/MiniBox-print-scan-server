#include <assert.h>
#include <string.h>
#include "../src/minibox-scan/soapht_transport.h"
struct mock { int opened,writes,reads,closed; char channel[32]; };
static int op(void*v,const char*c){struct mock*m=v;m->opened++;strncpy(m->channel,c,sizeof m->channel-1);m->channel[sizeof m->channel-1]=0;return 0;}
static int wr(void*v,const unsigned char*b,size_t n){struct mock*m=v;(void)b;if(!n)return-1;m->writes++;return 0;}
static int rd(void*v,unsigned char*b,size_t cap,size_t*got){struct mock*m=v;if(cap<4)return-1;memcpy(b,"JPEG",4);*got=4;m->reads++;return 0;}
static void cl(void*v){((struct mock*)v)->closed++;}
int main(void){struct mock m={0};struct soapht_io io={op,wr,rd,cl};struct soapht_session s;unsigned char b[8];size_t n=0;assert(!soapht_open(&s,&io,&m));assert(!strcmp(m.channel,"HP-SOAP-SCAN"));assert(!soapht_write(&s,(const unsigned char*)"REQ",3));assert(!soapht_read(&s,b,sizeof b,&n));assert(n==4&&!memcmp(b,"JPEG",4));soapht_close(&s);assert(m.opened==1&&m.writes==1&&m.reads==1&&m.closed==1);return 0;}
