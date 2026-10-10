import socket,sys,time
host='127.0.0.1'; port=18631
doc=(b'MiniBox-STREAM-0123456789\n'*4096)
key=b'document-format'; value=b'application/octet-stream'
attrs=(bytes((2,0,0,2,0,0,0,0x33,1,0x49))+len(key).to_bytes(2,'big')+key+
       len(value).to_bytes(2,'big')+value+b'\x03')
# Validated legacy raw path must begin with a printer-language signature.
doc=b'\x1bE'+doc
body=attrs+doc
head=(b'POST /ipp/print HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/ipp\r\nContent-Length: '+str(len(body)).encode()+b'\r\nConnection: close\r\n\r\n')
s=socket.create_connection((host,port))
wire=head+body
for i in range(0,len(wire),997):
    s.sendall(wire[i:i+997])
    if i<5000: time.sleep(0.001)
resp=b''
while True:
    x=s.recv(4096)
    if not x: break
    resp+=x
s.close()
if b'200 OK' not in resp: sys.exit('HTTP response failed')
open('/tmp/large.expected','wb').write(doc)
print(len(doc))
