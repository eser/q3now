// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// cg_ents.c -- present snapshot entities, happens every single frame

#include "cg_local.h"
LOG_DECLARE_CHANNEL( ch_cgame, "cgame" );

void CG_AddRefEntityTemporalBase( centity_t *cent, const refEntity_t *ent,
		refEntityMotionRole_t role ) {
	centity_t *owner = cent;
	int entityNumber;

	if ( !cent || !ent ) {
		return;
	}
	if ( ent->renderfx & RF_FORCE_ENT_ALPHA ) {
		// Explicit vertex-alpha fading is not an opaque temporal producer.
		trap_R_AddRefEntityToScene( ent );
		return;
	}
	entityNumber = cent->currentState.number;
	if ( entityNumber < 0 || entityNumber >= MAX_GENTITIES ) {
		trap_R_AddRefEntityToScene( ent );
		return;
	}
	if ( cent == &cg.predictedPlayerEntity ) {
		owner = &cg_entities[entityNumber];
	}
	CG_TemporalIdentitySubmit( &owner->temporalIdentity,
		(uint32_t)entityNumber, role, ent,
		trap_R_AddRefEntityToScene, trap_R_AddRefEntityToSceneTemporal );
}


/*
======================
CG_PositionEntityOnTag

Modifies the entities position and axis by the given
tag location
======================
*/
void CG_PositionEntityOnTag( refEntity_t *entity, const refEntity_t *parent,
							qhandle_t parentModel, char *tagName ) {
	orientation_t	lerped;

	// lerp the tag
	trap_R_LerpTag( &lerped, parentModel, parent->oldframe, parent->frame,
		1.0 - parent->backlerp, tagName );

	// FIXME: allow origin offsets along tag?
	VectorCopy( parent->origin, entity->origin );
	for ( int i = 0 ; i < 3 ; i++ ) {
		VectorMA( entity->origin, lerped.origin[i], parent->axis[i], entity->origin );
	}

	// had to cast away the const to avoid compiler problems...
	MatrixMultiply( lerped.axis, ((refEntity_t *)parent)->axis, entity->axis );
	entity->backlerp = parent->backlerp;
}


/*
======================
CG_PositionRotatedEntityOnTag

Modifies the entities position and axis by the given
tag location
======================
*/
void CG_PositionRotatedEntityOnTag( refEntity_t *entity, const refEntity_t *parent,
							qhandle_t parentModel, char *tagName ) {
	orientation_t	lerped;
	vec3_t			tempAxis[3];

//AxisClear( entity->axis );
	// lerp the tag
	trap_R_LerpTag( &lerped, parentModel, parent->oldframe, parent->frame,
		1.0 - parent->backlerp, tagName );

	// FIXME: allow origin offsets along tag?
	VectorCopy( parent->origin, entity->origin );
	for ( int i = 0 ; i < 3 ; i++ ) {
		VectorMA( entity->origin, lerped.origin[i], parent->axis[i], entity->origin );
	}

	// had to cast away the const to avoid compiler problems...
	MatrixMultiply( entity->axis, lerped.axis, tempAxis );
	MatrixMultiply( tempAxis, ((refEntity_t *)parent)->axis, entity->axis );
}



/*
==========================================================================

FUNCTIONS CALLED EACH FRAME

==========================================================================
*/

/*
======================
CG_SetEntitySoundPosition

Also called by event processing code
======================
*/
void CG_SetEntitySoundPosition( centity_t *cent ) {
	if ( cent->currentState.solid == SOLID_BMODEL ) {
		vec3_t	origin;
		float	*v;

		v = cgs.inlineModelMidpoints[ cent->currentState.modelindex ];
		VectorAdd( cent->lerpOrigin, v, origin );
		trap_S_UpdateEntityPosition( cent->currentState.number, origin );
	} else {
		trap_S_UpdateEntityPosition( cent->currentState.number, cent->lerpOrigin );
	}
}

/*
==================
CG_EntityEffects

Add continuous entity effects, like local entity emission and lighting
==================
*/
static void CG_EntityEffects( centity_t *cent ) {

	// update sound origins
	CG_SetEntitySoundPosition( cent );

	// add loop sound
	if ( cent->currentState.loopSound ) {
		if (cent->currentState.eType != ET_SPEAKER) {
			trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin,
				cgs.gameSounds[ cent->currentState.loopSound ] );
		} else {
			trap_S_AddRealLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin,
				cgs.gameSounds[ cent->currentState.loopSound ] );
		}
	}


	// constant light glow
	if(cent->currentState.constantLight)
	{
		int		cl;
		float		i, r, g, b;

		cl = cent->currentState.constantLight;
		r = (float) (cl & 0xFF) / 255.0;
		g = (float) ((cl >> 8) & 0xFF) / 255.0;
		b = (float) ((cl >> 16) & 0xFF) / 255.0;
		i = (float) ((cl >> 24) & 0xFF) * 4.0;
		trap_R_AddLightToScene(cent->lerpOrigin, i, r, g, b);
	}

}


/*
==================
CG_General
==================
*/
static void CG_General( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// if set to invisible, skip
	if (!s1->modelindex) {
		return;
	}

	memset (&ent, 0, sizeof(ent));

	// set frame — a Q1 monster (a model with a derived monster anim table) is
	// driven from its entityState.legsAnim code (client-side tick + lerp, smooth);
	// any other generic entity uses its server-set s.frame directly (snap).
	{
		int   mframe, moldframe;
		float mbacklerp;
		if ( CG_MonsterAnimation( cent, &mframe, &moldframe, &mbacklerp ) ) {
			ent.frame = mframe;
			ent.oldframe = moldframe;
			ent.backlerp = mbacklerp;
		} else {
			ent.frame = s1->frame;
			ent.oldframe = ent.frame;
			ent.backlerp = 0;
		}
	}

	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	ent.hModel = cgs.gameModels[s1->modelindex];

	// player model
	if (s1->number == cg.snap->ps.clientNum) {
		ent.renderfx |= RF_THIRD_PERSON;	// only draw from mirrors
	}

	// convert angles to axis
	AnglesToAxis( cent->lerpAngles, ent.axis );

#if FEAT_SCREENSHOT_TOOLS
	if ( cg.stopTime ) ent.shaderTime.f = (float)( cg.time - cg.stopTime ) / 1000.0f;
#endif

	CG_Q1_MaybeEmitTrail( cent );

	// add to refresh list
	CG_AddRefEntityTemporalBase( cent, &ent, REF_ENTITY_MOTION_ROLE_GENERAL );
}

