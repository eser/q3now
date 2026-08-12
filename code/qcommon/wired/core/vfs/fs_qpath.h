// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_CORE_VFS_FS_QPATH_H
#define WIRED_CORE_VFS_FS_QPATH_H

#include <stdbool.h>

/* Lexical qpath validation only. This rejects exact parent-directory
 * components and the legacy "::" spelling; it does not provide symlink or
 * junction containment after a path reaches the host filesystem. Callers must
 * invoke it before allocating a handle or constructing/opening an OS path. */
bool wired_fs_qpath_is_safe( const char *qpath );

#endif
