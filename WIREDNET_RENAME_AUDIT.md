# WiredNet Rename Audit

**Purpose**: Complete inventory of QUIC/WebTransport naming that must change
when renaming the networking subsystem to "WiredNet" / `wn_` under `code/wired/net/`.

**Status**: Audit only — no code has been changed.

---

## 1. Files to Move

`code/webtransport/` → `code/wired/net/`

```
grep command: find code/webtransport -type f | sort
Total files: 11
```

| Old path | Proposed new path |
|---|---|
| `code/webtransport/wt_connection.c`     | `code/wired/net/wn_connection.c`     |
| `code/webtransport/wt_events.c`         | `code/wired/net/wn_events.c`         |
| `code/webtransport/wt_http.c`           | `code/wired/net/wn_http.c`           |
| `code/webtransport/wt_local.h`          | `code/wired/net/wn_local.h`          |
| `code/webtransport/wt_main.c`           | `code/wired/net/wn_main.c`           |
| `code/webtransport/wt_mcp.c`            | `code/wired/net/wn_mcp.c`            |
| `code/webtransport/wt_msgpack.c`        | `code/wired/net/wn_msgpack.c`        |
| `code/webtransport/wt_picoquic_stubs.c` | `code/wired/net/wn_picoquic_stubs.c` |
| `code/webtransport/wt_public.h`         | `code/wired/net/wn_public.h`         |
| `code/webtransport/wt_recording.c`      | `code/wired/net/wn_recording.c`      |
| `code/webtransport/wt_transport.c`      | `code/wired/net/wn_transport.c`      |

### `net_transport.h` — stays in `code/qcommon/`

`code/qcommon/net_transport.h` is the engine-wide `transport_t` vtable abstraction.
Five files across `client/`, `server/`, and `webtransport/` include it.
It contains no `wt_`/`WT_`/`QUIC` symbols (only `wn_bootstrap_msg_type_t` /
`wn_download_msg_type_t` which are already WN-prefixed, and `conn_handle_t`,
`reliable_channel_t`, `transport_t`). **It should stay in `code/qcommon/`.**

---

## 2. Public API Functions (QUIC_* prefix)

```
grep command: rg -on "QUIC_[A-Z][A-Za-z_]*" code/ -g "*.c" -g "*.h" | sed 's/.*://' | sort -u
Unique hits: 4
```

> **Finding**: The `QUIC_*` public API layer **no longer exists** in our code.
> All four hits are either embedded inside `PICOQUIC_*` third-party constants
> (grep matched the suffix) or a comment-only stub. See details below.

| Symbol found | Location | Status | Proposed action |
|---|---|---|---|
| `QUIC_ENFORCED_INITIAL_MTU` | `wt_public.h:124` (comment) | Part of `PICOQUIC_ENFORCED_INITIAL_MTU` — third-party, **do not rename** | None |
| `QUIC_ERROR_DETECTED`       | `wt_main.c:227`            | Part of `PICOQUIC_ERROR_DETECTED` — third-party, **do not rename**       | None |
| `QUIC_ERROR_SERVER_BUSY`    | `wt_main.c:127,139`        | Part of `PICOQUIC_ERROR_SERVER_BUSY` — third-party, **do not rename**    | None |
| `QUIC_ProcessMcpChannelMessage` | `sv_client.c:867`      | Comment-only stub (`/* Future: route to QUIC_ProcessMcpChannelMessage */`) | Update comment |

The original `QUIC_*` public API has already been migrated to `WiredNet_*`
functions (47 unique — see §2a below).

### §2a — Current Public API: `WiredNet_*` functions

```
grep command: rg -on "WiredNet_[A-Z][A-Za-z_]*" code/ -g "*.c" -g "*.h" | sed 's/.*://' | sort -u
Unique functions: 47
```

These are the actual public interface between the engine and the transport module.
**Decision needed**: do these become `WN_*` or stay as `WiredNet_*`?