/*
==================
CG_Speaker

Speaker entities can automatically play sounds
==================
*/
static void CG_Speaker( centity_t *cent ) {
	if ( ! cent->currentState.clientNum ) {	// FIXME: use something other than clientNum...
		return;		// not auto triggering
	}

	if ( cg.time < cent->miscTime ) {
		return;
	}

	trap_S_StartSound (NULL, cent->currentState.number, CHAN_ITEM, cgs.gameSounds[cent->currentState.eventParm] );

	//	ent->s.frame = ent->wait * 10;
	//	ent->s.clientNum = ent->random * 10;
	cent->miscTime = cg.time + cent->currentState.frame * 100 + cent->currentState.clientNum * 100 * crandom();
}

/*
==================
CG_Item
==================
*/
static void CG_Item( centity_t *cent ) {
	refEntity_t		ent;
	entityState_t	*es;
	gitem_t			*item;
	int				msec;
	float			frac;
	float			scale;
	weaponInfo_t	*wi;

	es = &cent->currentState;
	if ( es->modelindex >= bg_numItems ) {
		Com_Terminate( TERM_CLIENT_DROP, "Bad item index %i on entity", es->modelindex );
	}

	// if set to invisible, skip
	if ( !es->modelindex || ( es->eFlags & EF_NODRAW ) ) {
		return;
	}

	item = &bg_itemlist[ es->modelindex ];
	if ( cg_simpleItems.integer && item->giType != IT_TEAM ) {
		memset( &ent, 0, sizeof( ent ) );
		ent.reType = RT_SPRITE;
		VectorCopy( cent->lerpOrigin, ent.origin );
		ent.radius = 14;
		// ent.customShader = cg_items[es->modelindex].icon;

		// PM: Draw backpack shader if it's a backpack
        if (es->eFlags & EF_BACKPACK)
            ent.customShader = cgs.media.backpackIcon;
        else
            ent.customShader = cg_items[es->modelindex].icon;

		ent.shaderRGBA[0] = 255;
		ent.shaderRGBA[1] = 255;
		ent.shaderRGBA[2] = 255;
		ent.shaderRGBA[3] = 255;
		trap_R_AddRefEntityToScene(&ent);
		return;
	}

	// items bob up and down continuously
	scale = 0.005 + cent->currentState.number * 0.00001;
	cent->lerpOrigin[2] += 4 + cos( ( cg.time + 1000 ) *  scale ) * 4;

	memset (&ent, 0, sizeof(ent));

	// autorotate at one of two speeds
	if ( item->giType == IT_HEALTH ) {
		VectorCopy( cg.autoAnglesFast, cent->lerpAngles );
		AxisCopy( cg.autoAxisFast, ent.axis );
	} else {
		VectorCopy( cg.autoAngles, cent->lerpAngles );
		AxisCopy( cg.autoAxis, ent.axis );
	}

	wi = NULL;
	// the weapons have their origin where they attatch to player
	// models, so we need to offset them or they will rotate
	// eccentricly
	if ( item->giType == IT_WEAPON ) {
		wi = &cg_weapons[item->giTag];
		cent->lerpOrigin[0] -=
			wi->weaponMidpoint[0] * ent.axis[0][0] +
			wi->weaponMidpoint[1] * ent.axis[1][0] +
			wi->weaponMidpoint[2] * ent.axis[2][0];
		cent->lerpOrigin[1] -=
			wi->weaponMidpoint[0] * ent.axis[0][1] +
			wi->weaponMidpoint[1] * ent.axis[1][1] +
			wi->weaponMidpoint[2] * ent.axis[2][1];
		cent->lerpOrigin[2] -=
			wi->weaponMidpoint[0] * ent.axis[0][2] +
			wi->weaponMidpoint[1] * ent.axis[1][2] +
			wi->weaponMidpoint[2] * ent.axis[2][2];

		cent->lerpOrigin[2] += 8;	// an extra height boost
	}

	// if( item->giType == IT_WEAPON && item->giTag == WP_RAILGUN ) {
    //     ent.shaderRGBA[0] = colorSkyBlue[0] * 255;
    //     ent.shaderRGBA[1] = colorSkyBlue[1] * 255;
    //     ent.shaderRGBA[2] = colorSkyBlue[2] * 255;
    //     ent.shaderRGBA[3] = 255;
	// }

	// if( item->giType == IT_ARMOR ) {
	// 	switch ( item->giTag ) {
    //         case ARM_HEAVY:  VectorSet( ent.shaderRGBA, 255,   0,   0 ); break;
    //         case ARM_COMBAT: VectorSet( ent.shaderRGBA, 255, 255,   0 ); break;
    //         case ARM_JACKET: VectorSet( ent.shaderRGBA,   0,   0, 255 ); break;
    //         default:         VectorSet( ent.shaderRGBA, 255, 255, 255 ); break;
    //     }
    //     ent.shaderRGBA[3] = 255;
	// }

	// ent.hModel = cg_items[es->modelindex].models[0];

	// PM: Use backpack model
    if (es->eFlags & EF_BACKPACK)
        ent.hModel = cgs.media.backpackModel;
    else
        ent.hModel = cg_items[es->modelindex].models[0];

	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	ent.nonNormalizedAxes = qfalse;

	// if just respawned, slowly scale up
	msec = cg.time - cent->miscTime;
	if ( msec >= 0 && msec < ITEM_SCALEUP_TIME ) {
		frac = (float)msec / ITEM_SCALEUP_TIME;
		VectorScale( ent.axis[0], frac, ent.axis[0] );
		VectorScale( ent.axis[1], frac, ent.axis[1] );
		VectorScale( ent.axis[2], frac, ent.axis[2] );
		ent.nonNormalizedAxes = qtrue;
	} else {
		frac = 1.0;
	}

	// items without glow textures need to keep a minimum light value
	// so they are always visible
	if ( ( item->giType == IT_WEAPON ) ||
		 ( item->giType == IT_ARMOR ) ) {
		ent.renderfx |= RF_MINLIGHT;
	}

	// increase the size of the weapons when they are presented as items
	// if ( item->giType == IT_WEAPON ) {
    if (item->giType == IT_WEAPON && !(es->eFlags & EF_BACKPACK)) { // PM: don't scale backpacks
		VectorScale( ent.axis[0], 1.5, ent.axis[0] );
		VectorScale( ent.axis[1], 1.5, ent.axis[1] );
		VectorScale( ent.axis[2], 1.5, ent.axis[2] );
		ent.nonNormalizedAxes = qtrue;
		trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin, cgs.media.weaponHoverSound );
	}

	if ( item->giType == IT_HOLDABLE && item->giTag == HI_KAMIKAZE ) {
		VectorScale( ent.axis[0], 2, ent.axis[0] );
		VectorScale( ent.axis[1], 2, ent.axis[1] );
		VectorScale( ent.axis[2], 2, ent.axis[2] );
		ent.nonNormalizedAxes = qtrue;
	}

	// add to refresh list
	CG_AddRefEntityTemporalBase( cent, &ent,
		REF_ENTITY_MOTION_ROLE_ITEM_PRIMARY );

	if ( item->giType == IT_WEAPON && wi && wi->barrelModel && !(es->eFlags & EF_BACKPACK) ) {
		refEntity_t	barrel;
		vec3_t		angles;

		memset( &barrel, 0, sizeof( barrel ) );

		barrel.hModel = wi->barrelModel;

		VectorCopy( ent.lightingOrigin, barrel.lightingOrigin );
		barrel.renderfx = ent.renderfx;

		angles[YAW] = 0;
		angles[PITCH] = 0;
		angles[ROLL] = 0;
		AnglesToAxis( angles, barrel.axis );

		CG_PositionRotatedEntityOnTag( &barrel, &ent, wi->weaponModel, "tag_barrel" );

		barrel.nonNormalizedAxes = ent.nonNormalizedAxes;

		CG_AddRefEntityTemporalBase( cent, &barrel,
			REF_ENTITY_MOTION_ROLE_ITEM_BARREL );
	}

	// accompanying rings / spheres for powerups
	if ( !cg_simpleItems.integer )
	{
		vec3_t spinAngles;

		VectorClear( spinAngles );

		if ( item->giType == IT_HEALTH || item->giType == IT_POWERUP )
		{
			if ( ( ent.hModel = cg_items[es->modelindex].models[1] ) != 0 )
			{
				if ( item->giType == IT_POWERUP )
				{
					ent.origin[2] += 12;
					spinAngles[1] = ( cg.time & 1023 ) * 360 / -1024.0f;
				}
				AnglesToAxis( spinAngles, ent.axis );

				// scale up if respawning
				if ( frac != 1.0 ) {
					VectorScale( ent.axis[0], frac, ent.axis[0] );
					VectorScale( ent.axis[1], frac, ent.axis[1] );
					VectorScale( ent.axis[2], frac, ent.axis[2] );
					ent.nonNormalizedAxes = qtrue;
				}
				trap_R_AddRefEntityToScene( &ent );
			}
		}
#if FEAT_LENS_FLARES
		if ( item->giType == IT_POWERUP ) {
			CG_AddPowerupFlare( cent, item->giTag );
		}
#endif
	}
}

