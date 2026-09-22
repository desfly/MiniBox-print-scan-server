#include "../src/minibox-discoveryd/wsd.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 const char *probe="<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" xmlns:p=\"http://schemas.microsoft.com/windows/2006/08/wdp/print\"><s:Header><a:MessageID>urn:uuid:01234567-89ab-cdef-0123-456789abcdef</a:MessageID></s:Header><s:Body><d:Probe><d:Types>x:Other p:PrintDeviceType</d:Types></d:Probe></s:Body></s:Envelope>";
 const char *false_probe="<s:Envelope><s:Header><a:MessageID>urn:uuid:22222222-2222-3333-4444-555555555555</a:MessageID></s:Header><s:Body><d:Probe><d:Types>p:NotPrintDeviceType x:PrintDeviceTypeExtra</d:Types></d:Probe></s:Body></s:Envelope>";
 const char *resolve="<s:Envelope><s:Header><a:MessageID>urn:uuid:11111111-2222-3333-4444-555555555555</a:MessageID></s:Header><s:Body><d:Resolve><a:EndpointReference><a:Address>urn:uuid:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee</a:Address></a:EndpointReference></d:Resolve></s:Body></s:Envelope>";
 const char *header_injection="<s:Envelope><s:Header><a:MessageID>urn:uuid:33333333-2222-3333-4444-555555555555</a:MessageID><d:Probe><d:Types>p:PrintDeviceType</d:Types></d:Probe></s:Header><s:Body><x:Noise/></s:Body></s:Envelope>";
 const char *header_probe_body_resolve="<s:Envelope><s:Header><a:MessageID>urn:uuid:44444444-2222-3333-4444-555555555555</a:MessageID><d:Probe><d:Types>p:PrintDeviceType</d:Types></d:Probe></s:Header><s:Body><d:Resolve><a:EndpointReference><a:Address>urn:uuid:bbbbbbbb-cccc-dddd-eeee-ffffffffffff</a:Address></a:EndpointReference></d:Resolve></s:Body></s:Envelope>";
 const char *outside_body="<s:Envelope><s:Header><a:MessageID>urn:uuid:55555555-2222-3333-4444-555555555555</a:MessageID></s:Header></s:Envelope><s:Body><d:Probe><d:Types>p:PrintDeviceType</d:Types></d:Probe></s:Body>";
 const char *outside_message="<s:Envelope><s:Header></s:Header><s:Body><d:Probe><d:Types>p:PrintDeviceType</d:Types></d:Probe></s:Body></s:Envelope><a:MessageID>urn:uuid:66666666-2222-3333-4444-555555555555</a:MessageID>";
 const char *no_envelope="<Probe><MessageID>x</MessageID></Probe>";
 const char *no_message_id="<s:Envelope><s:Body><d:Probe/></s:Body></s:Envelope>";
 struct mb_wsd_request r;
 assert(mb_wsd_parse(probe,strlen(probe),&r)==0);assert(r.kind==MB_WSD_PROBE);assert(mb_wsd_is_print_probe(&r));assert(!strcmp(r.types,"x:Other p:PrintDeviceType"));
 assert(mb_wsd_parse(false_probe,strlen(false_probe),&r)==0);assert(!mb_wsd_is_print_probe(&r));
 assert(mb_wsd_parse(resolve,strlen(resolve),&r)==0);assert(r.kind==MB_WSD_RESOLVE);assert(!strcmp(r.endpoint,"urn:uuid:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"));assert(!mb_wsd_is_print_probe(&r));
 assert(mb_wsd_parse(header_injection,strlen(header_injection),&r)<0);
 assert(mb_wsd_parse(header_probe_body_resolve,strlen(header_probe_body_resolve),&r)==0);assert(r.kind==MB_WSD_RESOLVE);assert(!strcmp(r.endpoint,"urn:uuid:bbbbbbbb-cccc-dddd-eeee-ffffffffffff"));
 assert(mb_wsd_parse(outside_body,strlen(outside_body),&r)<0);
 assert(mb_wsd_parse(outside_message,strlen(outside_message),&r)<0);
 assert(mb_wsd_parse(no_envelope,strlen(no_envelope),&r)<0);
 assert(mb_wsd_parse(no_message_id,strlen(no_message_id),&r)<0);
 puts("WSD parser contract OK");return 0;
}