| Current name | Proposed WN_ name |
|---|---|
| `WiredNet_CheckPacket`            | `WN_CheckPacket`            |
| `WiredNet_ClientCheckPacket`      | `WN_ClientCheckPacket`      |
| `WiredNet_ClientClearError`       | `WN_ClientClearError`       |
| `WiredNet_ClientConnect`          | `WN_ClientConnect`          |
| `WiredNet_ClientDisconnect`       | `WN_ClientDisconnect`       |
| `WiredNet_ClientFlushOutbound`    | `WN_ClientFlushOutbound`    |
| `WiredNet_ClientFrame`            | `WN_ClientFrame`            |
| `WiredNet_ClientGetPacket`        | `WN_ClientGetPacket`        |
| `WiredNet_ClientHasError`         | `WN_ClientHasError`         |
| `WiredNet_ClientIsConnecting`     | `WN_ClientIsConnecting`     |
| `WiredNet_ClientRecvReliable`     | `WN_ClientRecvReliable`     |
| `WiredNet_ClientSendPacket`       | `WN_ClientSendPacket`       |
| `WiredNet_ClientSendReady`        | `WN_ClientSendReady`        |
| `WiredNet_CONTROL`                | `WN_CONTROL`                |
| `WiredNet_DrainPendingConnects`   | `WN_DrainPendingConnects`   |
| `WiredNet_DrainPendingReady`      | `WN_DrainPendingReady`      |
| `WiredNet_Emit`                   | `WN_Emit`                   |
| `WiredNet_EmitBotEvent`           | `WN_EmitBotEvent`           |
| `WiredNet_EmitChat`               | `WN_EmitChat`               |
| `WiredNet_EmitDamage`             | `WN_EmitDamage`             |
| `WiredNet_EmitDelag`              | `WN_EmitDelag`              |
| `WiredNet_EmitItemPickup`         | `WN_EmitItemPickup`         |
| `WiredNet_EmitKill`               | `WN_EmitKill`               |
| `WiredNet_EmitMatchEvent`         | `WN_EmitMatchEvent`         |
| `WiredNet_FlushOutbound`          | `WN_FlushOutbound`          |
| `WiredNet_GAME`                   | `WN_GAME`                   |
| `WiredNet_GameAllocConn`          | `WN_GameAllocConn`          |
| `WiredNet_GameFreeConn`           | `WN_GameFreeConn`           |
| `WiredNet_GameHandleDatagram`     | `WN_GameHandleDatagram`     |
| `WiredNet_GameHandleHandshake`    | `WN_GameHandleHandshake`    |
| `WiredNet_GameHandleReliable`     | `WN_GameHandleReliable`     |
| `WiredNet_GetAddrByConnHandle`    | `WN_GetAddrByConnHandle`    |
| `WiredNet_GetConnHandleByAddr`    | `WN_GetConnHandleByAddr`    |
| `WiredNet_GetGamePacket`          | `WN_GetGamePacket`          |
| `WiredNet_HTTP`                   | `WN_HTTP`                   |
| `WiredNet_Init`                   | `WN_Init`                   |
| `WiredNet_ProcessCommandQueue`    | `WN_ProcessCommandQueue`    |
| `WiredNet_ProcessTimers`          | `WN_ProcessTimers`          |
| `WiredNet_PushEvents`             | `WN_PushEvents`             |
| `WiredNet_RegisterCommands`       | `WN_RegisterCommands`       |
| `WiredNet_RequeueConnect`         | `WN_RequeueConnect`         |
| `WiredNet_SendDatagrams`          | `WN_SendDatagrams`          |
| `WiredNet_SendGamePacketToAddr`   | `WN_SendGamePacketToAddr`   |
| `WiredNet_ServerRecvReliable`     | `WN_ServerRecvReliable`     |
| `WiredNet_ServerRecvUsercmd`      | `WN_ServerRecvUsercmd`      |
| `WiredNet_Shutdown`               | `WN_Shutdown`               |
| `WiredNet_Status_f`               | `WN_Status_f`               |

---

## 3. Internal Functions (QT_* prefix)

```
grep command: rg -on "QT_[A-Z][A-Za-z_]*" code/ -g "*.c" -g "*.h" | sed 's/.*://' | sort -u
Unique functions: 30
```

All 30 are `static` functions inside `wt_transport.c`. One comment reference exists
in `cl_main.c:2317`. Proposed convention: lowercase `wn_` (matches C static helper style).

