#include "escl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int text_field(const char *xml,const char *name,char *out,size_t cap){
    char open[64],close[64];const char *a,*b;size_t n;
    if(!xml||!name||!out||cap<2)return -1;
    if(snprintf(open,sizeof open,"<scan:%s>",name)>=(int)sizeof open ||
       snprintf(close,sizeof close,"</scan:%s>",name)>=(int)sizeof close)return -1;
    a=strstr(xml,open);
    if(a){a+=strlen(open);b=strstr(a,close);}
    else{
        if(snprintf(open,sizeof open,"<%s>",name)>=(int)sizeof open ||
           snprintf(close,sizeof close,"</%s>",name)>=(int)sizeof close)return -1;
        a=strstr(xml,open);if(!a)return 1;a+=strlen(open);b=strstr(a,close);
    }
    if(!b)return -1;
    while(a<b&&(*a==' '||*a=='\t'||*a=='\r'||*a=='\n'))a++;
    while(b>a&&(b[-1]==' '||b[-1]=='\t'||b[-1]=='\r'||b[-1]=='\n'))b--;
    n=(size_t)(b-a);if(!n||n>=cap)return -1;
    memcpy(out,a,n);out[n]=0;return 0;
}

int escl_parse_scan_settings(const char *xml,size_t len,struct escl_job *job){
    char *copy,field[64];int r;
    if(!xml||!job||!len||len>65535)return -1;
    copy=malloc(len+1);if(!copy)return -2;
    memcpy(copy,xml,len);copy[len]=0;
    job->source=ESCL_SOURCE_PLATEN;job->dpi=300;job->color=1;\n    job->x_300=0;job->y_300=0;job->width_300=2550;job->height_300=3507;

    r=text_field(copy,"InputSource",field,sizeof field);
    if(r<0){free(copy);return -3;}
    if(r==0){
        if(!strcmp(field,"Platen"))job->source=ESCL_SOURCE_PLATEN;
        else if(!strcmp(field,"Adf")||!strcmp(field,"ADF"))job->source=ESCL_SOURCE_ADF;
        else{free(copy);return -3;}
    }

    r=text_field(copy,"XResolution",field,sizeof field);
    if(r<0){free(copy);return -4;}
    if(r==0){
        char *end=0;unsigned long v=strtoul(field,&end,10);
        if(!end||*end||(v!=200&&v!=300)){free(copy);return -4;}
        job->dpi=(unsigned)v;
    }

    r=text_field(copy,"YResolution",field,sizeof field);
    if(r<0){free(copy);return -4;}
    if(r==0){
        char *end=0;unsigned long v=strtoul(field,&end,10);
        if(!end||*end||v!=job->dpi){free(copy);return -4;}
    }

    {
        static const char *names[]={"XOffset","YOffset","Width","Height"};
        unsigned *values[]={&job->x_300,&job->y_300,&job->width_300,&job->height_300};
        size_t i;
        for(i=0;i<4;i++){
            r=text_field(copy,names[i],field,sizeof field);
            if(r<0){free(copy);return -6;}
            if(r==0){
                char *end=0;unsigned long v=strtoul(field,&end,10);
                if(!end||*end||v>20000){free(copy);return -6;}
                *values[i]=(unsigned)v;
            }
        }
        if(!job->width_300||!job->height_300){free(copy);return -6;}
    }

    r=text_field(copy,"ColorMode",field,sizeof field);
    if(r<0){free(copy);return -5;}
    if(r==0){
        if(!strcmp(field,"RGB24"))job->color=1;
        else if(!strcmp(field,"Grayscale8"))job->color=0;
        else{free(copy);return -5;}
    }
    free(copy);return 0;
}