//============================================================================

/*
===============
CG_Missile
===============
*/
static void CG_Missile( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;
	const weaponInfo_t		*weapon;
//	int	col;

	s1 = &cent->currentState;
	if ( s1->weapon >= WP_NUM_WEAPONS ) {
		s1->weapon = 0;
	}
	weapon = &cg_weapons[s1->weapon];

	// calculate the axis
	VectorCopy( s1->angles, cent->lerpAngles);

#if FEAT_ZNUDGE
	// znudge: extrapolate missile position forward by ping
	if ( cg_znudge.integer && cg_znProjectiles.integer ) {
		float nudge = ZN_GetNudge();
		if ( s1->weapon == WP_ROCKET_LAUNCHER ||
		     s1->weapon == WP_PLASMA_RIFLE ) {
			ZN_PredictMissile( cent, nudge, cent->lerpOrigin );
		} else if ( s1->weapon == WP_GRENADE_LAUNCHER ) {
			ZN_PredictGrenade( cent, nudge, cent->lerpOrigin );
		}
	}
#endif

	// add trails
	if ( weapon->missileTrailFunc )
	{
		weapon->missileTrailFunc( cent, weapon );
	}
/*
	if ( cent->currentState.modelindex == TEAM_RED ) {
		col = 1;
	}
	else if ( cent->currentState.modelindex == TEAM_BLUE ) {
		col = 2;
	}
	else {
		col = 0;
	}

	// add dynamic light
	if ( weapon->missileDlight ) {
		trap_R_AddLightToScene(cent->lerpOrigin, weapon->missileDlight,
			weapon->missileDlightColor[col][0], weapon->missileDlightColor[col][1], weapon->missileDlightColor[col][2] );
	}
*/
	// add dynamic light
	if ( weapon->missileDlight ) {
		trap_R_AddLightToScene(cent->lerpOrigin, weapon->missileDlight,
			weapon->missileDlightColor[0], weapon->missileDlightColor[1], weapon->missileDlightColor[2] );
	}

	// add missile sound
	if ( weapon->missileSound ) {
		vec3_t	velocity;

		BG_EvaluateTrajectoryDelta( &cent->currentState.pos, cg.time, velocity );

		trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, velocity, weapon->missileSound );
	}

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	if ( cent->currentState.pType == PROJ_PLASMA ) {
		ent.reType = RT_SPRITE;
		ent.radius = 16;
		ent.rotation = 0;
		ent.customShader = cgs.media.plasmaBallShader;
		trap_R_AddRefEntityToScene( &ent );
		return;
	}

    if (weapon->missileModel == 0) {
        return;
    }

	// flicker between two skins
	ent.skinNum = cg.clientFrame & 1;
    ent.hModel = weapon->missileModel;
	ent.renderfx = weapon->missileRenderfx | RF_NOSHADOW;

	// convert direction of travel into axis
	if ( VectorNormalize2( s1->pos.trDelta, ent.axis[0] ) == 0 ) {
		ent.axis[0][2] = 1;
	}

	// spin as it moves
	if ( s1->pos.trType != TR_STATIONARY ) {
#if FEAT_SCREENSHOT_TOOLS
		if ( cg.stopTime ) {
			RotateAroundDirection( ent.axis, cg.stopTime / 4 );
		} else
#endif
		RotateAroundDirection( ent.axis, cg.time / 4 );
	} else {
		RotateAroundDirection( ent.axis, s1->time );
	}

	// add to refresh list, possibly with quad glow
	CG_AddRefEntityWithPowerups( cent, &ent, s1, qfalse, TEAM_FREE,
		REF_ENTITY_MOTION_ROLE_NONE );
}