| Current name | Proposed `wn_` name |
|---|---|
| `QT_Connect`                           | `wn_connect`                              |
| `QT_Disconnect`                        | `wn_disconnect`                           |
| `QT_DropClient`                        | `wn_drop_client`                          |
| `QT_Frame`                             | `wn_frame`                                |
| `QT_GetAddressString`                  | `wn_get_address_string`                   |
| `QT_GetBandwidth`                      | `wn_get_bandwidth`                        |
| `QT_GetCnx`                            | `wn_get_cnx`                              |
| `QT_GetGameConn`                       | `wn_get_game_conn`                        |
| `QT_GetLoss`                           | `wn_get_loss`                             |
| `QT_GetPing`                           | `wn_get_ping`                             |
| `QT_Init`                              | `wn_init`                                 |
| `QT_Listen`                            | `wn_listen`                               |
| `QT_RecvReliable`                      | `wn_recv_reliable`                        |
| `QT_RecvUnreliable`                    | `wn_recv_unreliable`                      |
| `QT_ReliableAllocPartial`              | `wn_reliable_alloc_partial`               |
| `QT_ReliableChannelAllowsFixedStream`  | `wn_reliable_channel_allows_fixed_stream` |
| `QT_ReliableChannelDirectionMatches`   | `wn_reliable_channel_direction_matches`   |
| `QT_ReliableChannelIsGame`             | `wn_reliable_channel_is_game`             |
| `QT_ReliableClientConsumeStream`       | `wn_reliable_client_consume_stream`       |
| `QT_ReliableFindPartial`               | `wn_reliable_find_partial`                |
| `QT_ReliableFreePartial`               | `wn_reliable_free_partial`                |
| `QT_ReliableQueuePop`                  | `wn_reliable_queue_pop`                   |
| `QT_ReliableQueuePush`                 | `wn_reliable_queue_push`                  |
| `QT_ReliableServerConsumeStream`       | `wn_reliable_server_consume_stream`       |
| `QT_ReliableStageHeader`               | `wn_reliable_stage_header`                |
| `QT_ReliableStreamIsClientOwned`       | `wn_reliable_stream_is_client_owned`      |
| `QT_ResolveReliableSendStream`         | `wn_resolve_reliable_send_stream`         |
| `QT_SendReliable`                      | `wn_send_reliable`                        |
| `QT_SendUnreliable`                    | `wn_send_unreliable`                      |
| `QT_Shutdown`                          | `wn_shutdown`                             |

---

## 4. Type / Struct Names (wt_* prefix)

```
grep command: rg -on "wt_[a-z][A-Za-z0-9_]*" code/ -g "*.c" -g "*.h" | sed 's/.*://' | sort -u
Unique names: 39
```

Legend: **T** = typedef/struct name, **V** = variable/field name, **M** = module name (no rename needed beyond file move).

| Current name | Type | Proposed `wn_` name |
|---|---|---|
| `wt_client`             | V | `wn_client` (field in `wt_state_s`) |
| `wt_client_state_t`     | T | `wn_client_state_t` |
| `wt_connection`         | V | `wn_connection` (field in global `wt`) |
| `wt_connection_s`       | T | `wn_connection_s` |
| `wt_connection_t`       | T | `wn_connection_t` |
| `wt_datagrams`          | V | `wn_datagrams` (field) |
| `wt_demux`              | V | `wn_demux` (field) |
| `wt_env_map`            | V | `wn_env_map` (field) |
| `wt_event_s`            | T | `wn_event_s` |
| `wt_event_t`            | T | `wn_event_t` |
| `wt_events`             | V | `wn_events` (field) |
| `wt_game`               | V | `wn_game` (field) |
| `wt_game_conn_s`        | T | `wn_game_conn_s` |
| `wt_game_conn_t`        | T | `wn_game_conn_t` |
| `wt_game_hs_state_t`    | T | `wn_game_hs_state_t` |
| `wt_game_pkt_t`         | T | `wn_game_pkt_t` |
| `wt_http`               | V | `wn_http` (field) |
| `wt_local`              | M | (module name; no type, covered by file rename) |
| `wt_main`               | M | (module name; covered by file rename) |
| `wt_mcp`                | V | `wn_mcp` (field) |
| `wt_msgpack`            | M | (module name; covered by file rename) |
| `wt_pending_connect_t`  | T | `wn_pending_connect_t` |
| `wt_picoquic_stubs`     | M | (module name; covered by file rename) |
| `wt_public`             | M | (module name; covered by file rename) |
| `wt_record_event_count` | V | `wn_record_event_count` |
| `wt_record_file`        | V | `wn_record_file` |
| `wt_record_start_time`  | V | `wn_record_start_time` |
| `wt_recording`          | V/M | `wn_recording` |
| `wt_rel_msg_t`          | T | `wn_rel_msg_t` |
| `wt_rel_partial_t`      | T | `wn_rel_partial_t` |
| `wt_snap_pkt_t`         | T | `wn_snap_pkt_t` |
| `wt_state_s`            | T | `wn_state_s` |
| `wt_state_t`            | T | `wn_state_t` |
| `wt_tofu_check`         | V/F | `wn_tofu_check` |
| `wt_tofu_ctx_t`         | T | `wn_tofu_ctx_t` |
| `wt_tofu_file_path`     | V | `wn_tofu_file_path` |
| `wt_tofu_fingerprint`   | V | `wn_tofu_fingerprint` |
| `wt_tofu_save`          | F | `wn_tofu_save` |
| `wt_transport`          | M | (module name; covered by file rename) |

