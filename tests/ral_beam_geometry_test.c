// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "../code/render/ral/core/ral_beam_geometry.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(void) {
    /* Analytic side directions from cross(axis, eye-center), including
     * all three least-parallel basis choices and their tie ordering. */
    const struct { float start[3], end[3], eye[3], side[3]; } cases[] = {
        {{4,2,0}, {12,2,0}, {0,0,0}, {0,0,-1}},
        {{4,-2,0}, {12,-2,0}, {0,0,0}, {0,0,1}},
        {{4,0,2}, {12,0,2}, {0,0,0}, {0,1,0}},
        {{4,0,0}, {12,0,0}, {0,0,0}, {0,0,1}},
        {{0,4,0}, {0,12,0}, {0,0,0}, {0,0,-1}},
        {{0,0,4}, {0,0,12}, {0,0,0}, {0,1,0}},
        {{8,-4,0}, {8,4,0}, {0,0,0}, {0,0,1}},
    };
    for (unsigned c=0; c<sizeof(cases)/sizeof(cases[0]); ++c) {
        float axis[3], side[3];
        CHECK(wired_ral_beam_frame(cases[c].start,cases[c].end,cases[c].eye,axis,side));
        float dot=0, axisLength=0, sideLength=0;
        for (unsigned i=0; i<3; ++i) {
            CHECK(fabsf(side[i]-cases[c].side[i]) < 0.00001f);
            dot+=axis[i]*side[i]; axisLength+=axis[i]*axis[i]; sideLength+=side[i]*side[i];
        }
        CHECK(fabsf(dot)<0.00001f && fabsf(axisLength-1)<0.00001f && fabsf(sideLength-1)<0.00001f);
        /* World translation must not rotate a beam or mirror its UVs. */
        float start[3], end[3], eye[3], shiftedAxis[3], shiftedSide[3];
        for (unsigned i=0; i<3; ++i) {
            start[i]=cases[c].start[i]+100;
            end[i]=cases[c].end[i]+100;
            eye[i]=cases[c].eye[i]+100;
        }
        CHECK(wired_ral_beam_frame(start,end,eye,shiftedAxis,shiftedSide));
        CHECK(memcmp(axis,shiftedAxis,sizeof(axis))==0 && memcmp(side,shiftedSide,sizeof(side))==0);
    }
    const float zero[3]={0,0,0}, end[3]={1,0,0}, bad[3]={NAN,0,0};
    float axis[3]={7,8,9}, side[3]={10,11,12};
    const float originalAxis[3]={7,8,9}, originalSide[3]={10,11,12};
    CHECK(!wired_ral_beam_frame(zero,zero,zero,axis,side));
    CHECK(!wired_ral_beam_frame(zero,end,bad,axis,side));
    CHECK(memcmp(axis,originalAxis,sizeof(axis))==0 && memcmp(side,originalSide,sizeof(side))==0);
    puts("PASS RAL beam facing, collinear basis, translation and atomic rejection");
    return 0;
}
