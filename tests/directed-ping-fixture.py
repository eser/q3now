#!/usr/bin/env python3
"""Two-generation loopback responder for the directed /ping contract."""

import argparse, hashlib, json, select, signal, socket, time
OOB=b"\xff"*4

def response(challenge):
    info=(f"\\challenge\\{challenge}\\protocol\\74\\hostname\\EXPIRED A"
          "\\mapname\\arena7\\clients\\1\\sv_maxclients\\8"
          "\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0")
    return OOB+b"infoResponse\n"+info.encode("ascii")

def main():
    ap=argparse.ArgumentParser();ap.add_argument("--events",required=True);ap.add_argument("--timeout",type=float,default=30);args=ap.parse_args()
    stop=False
    def stopped(_s,_f):
        nonlocal stop;stop=True
    signal.signal(signal.SIGTERM,stopped);signal.signal(signal.SIGINT,stopped)
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(("127.0.0.1",0));sock.setblocking(False);port=sock.getsockname()[1]
    with open(args.events,"w",buffering=1) as out:
        began=time.monotonic();scheduled=[];challenges=[]
        def emit(event,**fields):out.write(json.dumps({"event":event,**fields},sort_keys=True)+"\n")
        def wf(w):return {"length":len(w),"sha256":hashlib.sha256(w).hexdigest(),"hex":w.hex()}
        def schedule(delay,scenario,wire,peer,ordinal):scheduled.append((time.monotonic()+delay,scenario,wire,peer,ordinal));scheduled.sort()
        def send(scenario,wire,peer,ordinal):
            sock.sendto(wire,peer);emit("packet_sent",scenario=scenario,request_ordinal=ordinal,source=f"127.0.0.1:{port}",source_port=port,peer=f"{peer[0]}:{peer[1]}",peer_port=peer[1],elapsed_ms=int((time.monotonic()-began)*1000),**wf(wire))
        emit("ready",responder_port=port)
        while not stop and time.monotonic()-began<args.timeout:
            now=time.monotonic()
            while scheduled and scheduled[0][0]<=now:
                _,scenario,wire,peer,ordinal=scheduled.pop(0);send(scenario,wire,peer,ordinal)
            readable,_,_=select.select([sock],[],[],.02)
            if not readable:continue
            wire,peer=sock.recvfrom(65535);body=wire[4:] if wire.startswith(OOB) else wire;parts=body.decode("ascii","replace").split();ordinal=len(challenges)+1
            challenge=parts[1] if len(parts)==2 and parts[0]=="getinfo" else ""
            emit("request",ordinal=ordinal,challenge=challenge,source=f"127.0.0.1:{peer[1]}",source_port=peer[1],peer=f"127.0.0.1:{port}",peer_port=port,elapsed_ms=int((time.monotonic()-began)*1000),**wf(wire))
            if not challenge or ordinal>2:emit("unexpected_request",ordinal=ordinal);continue
            challenges.append(challenge)
            if ordinal==1:schedule(.16,"expired_a",response(challenge),peer,1)
            else:
                schedule(.01,"stale_a_under_b",response(challenges[0]),peer,2)
                schedule(.04,"current_b",response(challenge),peer,2)
        emit("stopped",reason="signal" if stop else "timeout")
    sock.close();return 0
if __name__=="__main__":raise SystemExit(main())