**Note**: The global state singleton `wt` (declared as `static wt_state_t wt` in
`wt_main.c`) should be renamed to `wn` or `g_wn`.

---

## 5. Constants and Macros (WT_* prefix)

```
grep command: rg -on "WT_[A-Z][A-Za-z0-9_]*" code/ -g "*.c" -g "*.h" | sed 's/.*://' | sort -u
Unique symbols: 75
```

Legend: **C** = constant/macro, **F** = internal function, **G** = include guard.

### Constants / Macros

| Current name | Proposed `WN_` name | Note |
|---|---|---|
| `WT_ALPN`              | `WN_ALPN`              | protocol string constant |
| `WT_BALANCE`           | `WN_BALANCE`           | msgpack field constant |
| `WT_EVENT_RING_SIZE`   | `WN_EVENT_RING_SIZE`   | ring buffer size |
| `WT_GAME_HS_ACCEPTED`  | `WN_GAME_HS_ACCEPTED`  | handshake state enum value |
| `WT_GAME_HS_NONE`      | `WN_GAME_HS_NONE`      | handshake state enum value |
| `WT_GAME_HS_PENDING`   | `WN_GAME_HS_PENDING`   | handshake state enum value |
| `WT_GAME_HS_REFUSED`   | `WN_GAME_HS_REFUSED`   | handshake state enum value |
| `WT_GAME_PKT_MAX`      | `WN_GAME_PKT_MAX`      | max datagram packet size |
| `WT_GAME_QUEUE_SIZE`   | `WN_GAME_QUEUE_SIZE`   | inbound datagram slots per player |
| `WT_GAME_REL_VERSION`  | `WN_GAME_REL_VERSION`  | reliable protocol version |
| `WT_LOCAL_H`           | `WN_LOCAL_H`           | G — include guard |
| `WT_MAX_CLIENTS`       | `WN_MAX_CLIENTS`       | max simultaneous connections |
| `WT_MCP_JSON_BUF_SIZE` | `WN_MCP_JSON_BUF_SIZE` | MCP JSON buffer limit |
| `WT_PACKET_BUF_SIZE`   | `WN_PACKET_BUF_SIZE`   | per-connection packet buffer |
| `WT_PERM_`             | `WN_PERM_`             | permission bitmask prefix |
| `WT_PERM_ADMIN`        | `WN_PERM_ADMIN`        | |
| `WT_PERM_LEADER`       | `WN_PERM_LEADER`       | |
| `WT_PERM_OBSERVER_LEADER_ADMIN`   | `WN_PERM_OBSERVER_LEADER_ADMIN`   | |
| `WT_PERM_OBSERVER_MEMBER_USER`    | `WN_PERM_OBSERVER_MEMBER_USER`    | |
| `WT_PERM_PLAYER`       | `WN_PERM_PLAYER`       | |
| `WT_PERM_PLAYER_LEADER_ADMIN`     | `WN_PERM_PLAYER_LEADER_ADMIN`     | |
| `WT_PERM_PLAYER_MEMBER_USER`      | `WN_PERM_PLAYER_MEMBER_USER`      | |
| `WT_PUBLIC_H`          | `WN_PUBLIC_H`          | G — include guard |
| `WT_RECORD_DIR`        | `WN_RECORD_DIR`        | event-recording output dir |
| `WT_RECORD_MAGIC`      | `WN_RECORD_MAGIC`      | `.q3events` file magic bytes |
| `WT_RECORD_VERSION`    | `WN_RECORD_VERSION`    | `.q3events` format version |
| `WT_REL_PARTIAL_CAP`   | `WN_REL_PARTIAL_CAP`   | max in-flight partial streams |
| `WT_REL_QUEUE_SIZE`    | `WN_REL_QUEUE_SIZE`    | reliable receive queue depth |
| `WT_SNAP_DGRAM_MAX`    | `WN_SNAP_DGRAM_MAX`    | snapshot datagram buffer size (currently an alias for `WN_DATAGRAM_MTU`) |