const char *escl_scanner_capabilities_xml(void){
    static const char xml[]=
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<scan:ScannerCapabilities "
      "xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\" "
      "xmlns:pwg=\"http://www.pwg.org/schemas/2010/12/sm\">"
      "<pwg:Version>2.63</pwg:Version>"
      "<pwg:MakeAndModel>HP LaserJet M1522n @ MiniBox</pwg:MakeAndModel>"
      "<scan:Platen><scan:PlatenInputCaps>"
      "<scan:MinWidth>2550</scan:MinWidth><scan:MaxWidth>2550</scan:MaxWidth>"
      "<scan:MinHeight>3508</scan:MinHeight><scan:MaxHeight>3508</scan:MaxHeight>"
      "<scan:MaxScanRegions>1</scan:MaxScanRegions>"
      "<scan:SettingProfiles><scan:SettingProfile>"
      "<scan:ColorModes><scan:ColorMode>RGB24</scan:ColorMode>"
      "<scan:ColorMode>Grayscale8</scan:ColorMode></scan:ColorModes>"
      "<scan:DocumentFormats><pwg:DocumentFormat>image/jpeg</pwg:DocumentFormat>"
      "</scan:DocumentFormats>"
      "<scan:SupportedResolutions><scan:DiscreteResolutions>"
      "<scan:DiscreteResolution><scan:XResolution>300</scan:XResolution>"
      "<scan:YResolution>300</scan:YResolution></scan:DiscreteResolution>"
      "</scan:DiscreteResolutions></scan:SupportedResolutions>"
      "<scan:ColorSpaces><scan:ColorSpace scan:default=\"true\">sRGB</scan:ColorSpace>"
      "</scan:ColorSpaces>"
      "<scan:ContentTypes><pwg:ContentType>TextAndPhoto</pwg:ContentType>"
      "</scan:ContentTypes>"
      "</scan:SettingProfile></scan:SettingProfiles>"
      "</scan:PlatenInputCaps></scan:Platen>"
      "<scan:Adf><scan:AdfSimplexInputCaps>"
      "<scan:MinWidth>2550</scan:MinWidth><scan:MaxWidth>2550</scan:MaxWidth>"
      "<scan:MinHeight>3508</scan:MinHeight><scan:MaxHeight>3508</scan:MaxHeight>"
      "<scan:MaxScanRegions>1</scan:MaxScanRegions>"
      "<scan:SettingProfiles><scan:SettingProfile>"
      "<scan:ColorModes><scan:ColorMode>RGB24</scan:ColorMode>"
      "<scan:ColorMode>Grayscale8</scan:ColorMode></scan:ColorModes>"
      "<scan:DocumentFormats><pwg:DocumentFormat>image/jpeg</pwg:DocumentFormat>"
      "</scan:DocumentFormats>"
      "<scan:SupportedResolutions><scan:DiscreteResolutions>"
      "<scan:DiscreteResolution><scan:XResolution>300</scan:XResolution>"
      "<scan:YResolution>300</scan:YResolution></scan:DiscreteResolution>"
      "</scan:DiscreteResolutions></scan:SupportedResolutions>"
      "<scan:ColorSpaces><scan:ColorSpace scan:default=\"true\">sRGB</scan:ColorSpace>"
      "</scan:ColorSpaces>"
      "<scan:ContentTypes><pwg:ContentType>TextAndPhoto</pwg:ContentType>"
      "</scan:ContentTypes>"
      "</scan:SettingProfile></scan:SettingProfiles>"
      "</scan:AdfSimplexInputCaps><scan:FeederCapacity>50</scan:FeederCapacity></scan:Adf>"
      "</scan:ScannerCapabilities>\n";
    return xml;
}


const char *escl_scanner_capabilities_xml_identity(const char *uuid,const char *serial,const char *admin_uri){
    static char out[4096];
    const char *base=escl_scanner_capabilities_xml();
    const char *needle="<scan:Platen>";
    const char *p;
    size_t head;
    int n;
    if(!uuid||!*uuid||!serial||!*serial||!admin_uri||!*admin_uri)return base;
    p=strstr(base,needle);
    if(!p)return base;
    head=(size_t)(p-base);
    n=snprintf(out,sizeof out,"%.*s<pwg:SerialNumber>%s</pwg:SerialNumber><pwg:Manufacturer>HP</pwg:Manufacturer><scan:UUID>%s</scan:UUID><scan:AdminURI>%s</scan:AdminURI>%s",
               (int)head,base,serial,uuid,admin_uri,p);
    if(n<0||(size_t)n>=sizeof out)return base;
    return out;
}

const char *escl_scanner_status_xml(int busy,int mfp_online){
    static const char idle[]=
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<scan:ScannerStatus xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\" "
      "xmlns:pwg=\"http://www.pwg.org/schemas/2010/12/sm\">"
      "<pwg:Version>2.63</pwg:Version><pwg:State>Idle</pwg:State>"
      "</scan:ScannerStatus>\n";
    static const char stopped[]=
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<scan:ScannerStatus xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\" "
      "xmlns:pwg=\"http://www.pwg.org/schemas/2010/12/sm\">"
      "<pwg:Version>2.63</pwg:Version><pwg:State>Stopped</pwg:State>"
      "</scan:ScannerStatus>\n";
    static const char processing[]=
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<scan:ScannerStatus xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\" "
      "xmlns:pwg=\"http://www.pwg.org/schemas/2010/12/sm\">"
      "<pwg:Version>2.63</pwg:Version><pwg:State>Processing</pwg:State>"
      "</scan:ScannerStatus>\n";
    return !mfp_online?stopped:(busy?processing:idle);
}
