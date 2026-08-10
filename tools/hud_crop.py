#!/usr/bin/env python3
# Temp HUD crop+upscale helper (pure stdlib zlib) — crop a normalized region of a
# PNG and nearest-neighbor upscale it so small HUD elements are inspectable.
# Usage: hud_crop.py <in.png> <out.png> <nx> <ny> <nw> <nh> [scale]
import sys, zlib, struct

def read_png(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n', "not a png"
    pos = 8; w=h=bd=ct=0; idat=b''
    while pos < len(d):
        ln = struct.unpack('>I', d[pos:pos+4])[0]; typ = d[pos+4:pos+8]
        chunk = d[pos+8:pos+8+ln]
        if typ == b'IHDR':
            w,h,bd,ct = struct.unpack('>IIBB', chunk[:10])
        elif typ == b'IDAT':
            idat += chunk
        elif typ == b'IEND':
            break
        pos += 12 + ln
    assert bd == 8 and ct in (2,6), f"unsupported bd={bd} ct={ct}"
    ch = 3 if ct == 2 else 4
    raw = zlib.decompress(idat)
    stride = w*ch
    out = bytearray(w*h*ch)
    prev = bytearray(stride)
    p = 0
    for y in range(h):
        f = raw[p]; p+=1
        line = bytearray(raw[p:p+stride]); p+=stride
        for x in range(stride):
            a = line[x-ch] if x>=ch else 0
            b = prev[x]
            c = prev[x-ch] if x>=ch else 0
            if f==1: line[x]=(line[x]+a)&255
            elif f==2: line[x]=(line[x]+b)&255
            elif f==3: line[x]=(line[x]+((a+b)>>1))&255
            elif f==4:
                pp=a+b-c; pa=abs(pp-a); pb=abs(pp-b); pc=abs(pp-c)
                pr=a if (pa<=pb and pa<=pc) else (b if pb<=pc else c)
                line[x]=(line[x]+pr)&255
        out[y*stride:(y+1)*stride]=line
        prev=line
    return w,h,ch,out

def write_png(path,w,h,ch,px):
    ct = 2 if ch==3 else 6
    stride=w*ch; raw=bytearray()
    for y in range(h):
        raw.append(0); raw+=px[y*stride:(y+1)*stride]
    comp=zlib.compress(bytes(raw),9)
    def chunk(t,d): return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
    out=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,ct,0,0,0))+chunk(b'IDAT',comp)+chunk(b'IEND',b'')
    open(path,'wb').write(out)

def main():
    a=sys.argv
    inp,outp=a[1],a[2]
    nx,ny,nw,nh=float(a[3]),float(a[4]),float(a[5]),float(a[6])
    scale=int(a[7]) if len(a)>7 else 4
    w,h,ch,px=read_png(inp)
    x0=max(0,int(nx*w)); y0=max(0,int(ny*h))
    x1=min(w,int((nx+nw)*w)); y1=min(h,int((ny+nh)*h))
    cw,ch2=x1-x0,y1-y0
    # crop
    crop=bytearray(cw*ch2*ch)
    for y in range(ch2):
        src=( (y0+y)*w + x0)*ch
        crop[y*cw*ch:(y+1)*cw*ch]=px[src:src+cw*ch]
    # nearest upscale
    sw,sh=cw*scale,ch2*scale
    up=bytearray(sw*sh*ch)
    for y in range(sh):
        sy=y//scale
        for x in range(sw):
            sx=x//scale
            s=(sy*cw+sx)*ch; d=(y*sw+x)*ch
            up[d:d+ch]=crop[s:s+ch]
    write_png(outp,sw,sh,ch,up)
    # quick stats of the cropped region (mean rgb + per-channel)
    n=cw*ch2; r=g=b=0
    for i in range(n):
        r+=crop[i*ch]; g+=crop[i*ch+1]; b+=crop[i*ch+2]
    print(f"crop {cw}x{ch2} @({x0},{y0}) -> {outp} {sw}x{sh}  mean rgb=({r//max(n,1)},{g//max(n,1)},{b//max(n,1)})")

if __name__=='__main__': main()
