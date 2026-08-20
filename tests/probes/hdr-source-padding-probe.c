/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2024-present Wired Engine contributors
 *
 * hdr-source-padding-probe — decide ONE question about
 * vk_temporal_resolved_hdr_test.c:355, which fails on Windows and passes on
 * macOS:
 *
 *   does this host's compiler copy a struct's tail padding on assignment?
 *
 * WHY THE QUESTION MATTERS
 * That line does `memcmp(&routed, &current, sizeof(routed))`, and
 * vkHdrPostprocessSource_t has 4 bytes of tail padding (11 four-byte fields
 * after four pointers and two uint64_ts, so the struct rounds up to 96). memcmp
 * over sizeof therefore compares the padding too — but C leaves padding bytes
 * unspecified (C11 6.2.6.1p6), and the production code reaches its output
 * through `source = *current; ... *outSource = source;`, i.e. struct
 * assignment, which is not required to carry padding across.
 *
 * So the test is only portable if every compiler happens to copy padding.
 * Clang on macOS does — measured at -O0/-O1/-O2/-Os, tail comes out 00 00 00 00
 * — which is exactly why the failure does not reproduce there.
 *
 * This probe mirrors the real struct layout and the real code shape (an
 * uninitialised local, assigned in and out) and prints the verdict directly. It
 * needs no engine, no renderer and no Vulkan.
 *
 *   gcc -O2 -o hdrprobe.exe hdr-source-padding-probe.c && ./hdrprobe.exe
 *
 * If it reports padding is NOT copied, the test — not the renderer — is what
 * needs fixing: compare fields, or memset the output before filling it.
 * If it reports padding IS copied, this hypothesis is dead and the failure is
 * something else; run the real test under a debugger and find which CHECK on
 * line 355 fails (the call returning false, or the memcmp).
 */
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
typedef int qboolean; typedef int ralFormat_t;
typedef struct {
  void *backend, *attachment, *postprocessGroup, *histogramGroup;
  uint64_t batchToken, frameId;
  uint32_t commandSlot, frameCount; int32_t worldIndex;
  uint32_t width, height, topologyEpoch, planGeneration;
  uint32_t sceneColorAttachmentGeneration, targetAllocationGeneration;
  ralFormat_t sceneFormat; qboolean resolved;
} S;
static void route(const S *cur, S *out){ S local; local = *cur; *out = local; }
int main(void){
  S cur, routed; unsigned char *p;
  size_t tail = sizeof(S) - (offsetof(S,resolved) + sizeof(qboolean));
  printf("sizeof=%zu tail-padding=%zu bytes\n", sizeof(S), tail);
  memset(&cur,0,sizeof cur); cur.width=640;
  memset(&routed,0xcc,sizeof routed);
  route(&cur,&routed);
  p=(unsigned char*)&routed;
  printf("routed tail bytes: ");
  for (size_t i = offsetof(S,resolved)+sizeof(qboolean); i < sizeof(S); i++) printf("%02x ", p[i]);
  printf("\nmemcmp(routed,cur,sizeof) = %d\n", memcmp(&routed,&cur,sizeof cur));
  printf("\nVERDICT: %s\n", memcmp(&routed,&cur,sizeof cur)==0
     ? "padding IS copied here -> padding hypothesis does NOT explain the failure"
     : "padding NOT copied -> padding hypothesis CONFIRMED (test compares padding via memcmp)");
  return 0;
}
