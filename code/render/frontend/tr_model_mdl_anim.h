// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// tr_model_mdl_anim.h — the pure Q1-.mdl animation-range derivation.
//
// Grouping .mdl frame names into animation ranges is pure logic (only <string.h>
// + the mdlAnimRange_t struct), so it lives here and is compiled BYTE-IDENTICALLY
// into (a) the renderer (tr_model_mdl.c defines MDL_ANIM_IMPL) and (b) the unit
// test (mdl_anim_test.c defines MDL_ANIM_TEST_STANDALONE). No renderer state, no
// Q_strncpyz — a self-contained label copy — so the test needs no engine link.
//
// Algorithm: a frame's label is its name with trailing digits stripped
// ("walk1" -> "walk"). id Quake's convention puts the variant LETTER before the
// digits ("painb1", "deathc1", "atta1"), so a plain trailing-digit strip yields
// the correct label per range and never merges two logical animations nor splits
// one contiguous animation. A single pass opens a new range whenever the label
// changes; a same-label run extends the open range. An empty label (an all-digit
// or unnamed frame) opens no range.

#ifndef TR_MODEL_MDL_ANIM_H
#define TR_MODEL_MDL_ANIM_H

#include <string.h>

#ifdef MDL_ANIM_TEST_STANDALONE
// The test compiles without the renderer headers — mirror mdlAnimRange_t exactly.
typedef struct {
	char	label[16];
	int		first_frame;
	int		num_frames;
} mdlAnimRange_t;
#endif

// Linkage: the renderer TU (MDL_ANIM_IMPL) exports MDL_DeriveAnimRanges with
// external linkage (R_GetMDLAnimations in tr_model.c calls it across TUs); the
// standalone test keeps its own static copy. The label helper is always static.
#ifdef MDL_ANIM_IMPL
#define MDL_ANIM_LINKAGE  /* extern */
#else
#define MDL_ANIM_LINKAGE  static
#endif

// Strip trailing digits from a frame name to yield its animation label.
static void MDL_StripLabel( const char *name, char *out, int outSz ) {
	int len = (int)strlen( name );
	while ( len > 0 && name[len - 1] >= '0' && name[len - 1] <= '9' ) {
		len--;
	}
	if ( len >= outSz ) {
		len = outSz - 1;
	}
	memcpy( out, name, (size_t)len );
	out[len] = '\0';
}

// Group contiguous same-label frames into ranges. Returns the range count.
MDL_ANIM_LINKAGE int MDL_DeriveAnimRanges( const char (*frameNames)[16], int numFrames,
                                 mdlAnimRange_t *out, int maxOut ) {
	int  i, count = 0;
	char prevLabel[16];
	char curLabel[16];

	prevLabel[0] = '\0';

	for ( i = 0; i < numFrames; i++ ) {
		MDL_StripLabel( frameNames[i], curLabel, sizeof( curLabel ) );

		if ( curLabel[0] == '\0' ) {
			/* an unnamed / all-digit frame does not extend or open a range */
			prevLabel[0] = '\0';
			continue;
		}

		if ( count > 0 && strcmp( curLabel, prevLabel ) == 0 ) {
			out[count - 1].num_frames++;
		} else {
			if ( count >= maxOut ) {
				break;
			}
			/* self-contained copy (no Q_strncpyz dependency) */
			{
				size_t n = strlen( curLabel );
				if ( n >= sizeof( out[count].label ) ) n = sizeof( out[count].label ) - 1;
				memcpy( out[count].label, curLabel, n );
				out[count].label[n] = '\0';
			}
			out[count].first_frame = i;
			out[count].num_frames  = 1;
			count++;
			memcpy( prevLabel, curLabel, sizeof( prevLabel ) );
		}
	}
	return count;
}

#endif // TR_MODEL_MDL_ANIM_H