/*
===============
CG_Grapple

This is called when the grapple is sitting up against the wall
===============
*/
static void CG_Grapple( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;
	const weaponInfo_t		*weapon;

	s1 = &cent->currentState;
	if ( s1->weapon >= WP_NUM_WEAPONS ) {
		s1->weapon = 0;
	}
	weapon = &cg_weapons[s1->weapon];

	// calculate the axis
	VectorCopy( s1->angles, cent->lerpAngles);

#if 0 // FIXME add grapple pull sound here..?
	// add missile sound
	if ( weapon->missileSound ) {
		trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin, weapon->missileSound );
	}
#endif

	// Will draw cable if needed
	CG_GrappleTrail ( cent, weapon );

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

    if (weapon->missileModel == 0) {
        return;
    }

	// flicker between two skins
	ent.skinNum = cg.clientFrame & 1;
	ent.hModel = weapon->missileModel;
	ent.renderfx = weapon->missileRenderfx | RF_NOSHADOW;

	// convert direction of travel into axis
	if ( VectorNormalize2( s1->pos.trDelta, ent.axis[0] ) == 0 ) {
		ent.axis[0][2] = 1;
	}

	CG_AddRefEntityTemporalBase( cent, &ent,
		REF_ENTITY_MOTION_ROLE_GRAPPLE );
}

/*
===============
CG_Mover
===============
*/
static void CG_Mover( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);
	AnglesToAxis( cent->lerpAngles, ent.axis );

	ent.renderfx = RF_NOSHADOW;

	// BSP brush movers have a single lightmap style; alternating skinNum
	// causes visual artifacts. Non-BSP movers may legitimately cycle skins.
	if ( s1->solid == SOLID_BMODEL ) {
		ent.skinNum = 0;
	} else {
		ent.skinNum = ( cg.time >> 6 ) & 1;
	}

	// get the model, either as a bmodel or a modelindex
	if ( s1->solid == SOLID_BMODEL ) {
		ent.hModel = cgs.inlineDrawModel[s1->modelindex];
	} else {
		ent.hModel = cgs.gameModels[s1->modelindex];
	}

	ent.frame    = s1->frame;
	ent.oldframe = s1->frame;

	{
		static int mover_diag_count = 0;
		if ( mover_diag_count < 40 ) {
			mover_diag_count++;
			Com_Log( SEV_TRACE, LOG_CH(ch_cgame), "CG_Mover[%d]: num=%d s1->solid=0x%x SOLID_BMODEL=0x%x"
				" s1->modelindex=%d hModel=%d"
				" lerpOrigin=(%.0f,%.0f,%.0f)\n",
				mover_diag_count, s1->number,
				(unsigned)s1->solid, (unsigned)SOLID_BMODEL,
				s1->modelindex, ent.hModel,
				cent->lerpOrigin[0], cent->lerpOrigin[1], cent->lerpOrigin[2] );
		}
	}

	// add to refresh list
	CG_AddRefEntityTemporalBase( cent, &ent,
		REF_ENTITY_MOTION_ROLE_MOVER_PRIMARY );

	// add the secondary model
	if ( s1->modelindex2 ) {
		ent.skinNum = 0;
		ent.hModel = cgs.gameModels[s1->modelindex2];
		CG_AddRefEntityTemporalBase( cent, &ent,
			REF_ENTITY_MOTION_ROLE_MOVER_SECONDARY );
	}

}

/*
===============
CG_Beam

Also called as an event
===============
*/
void CG_Beam( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( s1->pos.trBase, ent.origin );
	VectorCopy( s1->origin2, ent.oldorigin );
	AxisClear( ent.axis );
	ent.reType = RT_BEAM;

	ent.renderfx = RF_NOSHADOW;

	// add to refresh list
	trap_R_AddRefEntityToScene(&ent);
}


/*
===============
CG_Portal
===============
*/
static void CG_Portal( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin );
	VectorCopy( s1->origin2, ent.oldorigin );
	ByteToDir( s1->eventParm, ent.axis[0] );
	PerpendicularVector( ent.axis[1], ent.axis[0] );

	// negating this tends to get the directions like they want
	// we really should have a camera roll value
	VectorSubtract( vec3_origin, ent.axis[1], ent.axis[1] );

	CrossProduct( ent.axis[0], ent.axis[1], ent.axis[2] );
	ent.reType = RT_PORTALSURFACE;
	ent.oldframe = s1->powerups;
	ent.frame = s1->frame;		// rotation speed
	ent.skinNum = s1->clientNum/256.0 * 360;	// roll offset

	// add to refresh list
	trap_R_AddRefEntityToScene(&ent);
}


/*
================
CG_CreateRotationMatrix
================
*/
void CG_CreateRotationMatrix(vec3_t angles, vec3_t matrix[3]) {
	AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
	VectorInverse(matrix[1]);
}

/*
================
CG_TransposeMatrix
================
*/
void CG_TransposeMatrix(vec3_t matrix[3], vec3_t transpose[3]) {
	int i, j;
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			transpose[i][j] = matrix[j][i];
		}
	}
}

/*
================
CG_RotatePoint
================
*/
void CG_RotatePoint(vec3_t point, vec3_t matrix[3]) {
	vec3_t tvec;

	VectorCopy(point, tvec);
	point[0] = DotProduct(matrix[0], tvec);
	point[1] = DotProduct(matrix[1], tvec);
	point[2] = DotProduct(matrix[2], tvec);
}

