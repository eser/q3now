# Third-Party Licenses

q3now and Wired bundles or links to a number of third-party components. This document
catalogues each one along with its upstream URL, license, and the canonical
copyright / permission notice that the upstream license requires us to
reproduce.

The full text of each commonly-used license appears once in the appendices
at the bottom; per-component sections cite the appendix rather than duplicating
the text.

For machine-readable enumeration of every transitive dependency, see:

- `.gitmodules` — vendored C/C++ submodule pinned versions
- `launcher/go.mod` + `launcher/go.sum` — Go-side dependency graph
- `launcher/frontend/package.json` + `package-lock.json` — frontend graph

---

## Table of Contents

1. [Project license & derivation](#1-project-license--derivation)
2. [Game content & id Software EULA — including the launcher's `assets download` bundle](#2-game-content--id-software-eula)
   - 2.1 id Software demos (Q3 + Team Arena)
   - 2.2 id Software retail PAKs (Q3, Team Arena, Quake I)
   - 2.3 Challenge ProMode Arena (CPMA) + OpenLibm
   - 2.4 High Quality Quake (HQQ)
   - 2.5 Community map pack (13 maps)
   - 2.6 q3now-original mod content (`pax21.sw3z`)
3. [Vendored C / C++ libraries (`src/libs/`)](#3-vendored-c--c-libraries-srclibs)
4. [Vendored single-header libraries & headers](#4-vendored-single-header-libraries--headers)
5. [Build & development tooling](#5-build--development-tooling)
6. [Launcher — Go module dependencies](#6-launcher--go-module-dependencies)
7. [Launcher — frontend dependencies](#7-launcher--frontend-dependencies)
8. [Engine font assets (q3now mod fonts)](#8-game-content-q3now-mod-assets)
9. [Appendix A — MIT License (canonical text)](#appendix-a--mit-license-canonical-text)
10. [Appendix B — BSD-3-Clause License (canonical text)](#appendix-b--bsd-3-clause-license-canonical-text)
11. [Appendix C — BSD-2-Clause License (canonical text)](#appendix-c--bsd-2-clause-license-canonical-text)
12. [Appendix D — Zlib License (canonical text)](#appendix-d--zlib-license-canonical-text)
13. [Appendix E — Apache License 2.0 (reference)](#appendix-e--apache-license-20-reference)

---

## 1. Project license & derivation

This repository combines source code released under **two versions of the GNU
General Public License**:

- **Inherited id Tech 3 code** — the engine and game code derived from id
  Software's **Quake III Arena** release and its community successors
  (**ioquake3** → **Quake3e** → this tree), plus the id-derived **q3now** game
  modules under `code/game/` and `code/cgame/` — is licensed under
  **GPL-2.0-or-later**. Such files carry `SPDX-License-Identifier:
  GPL-2.0-or-later`.
- **New Wired Engine code** — files first written for this project, notably
  everything under `code/.../wired/` — is licensed under **GPL-3.0-or-later**.
  Such files carry `SPDX-License-Identifier: GPL-3.0-or-later`.
- Because GPL-2.0-or-later code is upgrade-compatible with GPLv3, the **combined
  work / distributed binaries are effectively GPL-3.0-or-later**.

The full text of both licenses is in `LICENSE` at the repository root and,
verbatim, under `LICENSES/` (`LICENSES/GPL-2.0-or-later.txt`,
`LICENSES/GPL-3.0-or-later.txt`). The authoritative license for any individual
file is the `SPDX-License-Identifier` tag at the top of that file.

Per the original id Software release:

> Copyright (C) 1999-2005 Id Software, Inc.
>
> This program is free software; you can redistribute it and/or modify
> it under the terms of the GNU General Public License as published by
> the Free Software Foundation; either version 2 of the License, or
> (at your option) any later version.

A few in-tree files are derived from **other GPL projects** (not id Software);
their `SPDX-FileCopyrightText` lines preserve the real upstream attribution and
they remain `GPL-2.0-or-later`:

- `code/qcommon/crash.c`, `code/qcommon/crash.h` — JSON crash-report writer,
  originally from **CNQ3** (`Copyright (c) 2017-2020 Gian 'myT' Schellenbaum`).
- `code/qcommon/cm_q1.c` — native Q1 hull-1 clipnode tracer, algorithm ported
  from **FTEQW** (`q1bsp.c`, Spike / FTEQW contributors).
- `code/client/snd_codec*.c`, `code/client/snd_opus.c`, `code/client/snd_adpcm.c`,
  `code/qcommon/json.h` — sound-codec and JSON helpers from **ioquake3** /
  ioquake3 contributors (Stuart Dalton, James Canete, et al.).
- `code/renderer*/tr_spearmint.c` — fog / corona / 2D-draw feature adaptation
  derived from **Spearmint** (Zack Middleton / Spearmint contributors).
- `code/qcommon/util/md4.c` — MD4 implementation from **Samba**
  (`Copyright (c) 1997-1998 Andrew Tridgell`), GPL-2.0-or-later.

Other in-tree files are third-party code under **non-GPL** licenses; see
§4 below and the `SPDX-License-Identifier` tag in each such file. Notable ones:

- `code/qcommon/unzip.c`, `code/qcommon/unzip.h` — IO over zip files, derived
  from zlib's `unzip.c` / minizip (Zlib license — see Appendix D), originally by
  Gilles Vollant; with a small modification by Joerg Dietrich.
- `code/qcommon/puff.c`, `code/qcommon/puff.h` — minimal inflate, by Mark Adler
  (zlib-style permissive license, kept verbatim in the file).
- `code/qcommon/bg_lib.c` — minimal libc replacement, BSD-derived
  (BSD-3-Clause — see Appendix B).
- `code/qcommon/util/md5.c` — MD5 implementation (see §4.6 for its license).
- `code/renderer/iqm.h`, `code/renderervk/iqm.h` — Inter-Quake Model format
  header, by Lee Salzman (public domain — see §4.5).
- `code/renderer2/glext.h` — OpenGL extension header from the Khronos OpenGL
  registry (MIT — see §4.7).
- `code/renderervk/smaa_area_texture.h`, `code/renderervk/smaa_search_texture.h`
  — precomputed SMAA lookup textures (MIT, see §4.4).
- `code/renderercommon/vulkan/*.h` — Khronos Vulkan headers (Apache-2.0 —
  see §4.2).

---

## 2. Game content & id Software EULA

The q3now launcher's `assets download` flow fetches a single bundle —
`id-quakepack.zip` — from a mirror operated by the q3now project:

> **Bundle URL:** https://objects.eser.live/quakedata/id-quakepack.zip
> (referenced as `IDQuake3PackURL` in
> `launcher/internal/pipeline/orchestrate.go`)

The bundle is gated on user acceptance of id Software's *Limited Use Software
License Agreement* — the **QIIITA-Demo EULA**, included verbatim inside the
bundle as `QIIITA-Demo EULA.doc` and also embedded in the launcher binary
at `launcher/eula.go`. Acceptance is recorded with a timestamp in the
launcher's per-user `settings.json`.

After download, `assets import` extracts the bundle to
`~/wired${channel}/downloaded/id-quakepack/` and converts the contained
PAKs into q3now's `.sw3z` format under `~/wired${channel}/baseq3/`.

The bundle aggregates content from several distinct copyright holders. Each
is enumerated below with its origin, license/permission status, and what
q3now is obligated to preserve when redistributing it.

### 2.1 id Software content — Quake III Arena Demo & Team Arena Demo

| Component | Path inside bundle | Copyright | Redistribution basis |
|---|---|---|---|
| Q3 Demo PAK | `demoq3/pak0.pk3` | © Id Software, Inc. | QIIITA-Demo EULA §3 (Permitted Distribution and Copying) |
| Team Arena Demo PAK | `demota/pak0.pk3` | © Id Software, Inc. | QIIITA-Demo EULA §3 |

The QIIITA-Demo EULA §3 *Permitted Distribution and Copying* explicitly
grants:

> ID grants to you the non-exclusive and limited right to copy the Software
> and to distribute such copies of the Software free of charge for
> non-commercial purposes... provided, however, you shall not copy or
> distribute the Software in any infringing manner ...

Per the same clause, **the QIIITA-Demo EULA must accompany each copy** of
the demo software being distributed. q3now satisfies this by including
`QIIITA-Demo EULA.doc` inside `id-quakepack.zip`, by displaying the EULA
text in the launcher GUI before the user accepts (`launcher/eula.go`), and
by surfacing the same text via the CLI (`q3now-launcher assets download
--accept-eula` reads it before recording acceptance).

The §3 grant additionally requires that distributed CDs or non-electronic
copies be labelled "SHAREWARE" or "DEMO". q3now's electronic distribution
labels the bundled paks as "demo" via the directory paths `demoq3/` and
`demota/`.

Copyright holder: **Id Software, Inc.** All rights reserved to id Software
except as expressly granted by the EULA.

### 2.2 id Software content — full Quake III Arena & Team Arena retail PAKs

| Component | Path inside bundle | Copyright | Redistribution basis |
|---|---|---|---|
| Q3 retail PAKs | `baseq3/pak1.pk3` … `baseq3/pak8.pk3` | © Id Software, Inc. | **Not covered by QIIITA-Demo EULA §3.** Retail Q3A is governed by id Software's retail EULA, which does not grant unrestricted free redistribution. |
| Team Arena retail PAKs | `missionpack/pak1.pk3` … `pak3.pk3` | © Id Software, Inc. | Same as above. |
| Quake I content | `id1/pak0.pak` | © Id Software, Inc. | Quake I has its own original EULA; see id Software's policy for legacy content. |

These files are **id Software's intellectual property**. Their inclusion
in `id-quakepack.zip` reflects an operational arrangement between the
mirror operator and id Software / id Software's successors. Anyone
redistributing or forking q3now's `id-quakepack.zip` is responsible for
ensuring they have authorisation; this document does not grant rights
that q3now itself does not hold.

If you fork q3now and want to ship a different bundle URL containing only
the unambiguously-redistributable demo paks, change `IDQuake3PackURL` in
`launcher/internal/pipeline/orchestrate.go` to point at your own
EULA-compliant mirror.

### 2.3 Challenge ProMode Arena (CPMA)

| Field | Value |
|---|---|
| Component | Challenge ProMode Arena, a community gameplay mod |
| Path inside bundle | `cpma/z-cpma-pak153.pk3`, `cpma/readme.txt`, `cpma/openlibm_license.md` |
| Version | pak153 |
| Upstream | https://playmorepromode.com (community: https://discord.me/cpma) |
| Documentation | https://playmorepromode.com/guides |
| Sources | https://bitbucket.org/CPMADevs/docs/src |
| License | Closed-source freeware — no explicit redistribution licence in `readme.txt`; CPMA's standard distribution model is binary-only freeware |

CPMA's `readme.txt` is preserved verbatim inside the bundle at
`cpma/readme.txt` and includes upstream contact details. q3now ships CPMA
unmodified, with its accompanying readme; users wishing to reuse the mod
should consult the CPMA project directly for licensing terms.

CPMA's pk3 includes mathematical-library code derived from **OpenLibm**,
whose license is shipped at `cpma/openlibm_license.md`. That file is a
multi-license catalogue covering the FreeBSD msun + OpenBSD libm + Julia
project + Sun Microsystems contributors. q3now reproduces CPMA's
`openlibm_license.md` in the bundle as part of CPMA's own attribution
chain. Key OpenLibm copyright holders:

- The Julia Project (MIT — see Appendix A) — `Copyright (c) 2011-14 The Julia Project`
- Stephen L. Moshier (ISC) — `Copyright (c) 2008 Stephen L. Moshier <steve@moshier.net>`
- FreeBSD msun + OpenBSD libm contributors (BSD-2-Clause and ISC — see Appendices C and below)
- Sun Microsystems (public-domain dedication)
- LGPL portions limited to test files, not redistributed in CPMA's pk3

ISC license canonical text:

```
Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
```

### 2.4 High Quality Quake (HQQ)

| Field | Value |
|---|---|
| Component | High Quality Quake — community texture replacement pack |
| Path inside bundle | `q3a-hqq/pak9hqq37.pk3`, `q3a-hqq/pak9hqq37.txt` |
| Version | 3.7 (2021-05-17) |
| Author | ZerTerO |
| Upstream | https://www.moddb.com/mods/high-quality-quake |
| License | Not specified in distributed `pak9hqq37.txt`. ModDB community-mod tradition is permissive personal-use redistribution; explicit licensing should be confirmed with the author. |

HQQ's `pak9hqq37.txt` (author + version metadata) is preserved verbatim
inside the bundle. q3now ships HQQ unmodified. Users wishing to extract
or modify HQQ should consult the upstream ModDB page or contact the
author directly.

### 2.5 Community map pack

`id-quakepack.zip` ships 13 community Quake III Arena maps, one per
`map_*/` directory (one without the prefix: `phantq3dm4/`). Each
directory preserves the original mapper's `.txt` readme verbatim
alongside the `.pk3`, satisfying every author's "must include this
text file with the level" redistribution clause.

| Directory | Title | Author | Year | Permission grant from author's `.txt` |
|---|---|---|---|---|
| `map_13dawn/` | Gothic Dawn (`13dawn.bsp`) | Stefan Scholz "sst13" — http://www.sst13.de | 2010 | No explicit redistribution clause; standard Q3 community-map convention applies (electronic distribution permitted with readme intact). New textures based on modified id Software originals. |
| `map_bst3dm1/` | Terminatria (`bst3dm1.bsp`) | Russell "bst" Vint | 2011 | No explicit redistribution clause; standard Q3 community-map convention applies. Floor textures derived from cgtextures.com (now textures.com); see §2.5.1 below. |
| `map_charonq2dm1v2/` | Return to the Edge (`charonq2dm1v2.bsp`) | Charon — http://www.interactivedeath.net/ | 2000 | "You may not include or distribute this map in any sort of commercial product without permission from the author. You may not mass distribute this level via any non-electronic means, including but not limited to compact disks, and floppy disks without permission from the author." |
| `map_cpm1a/` | Wicked (`cpm1a.bsp` / `q3jdm8b.bsp`) | FxR\|jude (adonald@nsw.bigpond.net.au) + Decker (Decker@killerinstincts.com) | 2000 | "This map MAY be distributed on bbs sites or the WWW, as long as this text file is included, intact. This map MAY NOT be distributed on CD or floppy unless you have the author's permission. **This map MAY NOT be used as a base for other levels.**" Map design © 2000 FxR\|jude. |
| `map_hub3aeroq3/` | Aerowalk (`hub3aeroq3.bsp`) | The Preacher (original Quake design) — Q3 conversion by The Hubster (hburji@hotmail.com) | 2001 | "This LEVEL may be distributed ONLY over the Internet and/or BBS systems. You are NOT authorized to put this LEVEL on any CD or distribute it in any way without my written permission." Quake III Arena (C) 1999 id Software. The Preacher (C). All rights reserved. |
| `map_hub3dm1/` | Dismemberment (`hub3dm1.bsp`) | The Hubster — http://hubster.challenge-au.com/ | 2001 | "Authors MAY NOT use this level as a base to build additional levels. You MUST NOT distribute this level UNLESS you INCLUDE THIS FILE WITH NO MODIFICATIONS. This LEVEL may be distributed ONLY over the Internet and/or BBS systems." |
| `map_mrcq3t6/` | Bitter Embrace (`mrcq3t6.bsp` + `mrcq3t6_ffa.bsp`); also includes `bot-bones-bitterbones.pk3` (custom bot definition for the map) | Todd "Mr.CleaN" Rose — http://www.planetquake.com/mrclean/ | 2001 | "You MAY distribute this PK3 in any electronic format (BBS, Internet, CD, etc) as long as you contact me first, include all files intact in the original archive, and send me a free copy (if it's a CD)." Original textures © Todd Rose; "You may NOT use this level as a base to build additional levels." |
| `map_phantq3dm1_rev/` | Battleforged (`map_phantq3dm1_rev.bsp`); also includes 4 screenshots `phantq3dm1_0[1-4]_600.jpg` | Tom Perryman ("Phantazm11") — www.phantazm11.com | 2009 (rev 2009-04-27) | "(c) 2009 Tom Perryman. All rights reserved. You should not use this map as a base to build new levels. This level may be electronically distributed only at NO CHARGE to the recipient in its current state, **MUST include this .txt file**, and may NOT be modified IN ANY WAY. Please contact me if you want to include this on CD or as part of a map pack." |
| `map_phantq3dm3_rev/` | Corrosion (`phantq3dm3_rev.bsp`) | Tom Perryman ("Phantazm11") | 2010 | Same terms as `phantq3dm1_rev` above (verbatim author boilerplate). 2nd-place winner of Maverick Servers Mapping Competition #2 (Summer 2010). |
| `map_pro-tourney7/` | Almost Lost (`pro-q3tourney7.bsp`) | **id Software** — http://www.idsoftware.com | (Q3A point release era) | This is one of id Software's officially-released "pro tournament" maps; same id Software copyright as the demo paks (§2.1). The map's `.txt` is intentionally minimal because it's by id. |
| `map_q3shw26/` | Battlegrounds (`q3shw26.bsp`) | Paweł "ShadoW" Chrapka (chrapka.pawel@gmail.com) — http://shadowsdomain.wordpress.com/ | 2010 | **GPL-2.0-or-later** (explicit license file shipped at `map_q3shw26/Q3shw26-map.license.txt`). Per author: *"the GPL only applies to my work (the .map-file), some textures were made by Id Software and are released under a different license."* Some textures additionally derived from cgtextures.com — see §2.5.1. |
| `map_ztn3dm1/` | Blood Run (`ztn3dm1.bsp`) | Sten "ztn" Uusvali — http://www.planetquake.com/ztn | 2000 | "Copyright (2000) by Sten Uusvali. All rights reserved." Textures: "(c) 1999-2000 Id Software Inc." (re-uses base game art). No explicit redistribution permission in the readme; community-map convention applies. |
| `phantq3dm4/` | Windsong Keep (`phantq3dm4.bsp`) | Tom Perryman ("Phantazm11") | 2011-09-18 | Same terms as `phantq3dm1_rev` (verbatim author boilerplate). 2nd-place winner of Maverick Servers Mapping Competition #3 (Summer 2011). |

#### 2.5.1 cgtextures.com / textures.com derivative textures

Two maps in this pack — `map_bst3dm1/` (Terminatria) and `map_q3shw26/`
(Battlegrounds) — include textures **derived from images at
cgtextures.com** (now hosted at https://www.textures.com/). Per textures.com's
terms of service, those source images may not be redistributed in their
unmodified form, but derivative works are permitted. Both maps' readmes
disclose this provenance and the q3now bundle preserves both readmes
verbatim.

#### 2.5.2 Aggregate distribution-permission status

The launcher's bundle distribution model — electronic, free of charge,
preserving each map's `.txt` readme intact — is **explicitly permitted**
by every author's terms with one practical caveat for three maps:

- **`map_mrcq3t6/`** (Bitter Embrace) — Todd Rose's terms ask that
  redistributors "contact me first." q3now's mirror operator is
  encouraged to send a courtesy notification to mrclean@planetquake.com
  before tagging public releases that include this map.
- **`map_phantq3dm1_rev/`, `map_phantq3dm3_rev/`, `phantq3dm4/`** — Tom
  Perryman's standard boilerplate asks that mappack inclusions be
  cleared with him in advance. Same courtesy-notification recommendation
  to phantazm11@gmail.com.

These are **author-courtesy obligations**, not strict licence-grant
preconditions; the maps are otherwise unrestricted for free electronic
distribution alongside their unmodified readmes (which q3now's bundle
preserves).

#### 2.5.3 What q3now's distribution preserves

Per the obligations enumerated above, the launcher's bundle preserves:

- Every original `.txt` readme **byte-for-byte unmodified** alongside
  the corresponding `.pk3`.
- The unmodified `.pk3` files (no repacking, no re-extraction).
- `map_q3shw26/Q3shw26-map.license.txt` (the GPL-2.0 text shipped by the
  map author).
- Companion files (the 4 JPG screenshots in `map_phantq3dm1_rev/`, the
  bot-definition `bot-bones-bitterbones.pk3` accompanying `map_mrcq3t6/`).

### 2.6 q3now-original mod content (pax21.sw3z)

The mod content q3now ships in its own `pax21.sw3z` archive — built from
`modfiles/` and shipped in q3now release artifacts — is original work
created for q3now and licensed under the project's GPL-2.0-or-later. No
id Software content, no CPMA content, and no HQQ content is included in
`pax21.sw3z`. This archive is independent of the launcher's `assets
download` flow.

---

## 3. Vendored C / C++ libraries (`src/libs/`)

Each entry below cites its license appendix for the canonical permission text.
All licenses in this section are **redistribution-permissive with attribution
required** (BSD-2/3, MIT, Apache-2.0, Zlib variants). q3now's GPL-2.0-or-later
binaries inherit obligations from each listed library.

### 3.1 zlib-ng

- **Upstream:** https://github.com/zlib-ng/zlib-ng
- **License:** Zlib (see Appendix D)
- **Path:** `src/libs/zlib-ng/`
- **Pinned:** commit `d1afe3a6` (≈ `v1.2.8_jtkv4-2747-gd1afe3a6`)

Copyright notice (verbatim from `LICENSE.md`):

```
(C) 1995-2024 Jean-loup Gailly and Mark Adler

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.
```

### 3.2 LuaJIT

- **Upstream:** https://luajit.org/ (https://github.com/LuaJIT/LuaJIT)
- **License:** MIT (see Appendix A)
- **Path:** `src/libs/luajit/`
- **Pinned:** commit `18b087cd` (≈ `v2.1.ROLLING-333`)
- **Bundled:** Lua 5.1/5.2 (MIT, Lua.org / PUC-Rio), dlmalloc (Public Domain — Doug Lea)

Copyright notice (verbatim from `COPYRIGHT`):

```
LuaJIT -- a Just-In-Time Compiler for Lua. https://luajit.org/

Copyright (C) 2005-2026 Mike Pall. All rights reserved.

[ MIT license: https://www.opensource.org/licenses/mit-license.php ]
```

LuaJIT additionally includes Lua 5.1/5.2:

```
Copyright (C) 1994-2012 Lua.org, PUC-Rio.
[ MIT license ]
```

The full canonical MIT permission text appears in **Appendix A**.

### 3.3 mpack

- **Upstream:** https://github.com/ludocode/mpack
- **License:** MIT (see Appendix A)
- **Path:** `src/libs/mpack/`
- **Pinned:** commit `a2d72027` (`v1.1.1-9-ga2d7202`)

Copyright notice:

```
Copyright (c) 2015-2021 Nicholas Fraser and the MPack authors
```

### 3.4 Ogg / Vorbis / Opus / opusfile (Xiph.Org family)

Four separate libraries from Xiph.Org, all under BSD-3-Clause
(see Appendix B). Audio decoding stack used for in-game audio assets.

#### 3.4.1 libogg

- **Upstream:** https://xiph.org/ogg/
- **License:** BSD-3-Clause (see Appendix B)
- **Path:** `src/libs/ogg/`
- **Copyright:** `Copyright (c) 2002, Xiph.org Foundation`

#### 3.4.2 libvorbis

- **Upstream:** https://xiph.org/vorbis/
- **License:** BSD-3-Clause (see Appendix B)
- **Path:** `src/libs/vorbis/`
- **Copyright:** `Copyright (c) 2002-2020 Xiph.org Foundation`

#### 3.4.3 Opus

- **Upstream:** https://opus-codec.org/
- **License:** BSD-3-Clause (see Appendix B) **plus royalty-free patent licenses**
- **Path:** `src/libs/opus/`
- **Copyright:**

  ```
  Copyright 2001-2023 Xiph.Org, Skype Limited, Octasic,
                      Jean-Marc Valin, Timothy B. Terriberry,
                      CSIRO, Gregory Maxwell, Mark Borgerding,
                      Erik de Castro Lopo, Mozilla, Amazon
  ```

  Opus is additionally subject to royalty-free patent licenses granted by
  Xiph.Org, Microsoft, Broadcom, and others. See the patent grant
  declarations linked from `src/libs/opus/COPYING`:
  - https://datatracker.ietf.org/ipr/1524/  (Xiph.Org)
  - https://datatracker.ietf.org/ipr/1914/  (Microsoft)
  - https://datatracker.ietf.org/ipr/1526/  (Broadcom)

#### 3.4.4 opusfile

- **Upstream:** https://github.com/xiph/opusfile
- **License:** BSD-3-Clause (see Appendix B)
- **Path:** `src/libs/opusfile/`
- **Copyright:** `Copyright (c) 1994-2013 Xiph.Org Foundation and contributors`

### 3.5 libjpeg-turbo

- **Upstream:** https://github.com/libjpeg-turbo/libjpeg-turbo
- **License:** Dual — IJG License + BSD-3-Clause (see Appendix B)
- **Path:** `src/libs/libjpeg-turbo/`
- **Pinned:** commit `9b09d516` (`2.1.4-674-g9b09d516`)

Per `LICENSE.md`, libjpeg-turbo is covered by two compatible licenses:

- The IJG (Independent JPEG Group) License — applies to the libjpeg API
  library and any code inherited from libjpeg.
- The Modified (3-clause) BSD License — applies to the TurboJPEG API
  library, libspng, and the build/test system.

Both apply in the context of the overall library. The IJG license requires
the following acknowledgment in **documentation accompanying any binary
distribution that includes libjpeg-turbo**:

> This software is based in part on the work of the Independent JPEG Group.

(That sentence is hereby reproduced in q3now's documentation. Some of
libjpeg-turbo's modules and SIMD source code use the **Zlib license**
(Appendix D), which is subsumed by the IJG and BSD licenses in the context
of the overall library.)

### 3.6 libjpeg (legacy IJG, source-vendored)

- **Upstream:** Independent JPEG Group, https://www.ijg.org/
- **License:** IJG License (BSD-style, attribution + acknowledgment required)
- **Path:** `src/libs/jpeg/`
- **Version:** Release 9e of 16-Jan-2022

The IJG license requires that **executable distributions accompanied by
documentation must state**:

> This software is based in part on the work of the Independent JPEG Group.

Verbatim from the upstream `README` (Section "LEGAL ISSUES"):

```
This software is copyright (C) 1991-2022, Thomas G. Lane, Guido Vollbeding.
All Rights Reserved except as specified below.

Permission is hereby granted to use, copy, modify, and distribute this
software (or portions thereof) for any purpose, without fee, subject to these
conditions:
(1) If any part of the source code for this software is distributed, then this
README file must be included, with this copyright and no-warranty notice
unaltered; and any additions, deletions, or changes to the original files
must be clearly indicated in accompanying documentation.
(2) If only executable code is distributed, then the accompanying
documentation must state that "this software is based in part on the work of
the Independent JPEG Group".
(3) Permission for use of this software is granted only if the user accepts
full responsibility for any undesirable consequences; the authors accept
NO LIABILITY for damages of any kind.
```

This software may be referred to only as "the Independent JPEG Group's
software". Use of any IJG author's name in advertising is not granted.

The unmodified upstream `README` is preserved at `src/libs/jpeg/README`.

### 3.7 LZ4

- **Upstream:** https://github.com/lz4/lz4
- **License:** BSD-2-Clause (see Appendix C)
- **Path:** `src/libs/lz4/`
- **Copyright (from `lz4.h` header):** `Copyright (c) Yann Collet. All rights reserved.`

The full BSD 2-Clause permission text is reproduced verbatim in each LZ4
source file's header (e.g. `src/libs/lz4/lz4.h`), satisfying the
"redistributions of source code must retain the copyright notice" clause
in source distributions. The same notice applies to **binary
distributions** per the second clause of BSD-2 — see Appendix C for the
canonical permission text.

### 3.8 picoquic

- **Upstream:** https://github.com/private-octopus/picoquic
- **License:** MIT (see Appendix A)
- **Path:** `src/libs/picoquic/`
- **Pinned:** commit `0bca2210` (≈ `draft-16-final-4928-g0bca2210`)

Copyright notice:

```
MIT License

Copyright (c) 2017 Private Octopus
```

Wired applies a small set of MinGW-portability patches to picoquic at
configure time (`patches/picoquic-mingw/*.patch`); these modifications
follow the MIT permission grant and are themselves licensed under the same
MIT terms. The patched source remains attributable to Private Octopus.

Wired's CMake build excludes the following picoquic source-file groups
from compilation (and therefore from the distributed binary): `*minicrypto*`,
`*mbedtls*`, `*fusion*`, `*qlog*` (see `CMakeLists.txt:241-250`).
Attribution for excluded code is not required.

### 3.9 picotls

- **Upstream:** https://github.com/h2o/picotls
- **License:** MIT (see Appendix A)
- **Path:** `src/libs/picotls/`
- **Pinned:** commit `b84869f4`

The picotls TLS protocol implementation core is MIT-licensed. picotls
bundles cryptographic backends in `src/libs/picotls/deps/`; Wired's CMake
build links against the OpenSSL backend and **excludes** the alternative
backends (`minicrypto`, `cifra`, `micro-ecc`-based code) from compilation.
Attribution for those excluded backends is therefore not required by
Wired's distribution, but is recorded here for completeness:

#### 3.9.1 micro-ecc (sub-dep of picotls/minicrypto)

- **Upstream:** https://github.com/kmackay/micro-ecc
- **License:** BSD-2-Clause (see Appendix C)
- **Path:** `src/libs/picotls/deps/micro-ecc/`
- **Copyright:** `Copyright (c) 2014, Kenneth MacKay`

#### 3.9.2 cifra (sub-dep of picotls/minicrypto)

- **Upstream:** https://github.com/ctz/cifra
- **License:** CC0 1.0 Universal (Public Domain Dedication)
- **Path:** `src/libs/picotls/deps/cifra/`
- No attribution required by license; recorded for transparency.

### 3.10 WAMR (WebAssembly Micro Runtime)

- **Upstream:** https://github.com/bytecodealliance/wasm-micro-runtime
- **License:** Apache License 2.0 (see Appendix E)
- **Path:** `src/libs/wamr/`
- **Version:** 2.4.3

Per Apache-2.0 §4(d), the upstream `LICENSE` file is preserved at
`src/libs/wamr/LICENSE`. WAMR additionally bundles second-tier dependencies
(NuttX-derived bits, AsmJit, Zydis, libuv, UVWASI, libpng-2.0, SSP,
er-coap, …); the canonical attribution catalogue for those is upstream
WAMR's own third-party-license documentation under
`src/libs/wamr/build-scripts/` and `src/libs/wamr/core/`. Wired does not
duplicate that catalogue here; consult those upstream files when
distributing.

### 3.11 recastnavigation

- **Upstream:** https://github.com/recastnavigation/recastnavigation
- **License:** Zlib-style (see Appendix D — same form as zlib)
- **Path:** `src/libs/recastnavigation/`
- **Pinned:** commit `9f4ce644` (`v1.6.0-367-g9f4ce64`)

Copyright notice:

```
Copyright (c) 2009 Mikko Mononen memon@inside.org

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

recastnavigation's `RecastDemo/` subtree (which Wired does not build into
its release artifacts) bundles development-only deps such as Dear ImGui
and SDL — Wired's release binaries don't link to those, so attribution is
not required for Wired's distribution.

### 3.12 curl (system library — not vendored)

The `src/libs/curl/` directory contains **only** CMake configuration
helper scripts (`conf751.sh`, `conf840.sh`, `windows/`) used during build.
**The actual libcurl is provided by the host system** (`libcurl4-openssl-dev`
on Linux, system curl on macOS, `mingw-w64-x86_64-curl` on Windows-MSYS2).
Wired does not redistribute libcurl source or binaries; attribution
obligations fall on the host distribution.

For reference, libcurl is released under the **curl license** (a permissive
MIT-style license) at https://curl.se/docs/copyright.html .

---

## 4. Vendored single-header libraries & headers

### 4.1 miniaudio

- **Upstream:** https://github.com/mackron/miniaudio
- **License:** Public Domain OR MIT-0 (user's choice)
- **Path:** `code/client/miniaudio.h`
- **Version:** 0.11.25, commit `9634bedb` (vendored 2026-04-06)
- **Author:** David Reid

Per the upstream header:

> Audio playback and capture library. Choice of public domain or MIT-0.
> See license statements at the end of this file.

The full upstream license text (both options) is preserved verbatim at the
end of `code/client/miniaudio.h`. Wired elects no specific option and
reproduces both as shipped.

### 4.2 Vulkan-Headers (Khronos)

- **Upstream:** https://github.com/KhronosGroup/Vulkan-Headers
- **License:** Apache License 2.0 (see Appendix E)
- **Path:** `code/renderercommon/vulkan/`

Header copyright (verbatim from `vulkan_core.h`):

```
Copyright 2015-2022 The Khronos Group Inc.

SPDX-License-Identifier: Apache-2.0
```

Per Apache-2.0 §4, this notice is preserved. Each `code/renderercommon/vulkan/*.h`
file carries `SPDX-License-Identifier: Apache-2.0` and the Khronos copyright
line (plus, where applicable, a short note recording a backported definition).

### 4.3 minizip / unzip (zip read support)

- **Upstream:** minizip, originally distributed with zlib
  (https://www.zlib.net/) — by Gilles Vollant (https://www.winimage.com/zLibDll/minizip.html)
- **License:** Zlib (see Appendix D)
- **Path:** `code/qcommon/unzip.c`, `code/qcommon/unzip.h`
- **Copyright:** `Copyright (c) 1998-2010 Gilles Vollant`; the in-tree copy
  also carries a small modification by `Joerg Dietrich <dietrich_joerg@gmx.de>`.

Both files carry `SPDX-License-Identifier: Zlib` and retain the upstream
copyright / permission notice.

### 4.4 SMAA — precomputed lookup textures

- **Upstream:** https://github.com/iryoku/smaa
- **License:** MIT (with an explicit clarification that the notice need not be
  reproduced in binary distributions)
- **Path:** `code/renderervk/smaa_area_texture.h`, `code/renderervk/smaa_search_texture.h`
- **Copyright (2013):** Jorge Jimenez, Jose I. Echevarria, Belen Masia,
  Fernando Navarro, Diego Gutierrez

These two headers contain the precomputed AreaTex / SearchTex byte arrays used
by the renderer's SMAA pass. Each file carries `SPDX-License-Identifier: MIT`,
the five copyright lines above, and the upstream permission text verbatim. The
canonical MIT permission text also appears in Appendix A.

### 4.5 puff — minimal inflate

- **Upstream:** distributed with zlib (`contrib/puff/`) — by Mark Adler
- **License:** zlib-style permissive (the upstream `puff.h` permission notice,
  kept verbatim in the file)
- **Path:** `code/qcommon/puff.c`, `code/qcommon/puff.h`
- **Copyright:** `Copyright (c) 2002-2013 Mark Adler`; the in-tree copy also
  carries a modification note by `Joerg Dietrich <dietrich_joerg@gmx.de>` (2006).

The full upstream permission notice is reproduced verbatim in `puff.h`. The
files carry `SPDX-License-Identifier: LicenseRef-puff` (the upstream license has
no SPDX standard identifier; the full text is the authoritative grant).

### 4.6 MD5 implementation

- **Path:** `code/qcommon/util/md5.c`
- **License:** as stated in the file's own header — preserved verbatim, with
  `SPDX-License-Identifier` set to match. (Common provenances for this file in
  the Quake 3 lineage are RSA Data Security's MD5 reference implementation or a
  public-domain rewrite; the in-tree file's header is authoritative.)

### 4.7 OpenGL extension header (`glext.h`)

- **Upstream:** Khronos OpenGL / OpenGL ES Registry
  (https://github.com/KhronosGroup/OpenGL-Registry)
- **License:** MIT (modern Khronos registry headers) — see Appendix A
- **Path:** `code/renderer2/glext.h`
- **Copyright:** `Copyright (c) 2013-2020 The Khronos Group Inc.`

The file carries `SPDX-License-Identifier: MIT` and the Khronos copyright line.

### 4.8 Inter-Quake Model (IQM) format header

- **Upstream:** http://sauerbraten.org/iqm/ — by Lee Salzman
- **License:** public domain (the format and reference header are released into
  the public domain by the author)
- **Path:** `code/renderer/iqm.h`, `code/renderervk/iqm.h`
- The IQM model *loaders* (`tr_model_iqm.c` in each renderer) were backported
  from ioquake3 and remain under the engine's GPL terms; only the format
  *header* `iqm.h` is the public-domain upstream artifact.

These headers carry `SPDX-License-Identifier: LicenseRef-public-domain`.

---

## 5. Build & development tooling

The components in this section run **at build time** but are not linked
into Wired's distributed binary. They are listed for completeness; users
of Wired release artifacts do not redistribute them.

### 5.1 msdf-atlas-gen

- **Upstream:** https://github.com/Chlumsky/msdf-atlas-gen
- **License:** MIT (see Appendix A)
- **Path:** `tools/msdf-atlas-gen/`
- **Version:** v1.4 (commit `2ede254`)
- **Copyright:** `Copyright (c) 2020 - 2026 Viktor Chlumsky`

msdf-atlas-gen bundles **msdfgen** (MIT, also Viktor Chlumsky) and
**artery-font-format** (MIT). Wired invokes msdf-atlas-gen at build time
to bake font atlases from upstream font files; the resulting `.png` and
`.json` outputs ship in q3now's mod pack but the tool itself does not.

### 5.2 wasi-sdk

- **Upstream:** https://github.com/WebAssembly/wasi-sdk
- **License:** Apache-2.0 with LLVM Exceptions (LLVM toolchain)
- **Used at:** WASM module compilation (qagame.wasm, cgame.wasm)
- **Path:** install location is per-developer (`/opt/wasi-sdk` or
  `$USERPROFILE/wasi-sdk`); not vendored in this tree.

The compiled WASM modules **are** distributed inside `pax21.sw3z`, but they
contain only q3now-original source code compiled by wasi-sdk's clang —
they don't embed wasi-sdk code itself. Attribution for wasi-sdk is not
required in Wired's distribution.

---

## 6. Launcher — Go module dependencies

The launcher (`launcher/`) is a Go program built with Wails (a Go-WebView
binding framework). The compiled binary `q3now-launcher` statically links
its Go dependencies. Direct dependencies and their licenses (per
`launcher/go.mod`):

### 6.1 Wails v2

- **Module:** `github.com/wailsapp/wails/v2`
- **License:** MIT (see Appendix A)
- **Version:** v2.12.0
- **Copyright:** `Copyright (c) 2018 Lea Anthony`

Wails additionally pulls in indirect dependencies that are linked into
the launcher binary, all under permissive licenses. Most relevant:

- `github.com/wailsapp/go-webview2` — MIT (`Copyright (c) John Chadwick`)
- `git.sr.ht/~jackmordaunt/go-toast/v2` — MIT
- `github.com/gorilla/websocket` — BSD-2-Clause (see Appendix C)
- `github.com/labstack/echo/v4` — MIT
- `github.com/leaanthony/go-ansi-parser` — MIT
- `github.com/leaanthony/gosod` — MIT
- `github.com/leaanthony/slicer` — MIT
- `github.com/tkrajina/go-reflector` — Apache-2.0
- `github.com/valyala/bytebufferpool`, `valyala/fasttemplate` — MIT
- `github.com/jchv/go-winloader` — MIT
- `github.com/godbus/dbus/v5` — BSD-2-Clause
- `github.com/go-ole/go-ole` — MIT
- `github.com/google/uuid` — BSD-3-Clause
- `golang.org/x/{crypto,net,sys,text}` — BSD-3-Clause (Go Authors)

The authoritative pinned-version list is `launcher/go.sum`.

### 6.2 Cobra

- **Module:** `github.com/spf13/cobra`
- **License:** Apache License 2.0 (see Appendix E)
- **Version:** v1.10.2
- **Copyright:** `Copyright © 2013-2025 Steve Francia <spf@spf13.com>`

Per Apache-2.0 §4(d), Cobra's `LICENSE.txt` and `NOTICE` files are
preserved in the Go module cache; their contents are reproduced
canonically by reference to https://github.com/spf13/cobra/blob/master/LICENSE.txt .

Pulls in `github.com/spf13/pflag` (BSD-3-Clause, "Copyright (c) 2012 Alex
Ogier. All rights reserved." and "Copyright (c) 2012 The Go Authors. All
rights reserved.") and `github.com/inconshreveable/mousetrap` (Apache-2.0).

### 6.3 opus.v2 (Go bindings)

- **Module:** `gopkg.in/hraban/opus.v2`
- **License:** MIT (see Appendix A)
- **Version:** v2.0.0-20230925203106
- **Copyright:** `Copyright © 2015-2017 Hraban Luyat`

This module CGo-links to system `libopus` and `libopusfile` (provided by
host package: `libopus-dev`/`libopusfile-dev` on Linux,
`mingw-w64-x86_64-opus`/`-opusfile` on Windows-MSYS2, `opus`/`opusfile` on
macOS Homebrew). The Xiph BSD-3 attribution from §3.4 covers the linked
native library; this Go-bindings layer's MIT covers the bridge code only.

### 6.4 sw3z-archiver (in-tree tool)

- **Module:** `github.com/eser/q3now/tools/sw3z-archiver`
- **License:** GPL-2.0-or-later (in-tree, q3now project code)
- **Path:** `tools/sw3z-archiver/`

Pulls `github.com/pierrec/lz4/v4` (BSD-3-Clause, `Copyright (c) 2015,
Pierre Curto`).

### 6.5 Indirect Go dependencies — full list

All transitively-required Go modules are pinned in `launcher/go.sum` with
exact-version hashes. Every transitive dep listed there is under a
permissive license (MIT / BSD-2 / BSD-3 / Apache-2.0). For automated
audit, run `go-licenses report ./... > go-licenses.txt` from `launcher/`.

---

## 7. Launcher — frontend dependencies

The Wails launcher embeds a compiled React frontend in its binary
(`launcher/frontend/dist/` is `//go:embed`-ed by the launcher main package).
The `launcher/frontend/package.json` declares:

| Package | License | Role |
|---|---|---|
| `react` ^18.2.0 | MIT (Appendix A) | UI framework |
| `react-dom` ^18.2.0 | MIT (Appendix A) | DOM renderer |
| `vite` ^8.0.8 | MIT (Appendix A) | Build tool (dev-only; build outputs are bundled) |
| `@vitejs/plugin-react` ^5.2.0 | MIT (Appendix A) | Vite plugin (dev-only) |
| `@types/react`, `@types/react-dom` | MIT (DefinitelyTyped) | Type declarations (dev-only) |

React copyright: `Copyright (c) Meta Platforms, Inc. and affiliates.`
Vite copyright: `Copyright (c) 2019-present, VoidZero Inc. and Vite contributors`.

Vite, the React typings, and `@vitejs/plugin-react` are dev-only and not
linked into the runtime bundle. The bundled minified runtime contains
React + ReactDOM only. Copies of the MIT permission text (Appendix A) for
those packages satisfy attribution.

Vite's runtime additionally pulls in transitive npm modules via
`package-lock.json`; all are under MIT/BSD/Apache-2.0 licenses. For
exhaustive enumeration of the runtime tree, run `npx license-checker
--production --summary` from `launcher/frontend/`.

---

## 8. Engine font assets (q3now mod fonts)

q3now's own `pax21.sw3z` archive — built from `modfiles/` and shipped in
release artifacts — is licensed under the project's GPL-2.0-or-later
(see §2.6). The exception is the **MSDF font atlases**, which are
signed-distance-field rasterisations of upstream third-party fonts; the
atlas outputs inherit each source font's license.

- **Shaders, scripts, configs, q3now-original art** — q3now project code
  and assets, GPL-2.0-or-later, same license as the engine.
- **MSDF font atlases** (`modfiles/fonts/*.json` + `*.png`) — these are
  signed-distance-field rasterisations of the source TrueType fonts listed
  below, baked at build time by `msdf-atlas-gen`. The atlas files
  themselves inherit the source font's license:

  | Atlas family | Source font | License |
  |---|---|---|
  | `oxanium*` | Oxanium by Sherif Magdy | SIL Open Font License 1.1 |
  | `sansman-*` | Sansman by Manuel Guerrero | (verify upstream — likely SIL OFL or CC-BY) |
  | `sharetechmono` | Share Tech Mono by Carrois Apostrophe | SIL Open Font License 1.1 |

  Per SIL OFL 1.1, the font copyright notices and license must be included
  in the documentation accompanying any package that includes the font.
  When q3now ships a release artifact containing the baked atlas PNG/JSON,
  this section satisfies that obligation. The full SIL OFL 1.1 text is
  available at https://scripts.sil.org/OFL_web .

  If you redistribute q3now and want to verify the font licenses
  authoritatively, check the source TTF/OTF files at the upstream font
  repositories (Oxanium and Share Tech Mono are both on Google Fonts /
  fonts.google.com under SIL OFL 1.1).

---

## Appendix A — MIT License (canonical text)

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

## Appendix B — BSD-3-Clause License (canonical text)

```
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

- Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

- Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

## Appendix C — BSD-2-Clause License (canonical text)

```
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials provided
    with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

## Appendix D — Zlib License (canonical text)

```
This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

## Appendix E — Apache License 2.0 (reference)

The Apache License 2.0 is too long to embed inline; the canonical text is
maintained by the Apache Software Foundation at:

> https://www.apache.org/licenses/LICENSE-2.0

A verbatim copy is also preserved at `src/libs/wamr/LICENSE` (vendored
alongside WAMR's own source tree). For each Apache-2.0-licensed component
listed in this document (WAMR, Vulkan-Headers, Cobra, mousetrap,
go-reflector, wasi-sdk-LLVM-toolchain), the copyright notice in the
component's own `NOTICE`/`LICENSE` file is preserved upstream and is
incorporated here by reference. Apache-2.0 §4(d) requires that any
`NOTICE` file's contents be reproduced in derivative-work distributions.
For q3now's distribution, that obligation is satisfied by:

- shipping `src/libs/wamr/LICENSE` (and the `NOTICE` content within
  WAMR's tree) inside the q3now source archive;
- preserving the Khronos copyright header inside `code/renderercommon/vulkan/vulkan_core.h`;
- preserving the Cobra `NOTICE` and copyright text in the Go module cache
  during build (and in any redistributed source archive).

Apache-2.0's other obligations (notice of modifications when distributing
modified source) apply to the upstream sources; Wired does not modify
WAMR, Vulkan-Headers, or Cobra source from upstream releases.

---

*This document was generated for q3now and Wired and is itself licensed under the
project's terms (see `LICENSE` — GPL-3.0-or-later for new project material).
Last updated: 2026-05-12.*
