#include "../src/minibox-discoveryd/wsd.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void contains(const char *s,const char *needle){assert(strstr(s,needle)!=0);}

int main(void){
 const char *probe="<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" xmlns:p=\"http://schemas.microsoft.com/windows/2006/08/wdp/print\"><s:Header><a:MessageID>urn:uuid:01234567-89ab-cdef-0123-456789abcdef</a:MessageID></s:Header><s:Body><d:Probe><d:Types>x:Other p:PrintDeviceType</d:Types></d:Probe></s:Body></s:Envelope>";
 const char *scan_probe="<s:Envelope><s:Header><a:MessageID>urn:uuid:77777777-2222-3333-4444-555555555555</a:MessageID></s:Header><s:Body><d:Probe><d:Types>scn:ScanDeviceType</d:Types></d:Probe></s:Body></s:Envelope>";
 const char *device_probe="<s:Envelope><s:Header><a:MessageID>urn:uuid:88888888-2222-3333-4444-555555555555</a:MessageID></s:Header><s:Body><d:Probe><d:Types>dp:Device</d:Types></d:Probe></s:Body></s:Envelope>";
 const char *false_probe="<s:Envelope><s:Header><a:MessageID>urn:uuid:22222222-2222-3333-4444-555555555555</a:MessageID></s:Header><s:Body><d:Probe><d:Types>p:NotPrintDeviceType x:PrintDeviceTypeExtra</d:Types></d:Probe></s:Body></s:Envelope>";
 const char *resolve="<s:Envelope><s:Header><a:MessageID>urn:uuid:11111111-2222-3333-4444-555555555555</a:MessageID></s:Header><s:Body><d:Resolve><a:EndpointReference><a:Address>urn:uuid:aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee</a:Address></a:EndpointReference></d:Resolve></s:Body></s:Envelope>";
 const char *header_injection="<s:Envelope><s:Header><a:MessageID>urn:uuid:33333333-2222-3333-4444-555555555555</a:MessageID><d:Probe><d:Types>p:PrintDeviceType</d:Types></d:Probe></s:Header><s:Body><x:Noise/></s:Body></s:Envelope>";
 const char *outside_body="<s:Envelope><s:Header><a:MessageID>urn:uuid:55555555-2222-3333-4444-555555555555</a:MessageID></s:Header></s:Envelope><s:Body><d:Probe><d:Types>p:PrintDeviceType</d:Types></d:Probe></s:Body>";
 struct mb_wsd_request r; char out[8192],message_id[192]; int n;
 const char *endpoint="urn:uuid:aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee";
 const char *xaddr="http://192.168.55.250/cgi-bin/minibox-wsd";
 const char *response_id="urn:uuid:99999999-aaaa-4bbb-8ccc-dddddddddddd";

 assert(mb_wsd_extract_message_id(probe,strlen(probe),message_id,sizeof message_id)==0);
 assert(!strcmp(message_id,"urn:uuid:01234567-89ab-cdef-0123-456789abcdef"));
 assert(mb_wsd_parse(probe,strlen(probe),&r)==0);
 assert(r.kind==MB_WSD_PROBE&&mb_wsd_is_print_probe(&r)&&mb_wsd_probe_supported(&r));
 n=mb_wsd_build_match(&r,endpoint,xaddr,response_id,1,2,out,sizeof out);assert(n>0);
 contains(out,"/ProbeMatches");contains(out,"p:PrintDeviceType");contains(out,"scn:ScanDeviceType");
 contains(out,r.message_id);contains(out,endpoint);contains(out,xaddr);contains(out,"MetadataVersion>1");

 assert(mb_wsd_parse(scan_probe,strlen(scan_probe),&r)==0);
 assert(mb_wsd_is_scan_probe(&r)&&mb_wsd_probe_supported(&r));
 assert(mb_wsd_build_match(&r,endpoint,xaddr,response_id,1,3,out,sizeof out)>0);

 assert(mb_wsd_parse(device_probe,strlen(device_probe),&r)==0);
 assert(mb_wsd_is_device_probe(&r)&&mb_wsd_probe_supported(&r));
 assert(mb_wsd_build_match(&r,endpoint,xaddr,response_id,1,4,out,sizeof out)>0);

 assert(mb_wsd_parse(false_probe,strlen(false_probe),&r)==0);
 assert(!mb_wsd_probe_supported(&r));
 assert(mb_wsd_build_match(&r,endpoint,xaddr,response_id,1,5,out,sizeof out)==0);

 assert(mb_wsd_parse(resolve,strlen(resolve),&r)==0);
 n=mb_wsd_build_match(&r,endpoint,xaddr,response_id,2,6,out,sizeof out);assert(n>0);
 contains(out,"/ResolveMatches");contains(out,r.message_id);contains(out,endpoint);
 assert(mb_wsd_build_match(&r,"urn:uuid:00000000-0000-4000-8000-000000000000",xaddr,response_id,2,7,out,sizeof out)==0);

 assert(mb_wsd_parse(header_injection,strlen(header_injection),&r)<0);
 assert(mb_wsd_parse(outside_body,strlen(outside_body),&r)<0);
 assert(mb_wsd_parse(probe,strlen(probe),&r)==0);
 assert(mb_wsd_build_match(&r,endpoint,xaddr,response_id,1,1,out,64)<0);
 n=mb_wsd_build_metadata_response(r.message_id,endpoint,xaddr,response_id,
                                  "001122334455","http://192.168.55.250/",
                                  out,sizeof out);
 assert(n>0);
 contains(out,"/transfer/GetResponse");
 contains(out,"HP LaserJet M1522n @ MiniBox");
 contains(out,"MFP Printers Scanners");
 contains(out,"001122334455");
 contains(out,"http://192.168.55.250/");
 contains(out,endpoint);

 puts("WSD parse/match contract OK");
 return 0;
}