/*
=========================
CG_AdjustPositionForMover

Also called by client movement prediction code
=========================
*/
void CG_AdjustPositionForMover(const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out, vec3_t angles_in, vec3_t angles_out) {
	centity_t	*cent;
	vec3_t	oldOrigin, origin, deltaOrigin;
	vec3_t	oldAngles, angles, deltaAngles;
	vec3_t	matrix[3], transpose[3];
	vec3_t	org, org2, move2;

	if ( moverNum <= 0 || moverNum >= ENTITYNUM_MAX_NORMAL ) {
		VectorCopy( in, out );
		VectorCopy(angles_in, angles_out);
		return;
	}

	cent = &cg_entities[ moverNum ];
	if ( cent->currentState.eType != ET_MOVER ) {
		VectorCopy( in, out );
		VectorCopy(angles_in, angles_out);
		return;
	}

	BG_EvaluateTrajectory( &cent->currentState.pos, fromTime, oldOrigin );
	BG_EvaluateTrajectory( &cent->currentState.apos, fromTime, oldAngles );

	BG_EvaluateTrajectory( &cent->currentState.pos, toTime, origin );
	BG_EvaluateTrajectory( &cent->currentState.apos, toTime, angles );

	VectorSubtract( origin, oldOrigin, deltaOrigin );
	VectorSubtract( angles, oldAngles, deltaAngles );

	// origin change when on a rotating object
	CG_CreateRotationMatrix( deltaAngles, transpose );
	CG_TransposeMatrix( transpose, matrix );
	VectorSubtract( in, oldOrigin, org );
	VectorCopy( org, org2 );
	CG_RotatePoint( org2, matrix );
	VectorSubtract( org2, org, move2 );
	VectorAdd( deltaOrigin, move2, deltaOrigin );

	VectorAdd( in, deltaOrigin, out );
	VectorAdd( angles_in, deltaAngles, angles_out );
}


/*
=============================
CG_InterpolateEntityPosition
=============================
*/
static void CG_InterpolateEntityPosition( centity_t *cent ) {
	vec3_t		current, next;
	float		f;

	// it would be an internal error to find an entity that interpolates without
	// a snapshot ahead of the current one
	if ( cg.nextSnap == NULL ) {
		Com_Terminate( TERM_CLIENT_DROP, "CG_InterpoateEntityPosition: cg.nextSnap == NULL" );
	}

	f = cg.frameInterpolation;

	// this will linearize a sine or parabolic curve, but it is important
	// to not extrapolate player positions if more recent data is available
	BG_EvaluateTrajectory( &cent->currentState.pos, cg.snap->serverTime, current );
	BG_EvaluateTrajectory( &cent->nextState.pos, cg.nextSnap->serverTime, next );

	cent->lerpOrigin[0] = current[0] + f * ( next[0] - current[0] );
	cent->lerpOrigin[1] = current[1] + f * ( next[1] - current[1] );
	cent->lerpOrigin[2] = current[2] + f * ( next[2] - current[2] );

	BG_EvaluateTrajectory( &cent->currentState.apos, cg.snap->serverTime, current );
	BG_EvaluateTrajectory( &cent->nextState.apos, cg.nextSnap->serverTime, next );

	cent->lerpAngles[0] = LerpAngle( current[0], next[0], f );
	cent->lerpAngles[1] = LerpAngle( current[1], next[1], f );
	cent->lerpAngles[2] = LerpAngle( current[2], next[2], f );

}

/*
===============
CG_CalcEntityLerpPositions

===============
*/
static void CG_CalcEntityLerpPositions( centity_t *cent ) {

	// if this player does not want to see extrapolated players
	if ( !cg_smoothClients.integer ) {
		// make sure the clients use TR_INTERPOLATE
		if ( cent->currentState.number < MAX_CLIENTS ) {
			cent->currentState.pos.trType = TR_INTERPOLATE;
			cent->nextState.pos.trType = TR_INTERPOLATE;
		}
	}

	if ( cent->interpolate && cent->currentState.pos.trType == TR_INTERPOLATE ) {
		CG_InterpolateEntityPosition( cent );
		return;
	}

	// first see if we can interpolate between two snaps for
	// linear extrapolated clients
	if ( cent->interpolate && cent->currentState.pos.trType == TR_LINEAR_STOP &&
											cent->currentState.number < MAX_CLIENTS) {
		CG_InterpolateEntityPosition( cent );
		return;
	}

	// just use the current frame and evaluate as best we can
	BG_EvaluateTrajectory( &cent->currentState.pos, cg.time, cent->lerpOrigin );
	BG_EvaluateTrajectory( &cent->currentState.apos, cg.time, cent->lerpAngles );

	// adjust for riding a mover if it wasn't rolled into the predicted
	// player state
	if ( cent != &cg.predictedPlayerEntity ) {
		CG_AdjustPositionForMover( cent->lerpOrigin, cent->currentState.groundEntityNum,
		cg.snap->serverTime, cg.time, cent->lerpOrigin, cent->lerpAngles, cent->lerpAngles);
	}
}

