// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009-2011 Andrei Drexler, Richard Allen, James Canete
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef __TR_EXTRATYPES_H__
#define __TR_EXTRATYPES_H__

// tr_extratypes.h, for mods that want to extend tr_types.h without losing compatibility with original VMs

// extra refdef flags start at 0x0008
#define RDF_NOFOG		0x0008		// don't apply fog to polys added using RE_AddPolyToScene
#define RDF_EXTRA		0x0010		// Makro - refdefex_t to follow after refdef_t
#define RDF_SUNLIGHT    0x0020      // SmileTheory - render sunlight and shadows

typedef struct {
	float			blurFactor;
	float           sunDir[3];
	float           sunCol[3];
	float           sunAmbCol[3];
} refdefex_t;

#endif