### Internal functions using WT_ prefix (module-private)

| Current name | Proposed `WN_` name |
|---|---|
| `WT_AllocConnection`              | `WN_AllocConnection`              |
| `WT_AlpnSelectCallback`           | `WN_AlpnSelectCallback`           |
| `WT_ClientCallback`               | `WN_ClientCallback`               |
| `WT_EncodeBotEvent`               | `WN_EncodeBotEvent`               |
| `WT_EncodeChatEvent`              | `WN_EncodeChatEvent`              |
| `WT_EncodeDamageEvent`            | `WN_EncodeDamageEvent`            |
| `WT_EncodeItemPickupEvent`        | `WN_EncodeItemPickupEvent`        |
| `WT_EncodeKillEvent`              | `WN_EncodeKillEvent`              |
| `WT_EncodeMatchEvent`             | `WN_EncodeMatchEvent`             |
| `WT_EncodeStateUpdate`            | `WN_EncodeStateUpdate`            |
| `WT_EventRingPush`                | `WN_EventRingPush`                |
| `WT_EventRingRead`                | `WN_EventRingRead`                |
| `WT_FindConnection`               | `WN_FindConnection`               |
| `WT_FreeConnection`               | `WN_FreeConnection`               |
| `WT_GameFindConnForAddr`          | `WN_GameFindConnForAddr`          |
| `WT_HandleCapabilityNegotiation`  | `WN_HandleCapabilityNegotiation`  |
| `WT_HasPermAdmin`                 | `WN_HasPermAdmin`                 |
| `WT_HasPermLeader`                | `WN_HasPermLeader`                |
| `WT_HasPermPlayer`                | `WN_HasPermPlayer`                |
| `WT_HttpHandleHealth`             | `WN_HttpHandleHealth`             |
| `WT_HttpHandleMetrics`            | `WN_HttpHandleMetrics`            |
| `WT_HttpHandleRequest`            | `WN_HttpHandleRequest`            |
| `WT_HttpSendResponse`             | `WN_HttpSendResponse`             |
| `WT_IsQuicPacket`                 | `WN_IsWiredNetPacket`             |
| `WT_JsonFindInt`                  | `WN_JsonFindInt`                  |
| `WT_JsonFindString`               | `WN_JsonFindString`               |
| `WT_LogConnect`                   | `WN_LogConnect`                   |
| `WT_LogDisconnect`                | `WN_LogDisconnect`                |
| `WT_McpHandleBotGetState`         | `WN_McpHandleBotGetState`         |
| `WT_McpHandleBotList`             | `WN_McpHandleBotList`             |
| `WT_McpHandleBotSetSkill`         | `WN_McpHandleBotSetSkill`         |
| `WT_McpHandleEventHistory`        | `WN_McpHandleEventHistory`        |
| `WT_McpHandleGameStatus`          | `WN_McpHandleGameStatus`          |
| `WT_McpHandleInitialize`          | `WN_McpHandleInitialize`          |
| `WT_McpHandleMessage`             | `WN_McpHandleMessage`             |
| `WT_McpHandleToolsList`           | `WN_McpHandleToolsList`           |
| `WT_McpRespond`                   | `WN_McpRespond`                   |
| `WT_MsgpackEncodeVec3`            | `WN_MsgpackEncodeVec3`            |
| `WT_NetadrToSockaddr`             | `WN_NetadrToSockaddr`             |
| `WT_PicoquicCallback`             | `WN_PicoquicCallback`             |
| `WT_RecordEvent`                  | `WN_RecordEvent`                  |
| `WT_RecordInit`                   | `WN_RecordInit`                   |
| `WT_RecordShutdown`               | `WN_RecordShutdown`               |
| `WT_RecordStart`                  | `WN_RecordStart`                  |
| `WT_RecordStop`                   | `WN_RecordStop`                   |
| `WT_WeaponShortName`              | `WN_WeaponShortName`              |