/*
===============
CG_TeamBase
===============
*/
static void CG_TeamBase( centity_t *cent ) {
	refEntity_t model;
#if FEAT_OBELISK
	vec3_t angles;
	int t, h;
	float c;
#endif

	if ( cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF ) {
	// show the flag base
		memset(&model, 0, sizeof(model));
		model.reType = RT_MODEL;
		VectorCopy( cent->lerpOrigin, model.lightingOrigin );
		VectorCopy( cent->lerpOrigin, model.origin );
		AnglesToAxis( cent->currentState.angles, model.axis );
		if ( cent->currentState.modelindex == TEAM_RED ) {
			model.hModel = cgs.media.redFlagBaseModel;
		}
		else if ( cent->currentState.modelindex == TEAM_BLUE ) {
			model.hModel = cgs.media.blueFlagBaseModel;
		}
		else {
			model.hModel = cgs.media.neutralFlagBaseModel;
		}
		trap_R_AddRefEntityToScene( &model );
	}
#if FEAT_OBELISK
	else if ( cgs.gametype == GT_OBELISK ) {
		// show the obelisk
		memset(&model, 0, sizeof(model));
		model.reType = RT_MODEL;
		VectorCopy( cent->lerpOrigin, model.lightingOrigin );
		VectorCopy( cent->lerpOrigin, model.origin );
		AnglesToAxis( cent->currentState.angles, model.axis );

		model.hModel = cgs.media.overloadBaseModel;
		trap_R_AddRefEntityToScene( &model );
		// if hit
		if ( cent->currentState.frame == 1) {
			// show hit model
			// modelindex2 is the health value of the obelisk
			c = cent->currentState.modelindex2;
			model.shaderRGBA[0] = 0xff;
			model.shaderRGBA[1] = c;
			model.shaderRGBA[2] = c;
			model.shaderRGBA[3] = 0xff;
			//
			model.hModel = cgs.media.overloadEnergyModel;
			trap_R_AddRefEntityToScene( &model );
		}
		// if respawning
		if ( cent->currentState.frame == 2) {
			if ( !cent->miscTime ) {
				cent->miscTime = cg.time;
			}
			t = cg.time - cent->miscTime;
			h = (cg_obeliskRespawnDelay.integer - 5) * 1000;
			//
			if (t > h) {
				c = (float) (t - h) / h;
				if (c > 1)
					c = 1;
			}
			else {
				c = 0;
			}
			// show the lights
			AnglesToAxis( cent->currentState.angles, model.axis );
			//
			model.shaderRGBA[0] = c * 0xff;
			model.shaderRGBA[1] = c * 0xff;
			model.shaderRGBA[2] = c * 0xff;
			model.shaderRGBA[3] = c * 0xff;

			model.hModel = cgs.media.overloadLightsModel;
			trap_R_AddRefEntityToScene( &model );
			// show the target
			if (t > h) {
				if ( !cent->muzzleFlashTime ) {
					trap_S_StartSound (cent->lerpOrigin, ENTITYNUM_NONE, CHAN_BODY,  cgs.media.obeliskRespawnSound);
					cent->muzzleFlashTime = 1;
				}
				VectorCopy(cent->currentState.angles, angles);
				angles[YAW] += (float) 16 * acos(1-c) * 180 / M_PI;
				AnglesToAxis( angles, model.axis );

				VectorScale( model.axis[0], c, model.axis[0]);
				VectorScale( model.axis[1], c, model.axis[1]);
				VectorScale( model.axis[2], c, model.axis[2]);

				model.shaderRGBA[0] = 0xff;
				model.shaderRGBA[1] = 0xff;
				model.shaderRGBA[2] = 0xff;
				model.shaderRGBA[3] = 0xff;
				//
				model.origin[2] += 56;
				model.hModel = cgs.media.overloadTargetModel;
				trap_R_AddRefEntityToScene( &model );
			}
			else {
				//FIXME: show animated smoke
			}
		}
		else {
			cent->miscTime = 0;
			cent->muzzleFlashTime = 0;
			// modelindex2 is the health value of the obelisk
			c = cent->currentState.modelindex2;
			model.shaderRGBA[0] = 0xff;
			model.shaderRGBA[1] = c;
			model.shaderRGBA[2] = c;
			model.shaderRGBA[3] = 0xff;
			// show the lights
			model.hModel = cgs.media.overloadLightsModel;
			trap_R_AddRefEntityToScene( &model );
			// show the target
			model.origin[2] += 56;
			model.hModel = cgs.media.overloadTargetModel;
			trap_R_AddRefEntityToScene( &model );
		}
	}
#endif
#if FEAT_HARVESTER
	else if ( cgs.gametype == GT_HARVESTER ) {
		// show harvester model
		memset(&model, 0, sizeof(model));
		model.reType = RT_MODEL;
		VectorCopy( cent->lerpOrigin, model.lightingOrigin );
		VectorCopy( cent->lerpOrigin, model.origin );
		AnglesToAxis( cent->currentState.angles, model.axis );

		if ( cent->currentState.modelindex == TEAM_RED ) {
			model.hModel = cgs.media.harvesterModel;
			model.customSkin = cgs.media.harvesterRedSkin;
		}
		else if ( cent->currentState.modelindex == TEAM_BLUE ) {
			model.hModel = cgs.media.harvesterModel;
			model.customSkin = cgs.media.harvesterBlueSkin;
		}
		else {
			model.hModel = cgs.media.harvesterNeutralModel;
			model.customSkin = 0;
		}
		trap_R_AddRefEntityToScene( &model );
	}
#endif
}

/*
===============
CG_AddCEntity

===============
*/
static void CG_AddCEntity( centity_t *cent ) {
	// event-only entities will have been dealt with already
	if ( cent->currentState.eType >= ET_EVENTS ) {
		return;
	}

	// calculate the current origin
	CG_CalcEntityLerpPositions( cent );

	// add automatic effects
	CG_EntityEffects( cent );

	switch ( cent->currentState.eType ) {
	default:
		Com_Terminate( TERM_CLIENT_DROP, "Bad entity type: %i", cent->currentState.eType );
		break;
	case ET_INVISIBLE:
	case ET_PUSH_TRIGGER:
	case ET_TELEPORT_TRIGGER:
		break;
	case ET_GENERAL:
#if FEAT_IQM
		// A behavior monster (a Q1 monster model) that names a loadable character
		// renders as that character through the single-mesh body path; anything else —
		// a plain generic entity, or a monster with no loadable character body — renders
		// through CG_General (which keeps the .mdl fallback for such monsters).
		if ( CG_IsMonsterModel( cent->currentState.modelindex ) && CG_CreatureRenders( cent ) ) {
			CG_Creature( cent );
		} else
#endif
		{
			CG_General( cent );
		}
		break;
	case ET_PLAYER:
		CG_Player( cent );
		break;
	case ET_ITEM:
		CG_Item( cent );
		break;
	case ET_MISSILE:
		CG_Missile( cent );
#if FEAT_LENS_FLARES
		CG_AddMissileFlare( cent );
#endif
		break;
	case ET_MOVER:
		CG_Mover( cent );
		break;
	case ET_BEAM:
		CG_Beam( cent );
		break;
	case ET_PORTAL:
		CG_Portal( cent );
		break;
	case ET_SPEAKER:
		CG_Speaker( cent );
		break;
	case ET_GRAPPLE:
		CG_Grapple( cent );
		break;
	case ET_TEAM:
		CG_TeamBase( cent );
		break;
	}
}

