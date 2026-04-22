/*
===========================================================================
nav_coord.c -- Quake <-> Recast coordinate conversion (see nav_coord.h)
===========================================================================
*/
#include "nav_coord.h"

void Nav_QuakeToRecast( const float in[3], float out[3] ) {
	out[0] =  in[0];
	out[1] =  in[2];
	out[2] = -in[1];
}

void Nav_RecastToQuake( const float in[3], float out[3] ) {
	out[0] =  in[0];
	out[1] = -in[2];
	out[2] =  in[1];
}
