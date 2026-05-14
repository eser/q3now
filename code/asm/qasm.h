// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef __ASM_I386__
#define __ASM_I386__

#include "../qcommon/q_platform.h"

#if defined(__MINGW32__) || defined(MACOS_X)
#undef ELF
#endif

#ifdef __ELF__
.section .note.GNU-stack,"",@progbits
#endif

#ifdef ELF
#define C(label) label
#else
#define C(label) _##label
#endif

#endif