/*
===============
CG_AddPacketEntities

===============
*/
void CG_AddPacketEntities( void ) {
	int					num;
	centity_t			*cent;
	playerState_t		*ps;

#if FEAT_SCREENSHOT_TOOLS
	cg.time -= cg.serverOffset;
#endif

	// set cg.frameInterpolation
	if ( cg.nextSnap ) {
		int		delta;

		delta = (cg.nextSnap->serverTime - cg.snap->serverTime);
		if ( delta == 0 ) {
			cg.frameInterpolation = 0;
		} else {
			cg.frameInterpolation = (float)( cg.time - cg.snap->serverTime ) / delta;
		}
	} else {
		cg.frameInterpolation = 0;	// actually, it should never be used, because
									// no entities should be marked as interpolating
	}

	// the auto-rotating items will all have the same axis
	cg.autoAngles[0] = 0;
	cg.autoAngles[1] = ( cg.time & 2047 ) * 360 / 2048.0;
	cg.autoAngles[2] = 0;

	cg.autoAnglesFast[0] = 0;
	cg.autoAnglesFast[1] = ( cg.time & 1023 ) * 360 / 1024.0f;
	cg.autoAnglesFast[2] = 0;

	AnglesToAxis( cg.autoAngles, cg.autoAxis );
	AnglesToAxis( cg.autoAnglesFast, cg.autoAxisFast );

	// generate and add the entity from the playerstate
	ps = &cg.predictedPlayerState;
	BG_PlayerStateToEntityState( ps, &cg.predictedPlayerEntity.currentState, qfalse );
	CG_AddCEntity( &cg.predictedPlayerEntity );

	// lerp the non-predicted value for lightning gun origins
	CG_CalcEntityLerpPositions( &cg_entities[ cg.snap->ps.clientNum ] );

	// add each entity sent over by the server
	{
		static int snap_diag_logged = 0;
		if ( snap_diag_logged < 3 && cg.snap->numEntities >= 3 ) {
			snap_diag_logged++;
			Com_Log( SEV_TRACE, LOG_CH(ch_cgame), "CG_AddPacketEntities[snap%d]: numEntities=%d\n",
				snap_diag_logged, cg.snap->numEntities );
			for ( num = 0; num < cg.snap->numEntities; num++ ) {
				entityState_t *es = &cg.snap->entities[ num ];
				Com_Log( SEV_TRACE, LOG_CH(ch_cgame), "  snap_ent[%d]: number=%d eType=%d solid=0x%x modelindex=%d origin=(%.0f,%.0f,%.0f)\n",
					num, es->number, es->eType, (unsigned)es->solid, es->modelindex,
					es->pos.trBase[0], es->pos.trBase[1], es->pos.trBase[2] );
			}
		}
	}
	for ( num = 0 ; num < cg.snap->numEntities ; num++ ) {
		cent = &cg_entities[ cg.snap->entities[ num ].number ];
		CG_AddCEntity( cent );
	}

	CG_DrawEntityBoxes();

#if FEAT_SCREENSHOT_TOOLS
	cg.time += cg.serverOffset;
#endif
}


/*
===============
CG_BoxColorForEntity

Give each entity a stable, distinguishable colour so several boxes in one frame
can be told apart, and so the same entity keeps its colour across frames. Derived
from the entity number (not the index into the snapshot, which shifts frame to
frame) via a small hash spread across the six saturated hue corners.
===============
*/
static void CG_BoxColorForEntity( int entityNum, byte rgba[4], byte alpha ) {
	static const byte hues[6][3] = {
		{ 255,  64,  64 },   /* red    */
		{  64, 255,  64 },   /* green  */
		{  64, 160, 255 },   /* blue   */
		{ 255, 255,  64 },   /* yellow */
		{ 255,  64, 255 },   /* magenta*/
		{  64, 255, 255 },   /* cyan   */
	};
	const byte *h = hues[ ((unsigned) entityNum * 2654435761u >> 13) % 6 ];

	rgba[0] = h[0];
	rgba[1] = h[1];
	rgba[2] = h[2];
	rgba[3] = alpha;
}


/*
===============
CG_AddBoxQuads

Emit an axis-aligned box as a WIREFRAME: 12 edges, each a thin camera-agnostic
quad, rather than 6 solid faces.

Solid faces were the first attempt and they defeat the tool's own purpose: the
near face covers the very model you are trying to look at, so "is the model
inside this box?" becomes unanswerable exactly when you need to ask it. Even at
low alpha, a translucent face washes the model out to a faint smear. Edges leave
the interior clear and still convey position and extent.

Polys are the only geometry primitive cgame can hand to the renderer
(trap_R_AddPolyToScene), and they are added as SF_POLY draw surfs owned by
REFENTITYNUM_WORLD, so they depth-test against the world like ordinary geometry.
There is no line primitive and no nodepth shader available here, hence edges are
drawn as thin quads.

Each edge is a separate 4-vert poly rather than one big submission because
RE_AddPolyToScene treats a multi-poly submission as a flat array of
numPolys * numVerts and requires every poly to share numVerts.
===============
*/
static void CG_AddBoxQuads( const vec3_t mins, const vec3_t maxs, const byte rgba[4] ) {
	/* The 8 corners, indexed by bit: x = bit0, y = bit1, z = bit2. */
	vec3_t corner[8];
	/* The 12 edges, as pairs of corner indices. */
	static const int edges[12][2] = {
		{ 0,1 }, { 2,3 }, { 4,5 }, { 6,7 },   /* along x */
		{ 0,2 }, { 1,3 }, { 4,6 }, { 5,7 },   /* along y */
		{ 0,4 }, { 1,5 }, { 2,6 }, { 3,7 },   /* along z */
	};
	polyVert_t verts[4];
	int i, e, v;

	for ( i = 0; i < 8; i++ ) {
		corner[i][0] = ( i & 1 ) ? maxs[0] : mins[0];
		corner[i][1] = ( i & 2 ) ? maxs[1] : mins[1];
		corner[i][2] = ( i & 4 ) ? maxs[2] : mins[2];
	}

	for ( e = 0; e < 12; e++ ) {
		const float *a = corner[ edges[e][0] ];
		const float *b = corner[ edges[e][1] ];
		vec3_t dir, up, side;
		int axis;

		VectorSubtract( b, a, dir );
		if ( VectorNormalize( dir ) < 0.001f ) {
			continue;   /* zero-extent axis: the box is flat here, skip */
		}
		/* Widen the edge perpendicular to BOTH the edge and the view direction, so
		   the quad always faces the camera. Widening along a fixed world axis
		   instead — the obvious first try — makes every edge that happens to lie in
		   the plane containing that axis and the eye present itself edge-on, so it
		   thins to nothing: the box shows up as one or two stray bars and reads as a
		   broken box rather than a thin one. */
		CrossProduct( dir, cg.refdef.viewaxis[0], side );
		if ( VectorNormalize( side ) < 0.001f ) {
			/* Edge points straight at the eye; any perpendicular is as good as any
			   other. Pick the world axis it runs along least so the cross product
			   stays well-conditioned. */
			axis = ( fabs( dir[0] ) < fabs( dir[1] ) ) ? 0 : 1;
			if ( fabs( dir[2] ) < fabs( dir[axis] ) ) {
				axis = 2;
			}
			VectorClear( up );
			up[axis] = 1.0f;
			CrossProduct( dir, up, side );
			VectorNormalize( side );
		}
		VectorScale( side, CG_BBOX_EDGE_HALFWIDTH, side );

		VectorAdd( a, side, verts[0].xyz );
		VectorSubtract( a, side, verts[1].xyz );
		VectorSubtract( b, side, verts[2].xyz );
		VectorAdd( b, side, verts[3].xyz );

		for ( v = 0; v < 4; v++ ) {
			verts[v].st[0] = ( v == 1 || v == 2 ) ? 1.0f : 0.0f;
			verts[v].st[1] = ( v >= 2 ) ? 1.0f : 0.0f;
			verts[v].modulate.rgba[0] = rgba[0];
			verts[v].modulate.rgba[1] = rgba[1];
			verts[v].modulate.rgba[2] = rgba[2];
			verts[v].modulate.rgba[3] = rgba[3];
		}
		trap_R_AddPolyToScene( cgs.media.whiteShader, 4, verts );

		/* Again with the winding reversed. The default shader is single-sided, and
		   which way an edge quad happens to wind depends on the edge direction and
		   where the camera is — so roughly half the edges get backface-culled and the
		   box appears as one or two stray bars. Submitting both windings is cheaper
		   and more predictable than reasoning about the sign per edge. 12 edges x 2
		   = 24 polys per box, against an 8192 budget. */
		{
			polyVert_t flipped[4];
			flipped[0] = verts[3];
			flipped[1] = verts[2];
			flipped[2] = verts[1];
			flipped[3] = verts[0];
			trap_R_AddPolyToScene( cgs.media.whiteShader, 4, flipped );
		}
	}
}


