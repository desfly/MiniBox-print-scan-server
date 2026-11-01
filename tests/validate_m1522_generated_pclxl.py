#!/usr/bin/env python3
"""Independent PCL XL stream parser for MiniBox's captured HP-style raster jobs."""
import pathlib, struct, sys
OPS={0x41:"BeginSession",0x42:"EndSession",0x43:"BeginPage",0x44:"EndPage",0x46:"VendorUnique",0x48:"OpenDataSource",0x49:"CloseDataSource",0x60:"PopGS",0x61:"PushGS",0x69:"SetClipToPage",0x6a:"SetColorSpace",0x6b:"SetCursor",0x75:"SetPageOrigin",0x77:"SetPageScale",0x78:"SetPatternTxMode",0x7b:"SetROP",0x7c:"SetSourceTxMode",0xb0:"BeginImage",0xb1:"ReadImage",0xb2:"EndImage"}
FIXED={0xc0:1,0xc1:2,0xc2:4,0xd1:4,0xd3:4,0xd5:8}
def check(path):
    data=pathlib.Path(path).read_bytes()
    head=data.find(b") HP-PCL XL;")
    if head<0: raise ValueError("no binding")
    p=data.index(b"\n",head)+1
    events=[]; stack=0; active=False; jpegs=[]; ended=False; uel=False
    while p<len(data):
        tag=data[p]; p+=1
        if tag==0x1b:
            if data[p:p+8]!=b"%-12345X": raise ValueError("invalid PJL UEL")
            tail=data[p+8:]
            if tail!=b"@PJL EOJ\n\x1b%-12345X": raise ValueError("unexpected PJL trailer or trailing bytes")
            uel=True
            break
        if tag in FIXED:
            p+=FIXED[tag]
        elif tag==0xf8:
            p+=1
        elif tag==0xc8:
            if data[p]!=0xc0: raise ValueError("invalid array length datatype")
            size=data[p+1]; p+=2+size
        elif tag==0xfa:
            if not active: raise ValueError("embedded JPEG outside image")
            size=struct.unpack_from("<I",data,p)[0]; p+=4
            jpeg=data[p:p+size]
            if len(jpeg)!=size or jpeg[:2]!=b"\xff\xd8" or jpeg[-2:]!=b"\xff\xd9": raise ValueError("invalid JPEG framing")
            if not events or events[-1]!=0xb1: raise ValueError("JPEG must immediately follow ReadImage")
            jpegs.append(size);p+=size
        elif tag in OPS:
            events.append(tag)
            if tag==0x42: ended=True
            if tag==0x61:stack+=1
            if tag==0x60:stack-=1
            if stack<0:raise ValueError("graphics stack underflow")
            if tag==0xb0:
                if active:raise ValueError("nested BeginImage")
                active=True
            if tag==0xb2:
                if not active:raise ValueError("EndImage before BeginImage")
                active=False
        else:raise ValueError("illegal token 0x%02x at byte %d"%(tag,p-1))
        if p>len(data):raise ValueError("truncated data")
    wanted=[0x41,0x48,0x43,0x46,0x77,0x6a,0x78,0x7c,0x7b,0x75,0x61,0x69,0x6b,0x78,0x7c,0x7b,0x6a,0xb0,0xb1,0xb2,0x60,0x44,0x49,0x42]
    if events!=wanted:raise ValueError("HP operator sequence mismatch: "+str([OPS[e] for e in events]))
    if not uel or not ended:raise ValueError("missing final PJL UEL / EndSession")
    if stack or active or len(jpegs)!=1:raise ValueError("graphics state / JPEG count mismatch")
    print("PASS: %s: %d operators, 1 JPEG (%d bytes)"%(path,len(events),jpegs[0]))
if __name__=="__main__":
    try:check(sys.argv[1])
    except (ValueError,IndexError,struct.error) as exc:
        print("FAIL:",exc,file=sys.stderr);sys.exit(1)
