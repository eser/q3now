// quic-probe — diagnostic client for q3now dedicated server QUIC transport
//
// Usage:
//
//	quic-probe -server 127.0.0.1:27960 -mode observer
//	quic-probe -server 127.0.0.1:27960 -mode player -userinfo '\name\ProbeBot'
//	quic-probe -server 1.2.3.4:27960  -mode observer -token mytoken -v
//
// Observer mode: JSON handshake on stream 0x00, receives state datagrams + events.
// Player  mode: binary TLV on stream 0x00, receives Q3 snapshot datagrams.
//
// Protocol (q3v69 ALPN):
//
//	Stream 0x00  — session control (client-initiated bidi)
//	Stream 0x03  — game events (server-initiated uni, msgpack)
//	Datagrams    — state updates (observer) or Q3 snapshots (player, 8-byte tick header)
//
// TLV format: [type:u8][plen:u16le][payload:plen bytes]
//
//	0x01 CONNECT  C→S  [version:u16le][userinfo_len:u16le][userinfo:bytes]
//	0x02 ACCEPT   S→C  [slot:u8][sv_fps:u8][sv_info_len:u16le]
//	0x03 REFUSE   S→C  [reason_len:u16le][reason:bytes]
//	0x05 READY    C→S  (empty payload)
//
// Auth: LAN mode (empty sv_quicAuthToken) accepts any non-empty token string.
// Default token "probe" works on any LAN server.
package main

import (
	"context"
	"crypto/tls"
	"encoding/binary"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"os/signal"
	"sync/atomic"
	"syscall"
	"time"

	"github.com/quic-go/quic-go"
)

const alpn = "q3v69"

// TLV message types (stream 0x00)
const (
	tlvConnect = 0x01
	tlvAccept  = 0x02
	tlvRefuse  = 0x03
	tlvReady   = 0x05
)

var (
	dgCount atomic.Int64
	evCount atomic.Int64
	flagV   bool
)