/*
===============
CG_DrawEntityBoxes

cg_drawBBox: draw a box at every entity the client received this frame.

WHY THIS EXISTS. Debugging "the entity is missing" previously had no instrument
that was independent of the entity's own material and lighting. Turning the world
off (r_drawWorld 0) only shows an entity that happens to be bright enough to see,
so "not in the world", "outside the frustum" and "too dark to notice" all produce
the same black frame and cannot be told apart. A box is drawn from geometry we
compute here, in a flat unlit colour, so it appears whenever the entity is in
front of the camera — whatever its shader does.

TWO PASSES, and the pair is the point:

  cg_drawBBox 1   depth-tested. The box is occluded by world geometry exactly as
                  the entity is. Box visible => the entity is unoccluded.
  cg_drawBBox 2   also draws a second, larger, translucent box. Polys cannot
                  disable depth testing from cgame (no nodepth shader exists, and
                  RF_DEPTHHACK is a refEntity flag that does not apply to polys),
                  so "see it through walls" is approximated by inflating the box
                  until it pokes out of whatever encloses it.

Reading the result:
  neither box     the entity is not in the frame at all — look at the camera,
                  the snapshot, or whether it spawned. Not a render bug.
  outline only    the entity is in the frame but its model is not drawing —
                  a model/shader/lighting problem.
  both + model    everything is working.

BOUNDS SOURCE. trap_R_ModelBounds on the entity's model, NOT the encoded
entityState_t.solid bbox. solid carries the *collision* extent and is zero for
anything without CONTENTS_SOLID|CONTENTS_BODY — a CTF flag is a trigger, so its
solid box is a 30-unit cube that has nothing to do with how big the model looks,
and other items have no solid box at all. Model bounds are what "where is it on
screen" actually depends on. Entities with no model (ET_PLAYER composes several,
speakers and triggers have none) fall back to a small fixed marker cube so their
position is still visible.
===============
*/
void CG_DrawEntityBoxes( void ) {
	int num;
	int only;

	if ( !cg_drawBBox.integer ) {
		return;
	}

	/* cg_drawBBoxEnt isolates one entity number. A busy arena puts a box on every
	   item, and one large box (a player, a mover) covers the rest — which defeats
	   the tool exactly when the scene is complicated enough to need it. Entity
	   numbers come from `poscheck`, whose area list prints them. -1 = show all. */
	only = cg_drawBBoxEnt.integer;

	for ( num = 0; num < cg.snap->numEntities; num++ ) {
		const entityState_t *es = &cg.snap->entities[ num ];
		const centity_t     *cent = &cg_entities[ es->number ];
		vec3_t    mins, maxs, org;
		byte      rgba[4];
		qhandle_t hModel;
		int       i;

		if ( only >= 0 && es->number != only ) {
			continue;
		}

		VectorCopy( cent->lerpOrigin, org );

		/* modelindex means different things per entity type, and getting this wrong
		   fails silently — every lookup returns handle 0, every entity falls back to
		   the marker cube, and the tool looks like it is working while showing
		   uniform boxes that have nothing to do with the models. For ET_ITEM the
		   index is into bg_itemlist and the model lives in cg_items[]; elsewhere it
		   indexes the configstring models in cgs.gameModels[]. */
		hModel = 0;
		if ( es->modelindex > 0 ) {
			if ( es->eType == ET_ITEM ) {
				if ( es->modelindex < bg_numItems ) {
					hModel = cg_items[ es->modelindex ].models[0];
				}
			} else if ( es->modelindex < MAX_MODELS ) {
				hModel = cgs.gameModels[ es->modelindex ];
			}
		}

		if ( hModel ) {
			trap_R_ModelBounds( hModel, mins, maxs );
		} else {
			VectorSet( mins, -8, -8, -8 );
			VectorSet( maxs,  8,  8,  8 );
		}

		/* A model with degenerate bounds would emit a zero-area box that is
		   invisible — the exact failure this tool exists to rule out. Give it the
		   fallback marker instead so "no box" always means "no entity here". */
		if ( maxs[0] - mins[0] < 1.0f && maxs[1] - mins[1] < 1.0f && maxs[2] - mins[2] < 1.0f ) {
			VectorSet( mins, -8, -8, -8 );
			VectorSet( maxs,  8,  8,  8 );
		}

		for ( i = 0; i < 3; i++ ) {
			mins[i] += org[i];
			maxs[i] += org[i];
		}

		CG_BoxColorForEntity( es->number, rgba, 255 );
		CG_AddBoxQuads( mins, maxs, rgba );

		if ( cg_drawBBox.integer >= 2 ) {
			vec3_t bigMins, bigMaxs;
			for ( i = 0; i < 3; i++ ) {
				bigMins[i] = mins[i] - CG_BBOX_XRAY_INFLATE;
				bigMaxs[i] = maxs[i] + CG_BBOX_XRAY_INFLATE;
			}
			CG_BoxColorForEntity( es->number, rgba, 96 );
			CG_AddBoxQuads( bigMins, bigMaxs, rgba );
		}
	}
}