---

## 6. Feature Flags

```
grep command: rg -n "FEAT_QUIC|FEAT_WEBTRANSPORT|USE_QUIC|USE_WEBTRANSPORT" code/ CMakeLists.txt
Result: 0 matches
```

**No compile-time feature flags found.** The transport is unconditionally compiled in.
No `FEAT_*` or `USE_*` macros need to be renamed or removed.

---

## 7. Cvar Names (Runtime)

```
grep command: rg -n "sv_quic|wn_debug|wn_cert|\"quic" code/ -g "*.c" -g "*.h"
```

### Already renamed (keep as-is)

| Cvar | File | Status |
|---|---|---|
| `wn_debug`       | `wt_main.c:282`, `wt_local.h:27`, `wt_public.h:38` | ✅ Already WN — keep |
| `wn_cert_verify` | `wt_transport.c:1285-1286`                          | ✅ Already WN — keep |

### Needs renaming

| Current cvar | Registered in | Proposed new name |
|---|---|---|
| `sv_quicAuthToken`  | `wt_main.c:278`, `common.c:3745`       | `sv_wirednetAuthToken`  |
| `sv_quicMaxClients` | `wt_main.c:279`, `common.c:3746`       | `sv_wirednetMaxClients` |
| `sv_quicStateRate`  | `wt_main.c:280`, `common.c:3747`       | `sv_wirednetStateRate`  |
| `sv_quicEventRate`  | `wt_main.c:281`, `common.c:3748`       | `sv_wirednetEventRate`  |
| `sv_quicRecord`     | `wt_recording.c:34,48`, `common.c:3749`| `sv_wirednetRecord`     |
| `sv_quicCertFile`   | `wt_main.c:292,317`                    | `sv_wirednetCertFile`   |
| `sv_quicKeyFile`    | `wt_main.c:293,318`                    | `sv_wirednetKeyFile`    |

### Environment variables (also needs renaming)

| Current env var | Registered in | Proposed new name |
|---|---|---|
| `Q3_QUIC_CERT`          | `README.md:264`    | `Q3_WIREDNET_CERT`          |
| `Q3_QUIC_KEY`           | `README.md:265`    | `Q3_WIREDNET_KEY`           |
| `Q3_SV_QUIC`            | `common.c:3720`    | `Q3_SV_WIREDNET` (or drop)  |
| `Q3_SV_QUICAUTHTOKEN`   | `common.c:3721`    | `Q3_SV_WIREDNETAUTHTOKEN`   |
| `SV_QUICAUTHTOKEN`      | `common.c:3745`    | `SV_WIREDNETAUTHTOKEN`      |
| `SV_QUICMAXCLIENTS`     | `common.c:3746`    | `SV_WIREDNETMAXCLIENTS`     |
| `SV_QUICSTATERATE`      | `common.c:3747`    | `SV_WIREDNETSTATERATE`      |
| `SV_QUICEVENTRATE`      | `common.c:3748`    | `SV_WIRDNETEVENTRATE`       |
| `SV_QUICRECORD`         | `common.c:3749`    | `SV_WIREDNETRECORD`         |

**Note**: The `"quic:%s"` / `"quic6:%s"` format strings in `net_ip.c:619,630,661`
are **protocol address prefixes** visible to users and configs. These should change
to `"wirednet:%s"` / `"wirednet6:%s"`.

---

## 8. Include Paths

```
grep command: rg -n '#include.*wt_|#include.*webtransport' code/ -g "*.c" -g "*.h"
Total includes to update: 19
```

### External callers (9 files — paths break when `webtransport/` moves)

| File | Line | Current include | New include |
|---|---|---|---|
| `code/client/cl_input.c`      | 26 | `"../webtransport/wt_public.h"` | `"../wired/net/wn_public.h"` |
| `code/client/cl_main.c`       | 29 | `"../webtransport/wt_public.h"` | `"../wired/net/wn_public.h"` |
| `code/client/cl_parse.c`      | 25 | `"../webtransport/wt_public.h"` | `"../wired/net/wn_public.h"` |
| `code/qcommon/net_ip.c`       | 27 | `"../webtransport/wt_public.h"` | `"../wired/net/wn_public.h"` |
| `code/server/sv_client.c`     | 25 | `"../webtransport/wt_public.h"` | `"../wired/net/wn_public.h"` |
| `code/server/sv_game.c`       | 27 | `"../webtransport/wt_public.h"` | `"../wired/net/wn_public.h"` |
| `code/server/sv_init.c`       | 26 | `"../webtransport/wt_public.h"` | `"../wired/net/wn_public.h"` |
| `code/server/sv_main.c`       | 26 | `"../webtransport/wt_public.h"` | `"../wired/net/wn_public.h"` |
| `code/server/sv_snapshot.c`   | 24 | `"../webtransport/wt_public.h"` | `"../wired/net/wn_public.h"` |