func main() {
	server   := flag.String("server", "127.0.0.1:27960", "q3now server host:port")
	mode     := flag.String("mode", "observer", "observer or player")
	token    := flag.String("token", "probe", "auth token (any non-empty value for LAN)")
	userinfo := flag.String("userinfo", `\name\ProbeBot\model\visor`, "player userinfo (player mode)")
	flag.BoolVar(&flagV, "v", false, "verbose: print raw bytes")
	flag.Parse()

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Cancel context on Ctrl+C / SIGTERM
	go func() {
		ch := make(chan os.Signal, 1)
		signal.Notify(ch, syscall.SIGINT, syscall.SIGTERM)
		<-ch
		logf("[probe] shutting down...")
		cancel()
	}()

	tlsCfg := &tls.Config{
		InsecureSkipVerify: true, //nolint:gosec // self-signed cert on game server is expected
		NextProtos:         []string{alpn},
		MinVersion:         tls.VersionTLS13,
	}

	quicCfg := &quic.Config{
		EnableDatagrams: true,
		MaxIdleTimeout:  60 * time.Second,
	}

	logf("[probe] dialing %s (mode=%s token=%q)", *server, *mode, *token)
	conn, err := quic.DialAddr(ctx, *server, tlsCfg, quicCfg)
	if err != nil {
		log.Fatalf("[probe] dial failed: %v", err)
	}
	logf("[probe] QUIC connected (ALPN=%s)", alpn)
	defer conn.CloseWithError(0, "quic-probe done")

	switch *mode {
	case "observer":
		if err := runObserver(ctx, conn, *token); err != nil {
			log.Fatalf("[observer] %v", err)
		}
	case "player":
		if err := runPlayer(ctx, conn, *token, *userinfo); err != nil {
			log.Fatalf("[player] %v", err)
		}
	default:
		log.Fatalf("[probe] unknown mode %q — use observer or player", *mode)
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Observer mode — JSON handshake on stream 0x00
// ─────────────────────────────────────────────────────────────────────────────

func runObserver(ctx context.Context, conn quic.Connection, token string) error {
	stream, err := conn.OpenStreamSync(ctx)
	if err != nil {
		return fmt.Errorf("open stream 0x00: %w", err)
	}

	req := map[string]any{
		"protocol": 69,
		"token":    token,
		"channels": []string{"datagrams", "events"},
	}
	reqBytes, _ := json.Marshal(req)
	logv("[stream0] → %s", reqBytes)
	if _, err := stream.Write(reqBytes); err != nil {
		return fmt.Errorf("stream write: %w", err)
	}

	// Server responds with a single JSON object then keeps stream open.
	buf := make([]byte, 4096)
	n, err := stream.Read(buf)
	if err != nil && err != io.EOF {
		return fmt.Errorf("stream read: %w", err)
	}
	logv("[stream0] ← %s", buf[:n])

	var resp map[string]any
	if jsonErr := json.Unmarshal(buf[:n], &resp); jsonErr != nil {
		logf("[stream0] raw response: %s", buf[:n])
	} else {
		if errMsg, _ := resp["error"].(string); errMsg != "" {
			return fmt.Errorf("server rejected: %s", errMsg)
		}
		printHandshake(resp)
	}

	logf("[observer] connected — listening for datagrams and events (Ctrl+C to stop)")
	go recvDatagrams(ctx, conn)
	go recvEvents(ctx, conn)

	// Block until context cancelled or connection dies
	select {
	case <-ctx.Done():
	case <-conn.Context().Done():
	}
	return nil
}

// ─────────────────────────────────────────────────────────────────────────────
// Player mode — binary TLV on stream 0x00
// ─────────────────────────────────────────────────────────────────────────────

func runPlayer(ctx context.Context, conn quic.Connection, token, userinfo string) error {
	stream, err := conn.OpenStreamSync(ctx)
	if err != nil {
		return fmt.Errorf("open stream 0x00: %w", err)
	}

	// Build TLV 0x01 CONNECT
	// Payload layout: [version:u16le=0x0100][userinfo_len:u16le][userinfo:bytes]
	uiBytes := []byte(userinfo)
	payload := make([]byte, 4+len(uiBytes))
	payload[0] = 0x00
	payload[1] = 0x01 // version 1.0 LE
	binary.LittleEndian.PutUint16(payload[2:], uint16(len(uiBytes)))
	copy(payload[4:], uiBytes)
	connectTLV := tlvEncode(tlvConnect, payload)

	logf("[stream0] → TLV CONNECT (userinfo=%q)", userinfo)
	logv("[stream0] bytes: % x", connectTLV)
	if _, err := stream.Write(connectTLV); err != nil {
		return fmt.Errorf("stream write CONNECT: %w", err)
	}

	// Read TLV ACCEPT or REFUSE
	buf := make([]byte, 512)
	n, err := stream.Read(buf)
	if err != nil && err != io.EOF {
		return fmt.Errorf("stream read: %w", err)
	}
	logv("[stream0] ← % x", buf[:n])

	msgType, tlvPayload, ok := tlvDecode(buf[:n])
	if !ok {
		return fmt.Errorf("malformed TLV response: % x", buf[:n])
	}

	switch msgType {
	case tlvAccept:
		// [slot:u8][sv_fps:u8][sv_info_len:u16le][sv_info:bytes]
		if len(tlvPayload) < 4 {
			return fmt.Errorf("ACCEPT payload too short (%d bytes)", len(tlvPayload))
		}
		slot := int(tlvPayload[0])
		fps  := int(tlvPayload[1])
		logf("[stream0] TLV ACCEPT: slot=%d sv_fps=%d", slot, fps)

		// Send TLV 0x05 READY (empty payload)
		readyMsg := tlvEncode(tlvReady, nil)
		logf("[stream0] → TLV READY")
		if _, err := stream.Write(readyMsg); err != nil {
			return fmt.Errorf("stream write READY: %w", err)
		}

	case tlvRefuse:
		// [reason_len:u16le][reason:bytes]
		reason := tlvParseString(tlvPayload)
		return fmt.Errorf("server REFUSED: %s", reason)

	default:
		return fmt.Errorf("unexpected TLV 0x%02x payload: % x", msgType, tlvPayload)
	}

	logf("[player] ready — listening for snapshots and events (Ctrl+C to stop)")
	go recvDatagrams(ctx, conn)
	go recvEvents(ctx, conn)

	select {
	case <-ctx.Done():
	case <-conn.Context().Done():
	}
	return nil
}

// ─────────────────────────────────────────────────────────────────────────────
// Datagram receiver
//
// Player datagrams: [srv_tick:u32le][base_tick:u32le][Q3 snapshot bytes]
// Observer datagrams: msgpack-encoded state update (no fixed header)
// ─────────────────────────────────────────────────────────────────────────────

func recvDatagrams(ctx context.Context, conn quic.Connection) {
	for {
		dg, err := conn.ReceiveDatagram(ctx)
		if err != nil {
			if ctx.Err() == nil && conn.Context().Err() == nil {
				logf("[datagram] recv error: %v", err)
			}
			return
		}

		n   := dgCount.Add(1)
		ts  := now()
		sz  := len(dg)

		if sz < 8 {
			// Short datagram — likely a msgpack state update (observer) or malformed
			if flagV {
				logf("[datagram #%d %s] %d bytes (short): % x", n, ts, sz, dg)
			} else {
				logf("[datagram #%d %s] %d bytes (no tick header)", n, ts, sz)
			}
			continue
		}

		// Attempt Q3 snapshot parse: first 8 bytes as [srv_tick:u32le][base_tick:u32le]
		srvTick  := binary.LittleEndian.Uint32(dg[0:4])
		baseTick := binary.LittleEndian.Uint32(dg[4:8])
		snapLen  := sz - 8

		if flagV {
			logf("[datagram #%d %s] srv_tick=%-8d  base_tick=%-8d  snap=%d bytes\n            payload: % x",
				n, ts, srvTick, baseTick, snapLen, dg[8:])
		} else {
			logf("[datagram #%d %s] srv_tick=%-8d  base_tick=%-8d  snap=%d bytes",
				n, ts, srvTick, baseTick, snapLen)
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Event stream receiver — server-initiated unidirectional streams (stream 0x03+)
// ─────────────────────────────────────────────────────────────────────────────

func recvEvents(ctx context.Context, conn quic.Connection) {
	for {
		stream, err := conn.AcceptUniStream(ctx)
		if err != nil {
			if ctx.Err() == nil && conn.Context().Err() == nil {
				logf("[events] accept error: %v", err)
			}
			return
		}
		logf("[events] stream %d opened", stream.StreamID())
		go drainEventStream(stream)
	}
}

func drainEventStream(stream quic.ReceiveStream) {
	buf := make([]byte, 65536)
	for {
		n, err := stream.Read(buf)
		if n > 0 {
			num := evCount.Add(1)
			ts  := now()
			if flagV {
				logf("[event #%d %s] %d bytes: % x", num, ts, n, buf[:n])
			} else {
				logf("[event #%d %s] %d bytes (msgpack)", num, ts, n)
			}
		}
		if err != nil {
			if err != io.EOF {
				logf("[events] stream %d closed: %v", stream.StreamID(), err)
			}
			return
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// TLV helpers
// ─────────────────────────────────────────────────────────────────────────────

// tlvEncode produces [type:u8][plen:u16le][payload].
func tlvEncode(msgType byte, payload []byte) []byte {
	plen := uint16(len(payload))
	out  := make([]byte, 3+int(plen))
	out[0] = msgType
	out[1] = byte(plen & 0xFF)
	out[2] = byte(plen >> 8)
	copy(out[3:], payload)
	return out
}

// tlvDecode parses the first TLV from buf.
func tlvDecode(buf []byte) (msgType byte, payload []byte, ok bool) {
	if len(buf) < 3 {
		return 0, nil, false
	}
	plen := int(buf[1]) | (int(buf[2]) << 8)
	if len(buf) < 3+plen {
		return 0, nil, false
	}
	return buf[0], buf[3 : 3+plen], true
}

// tlvParseString reads [len:u16le][bytes] from payload.
func tlvParseString(payload []byte) string {
	if len(payload) < 2 {
		return ""
	}
	slen := int(payload[0]) | (int(payload[1]) << 8)
	if len(payload) < 2+slen {
		slen = len(payload) - 2
	}
	return string(payload[2 : 2+slen])
}

// ─────────────────────────────────────────────────────────────────────────────
// Handshake display
// ─────────────────────────────────────────────────────────────────────────────

func printHandshake(resp map[string]any) {
	proto, _   := resp["protocol"].(float64)
	version, _ := resp["version"].(string)
	granted, _ := resp["granted"].([]any)
	perm, _    := resp["permission"].(map[string]any)
	srv, _     := resp["server"].(map[string]any)

	logf("[handshake] protocol=%.0f  version=%s", proto, version)

	if len(granted) > 0 {
		logf("[handshake] granted channels:")
		for _, g := range granted {
			logf("            • %v", g)
		}
	}
	if perm != nil {
		logf("[handshake] permission: connection=%v  role=%v  authority=%v",
			perm["connection"], perm["role"], perm["authority"])
	}
	if srv != nil {
		logf("[handshake] server:")
		logf("            hostname   = %v", srv["hostname"])
		logf("            map        = %v", srv["map"])
		logf("            gametype   = %v", srv["gametype"])
		logf("            state_rate = %v Hz", srv["state_rate"])
		logf("            event_rate = %v Hz", srv["event_rate"])
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Misc
// ─────────────────────────────────────────────────────────────────────────────

func now() string {
	return time.Now().Format("15:04:05.000")
}

func logf(format string, args ...any) {
	fmt.Printf(format+"\n", args...)
}

func logv(format string, args ...any) {
	if flagV {
		fmt.Printf(format+"\n", args...)
	}
}
