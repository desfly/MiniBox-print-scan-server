#include "../src/minibox-discoveryd/wsd.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 const char *probe="<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" xmlns:p=\"http://schemas.microsoft.com/windows/2006/08/wdp/print\"><s:Header><a:MessageID>urn:uuid:01234567-89ab-cdef-0123-456789abcdef</a:MessageID></s:Header><s:Body><d:Probe><d:Types>p:PrintDeviceType</d:Types></d:Probe></s:Body></s:Envelope>";
 const char *resolve="<s:Envelope><s:Header><a:MessageID>urn:uuid:11111111-2222-3333-4444-555555555555</a:MessageID></s:Header><s:Body><d:Resolve><a:EndpointReference><a:Address>urn:uuid:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee</a:Address></a:EndpointReference></d:Resolve></s:Body></s:Envelope>";
 const char *no_envelope="<Probe><MessageID>x</MessageID></Probe>";
 const char *no_message_id="<s:Envelope><s:Body><d:Probe/></s:Body></s:Envelope>";
 struct mb_wsd_request r;
 assert(mb_wsd_parse(probe,strlen(probe),&r)==0);assert(r.kind==MB_WSD_PROBE);assert(mb_wsd_is_print_probe(&r));assert(!strcmp(r.types,"p:PrintDeviceType"));
 assert(mb_wsd_parse(resolve,strlen(resolve),&r)==0);assert(r.kind==MB_WSD_RESOLVE);assert(!strcmp(r.endpoint,"urn:uuid:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"));assert(!mb_wsd_is_print_probe(&r));
 assert(mb_wsd_parse(no_envelope,strlen(no_envelope),&r)<0);
 assert(mb_wsd_parse(no_message_id,strlen(no_message_id),&r)<0);
 puts("WSD parser contract OK");return 0;
}