### Within the webtransport module (10 includes — internal; auto-fixed by file rename + content rewrite)

| File | Line | Current include | New include |
|---|---|---|---|
| `wt_local.h`         | 11 | `"wt_public.h"`  | `"wn_public.h"`  |
| `wt_connection.c`    |  6 | `"wt_local.h"`   | `"wn_local.h"`   |
| `wt_events.c`        | 10 | `"wt_local.h"`   | `"wn_local.h"`   |
| `wt_http.c`          | 16 | `"wt_local.h"`   | `"wn_local.h"`   |
| `wt_main.c`          |  9 | `"wt_local.h"`   | `"wn_local.h"`   |
| `wt_mcp.c`           | 22 | `"wt_local.h"`   | `"wn_local.h"`   |
| `wt_msgpack.c`       | 12 | `"wt_local.h"`   | `"wn_local.h"`   |
| `wt_recording.c`     | 23 | `"wt_local.h"`   | `"wn_local.h"`   |
| `wt_transport.c`     | 21 | `"wt_local.h"`   | `"wn_local.h"`   |
| `wt_transport.c`     | 22 | `"../qcommon/net_transport.h"` | unchanged |

---

## 9. CMakeLists.txt References

```
grep command: grep -n "webtransport\|wt_\|QUIC" CMakeLists.txt
Matching lines: 7
```

| Line | Current content | Required change |
|---|---|---|
| 134 | `FILE(GLOB PICOQUIC_SRCS src/libs/picoquic/picoquic/*.c)` | Keep — picoquic path unchanged |
| 137 | `list(FILTER PICOQUIC_SRCS EXCLUDE REGEX ".*minicrypto.*")` | Keep |
| 138 | `list(FILTER PICOQUIC_SRCS EXCLUDE REGEX ".*mbedtls.*")` | Keep |
| 142 | `# Our wt_picoquic_stubs.c provides a no-op stub instead.` | Update comment to `wn_picoquic_stubs.c` |
| 143 | `list(FILTER PICOQUIC_SRCS EXCLUDE REGEX ".*fusion.*")` | Keep |
| 146 | `list(FILTER PICOQUIC_SRCS EXCLUDE REGEX ".*qlog.*")` | Keep |
| 161 | `# webtransport module — our integration code` | `# wired/net module — WiredNet integration` |
| 162 | `FILE(GLOB WT_SRCS code/webtransport/*.c)` | `FILE(GLOB WN_SRCS code/wired/net/*.c)` |
| 164 | `SET(WIREDNET_ALL_SRCS ${PICOQUIC_SRCS} ${PICOTLS_SRCS} ${MPACK_SRCS} ${WT_SRCS})` | Replace `${WT_SRCS}` with `${WN_SRCS}` |
| 171 | `code/webtransport` (include dir) | `code/wired/net` |

---

## 10. Documentation References

```
grep command: rg -c "webtransport|QUIC_|wt_" docs/ README.md BUILD.md -g "*.md"
```

| File | Match count | Action |
|---|---|---|
| `docs/Q3NETCODE_VS_WIREDNET.md` | 74 | Heavy — all `code/webtransport/wt_*.c` citations need updating to `code/wired/net/wn_*.c`; `QUIC_` references in prose; `WT_REL_PARTIAL_CAP`, `WT_REL_QUEUE_SIZE`, `WT_GAME_QUEUE_SIZE` constants |
| `README.md`                     |  3 | `Q3_QUIC_CERT` / `Q3_QUIC_KEY` env var table (lines 254, 264, 265); cvar `sv_quicCertFile`/`sv_quicKeyFile` |
| `docs/command-audit.md`         |  2 | `code/webtransport/wt_main.c` path (line 252); `QUIC_Status_f` function name (line 253) |

---

## 11. Collisions / Conflicts

### Existing `wn_*` / `WN_*` symbols (already in codebase — no rename needed)

The following already carry the `WN_` prefix and **must not be touched** during
the rename. None of them clash with proposed new names.

**Protocol / message constants** (in `code/qcommon/net_transport.h` and elsewhere):
`WN_BOOTSTRAP_MSG_STATE`, `WN_BOOTSTRAP_SEC_ACK`, `WN_BOOTSTRAP_SEC_BASELINES`,
`WN_BOOTSTRAP_SEC_CLIENT_INFO`, `WN_BOOTSTRAP_SEC_CONFIGSTRINGS`,
`WN_BOOTSTRAP_SEC_SERVER_CMDS`, `WN_DOWNLOAD_MSG_BLOCK`, `WN_DOWNLOAD_MSG_ERROR`,
`WN_DATAGRAM_MTU`, `WN_FRAG_PAYLOAD`, `WN_QUEUE_DEPTH`

**Spawn-phase / connection-state constants**: `WN_P1_TEARDOWN_SETUP`,
`WN_P2_BSP_LOAD`, `WN_P3_GAME_VM_INIT`, `WN_P4_SETTLE_BASELINE`,
`WN_P5_RECONNECT_FINALIZE`, `WN_CONTEXT_RESET`, `WN_CONTEXT_RESET_ARB`,
`WN_IDLE`, `WN_ERROR`

**MsgPack field tags**: `WN_AMMO`, `WN_ARMOR`, `WN_BALANCE`, `WN_COUNT`,
`WN_DBG`, `WN_DELAY_ARMOR`, `WN_DELAY_HOLDABLE`, `WN_DELAY_POWERUP`,
`WN_EXT`, `WN_HEALTH`, `WN_HOLDABLE`, `WN_LENGTH`, `WN_NV`, `WN_POINTS`,
`WN_POWERUP`, `WN_PROTECT`, `WN_PROTECTION`, `WN_PSHADOWS`, `WN_VARS`,
`WN_VARS_CHARS`, `WN_VAR_CHARS`, `WN_WEAPON`

**Type names** (in `net_transport.h`):
`wn_bootstrap_msg_type_t`, `wn_bootstrap_section_t`, `wn_download_msg_type_t`

**Cvars**: `wn_debug`, `wn_cert_verify`

### `code/wired/net/` — clean slate

No `code/wired/` directory exists. The target path is unoccupied. ✅

### picoquic third-party `QUIC_*` symbols — do NOT rename

The picoquic library headers under `src/libs/picoquic/` define hundreds of
`QUIC_*` constants (`QUIC_ACK_DELAY_MAX`, `QUIC_BBR`, `QUIC_BYTESTREAM_H`, etc.).
These are third-party symbols. Any automated sed/replace that targets bare `QUIC_`
must be scoped to `code/` only and must explicitly exclude `PICOQUIC_` prefix.

### Potential name conflict: `WT_SNAP_DGRAM_MAX` → `WN_SNAP_DGRAM_MAX`

`WT_SNAP_DGRAM_MAX` is currently defined as `#define WT_SNAP_DGRAM_MAX WN_DATAGRAM_MTU`
(an alias). When renamed, it becomes `#define WN_SNAP_DGRAM_MAX WN_DATAGRAM_MTU`.
No collision — `WN_SNAP_DGRAM_MAX` does not yet exist in the codebase.

### `WT_IsQuicPacket` — rename to `WN_IsWiredNetPacket`

The name `WT_IsQuicPacket` tests whether a packet came from the QUIC transport.
The proposed rename `WN_IsQuicPacket` would preserve the protocol name, but
since picoquic headers also use `QUIC_*` that may cause visual confusion.
**Recommendation**: rename to `WN_IsWiredNetPacket`.

---

## Summary Counts

| Category | Count |
|---|---|
| Files to move                             | 11    |
| `WiredNet_*` public API functions         | 47    |
| `QT_*` internal static functions          | 30    |
| `wt_*` type / struct / variable names     | 39    |
| `WT_*` macros, constants, functions       | 75    |
| Feature flags (`FEAT_*` / `USE_*`)        | 0     |
| Cvars to rename                           | 7     |
| Env vars to rename                        | 9     |
| Broken `#include` paths                   | 19    |
| CMakeLists.txt lines to update            | 4     |
| Doc files with old references             | 3     |
| **Total touch-points**                    | **~240** |
