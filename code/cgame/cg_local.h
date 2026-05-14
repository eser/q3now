// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef CG_LOCAL_H
#define CG_LOCAL_H
//
#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_types.h"
#include "../game/bg_public.h"
#include "cg_public.h"

// Quake3e compat: refEntity_t renamed shaderRGBA[4] to shader (color4ub_t with .rgba[4])
#ifndef shaderRGBA
#define shaderRGBA shader.rgba
#endif


// The entire cgame module is unloaded and reloaded on each level change,
// so there is NO persistant data between levels on the client side.
// If you absolutely need something stored, it can either be kept
// by the server in the server stored userinfos, or stashed in a cvar.

#define	POWERUP_BLINKS		5
#define RAIL_SPIRAL_TRAILTIME	1200
#define RAIL_CORE_TRAILTIME		800

// ── rail trail system ───────────────────────────────────────────────
//
// Poly-strip helix + batched debris + HDR dynamic lighting.
// Helix submitted as quad-per-segment batch via AddPolyToScene.
// No localEntities used — zero entity pool impact.
//
//  CG_RailTrail()        — spawns a trail (fills railTrail_t slot)
//  CG_AddRailTrails()    — per-frame: fade, animate, submit polys
//  CG_ClearRailTrails()  — reset all trails (map change)
//
#define MAX_RAIL_TRAILS     8
#define MAX_RAIL_SEGMENTS   2048
#define MAX_RAIL_DEBRIS     128
#define MAX_RAIL_SPARKS     20
#define RAIL_TRAILTIME      1500
#define RAIL_HELIX_RADIUS   3.0f
#define RAIL_HELIX_SPACING  3.0f
#define RAIL_RIBBON_WIDTH   1.5f
#define RAIL_HELIX_ROTATION 2       // ring steps per segment (20° per step)

typedef struct {
	// ── shared with future Vulkan layer — do not reorder ──
	vec3_t      start;
	vec3_t      end;
	int         startTime;
	byte        color[4];
	int         numSegments;

	// ── helix axis data (rebuilt each frame with evolving radius/spacing) ──
	vec3_t      beamAxis;                       // normalized beam direction
	vec3_t      perpAxis[36];                   // precomputed ring positions
	float       beamLen;                        // total beam length

	polyVert_t  debris[MAX_RAIL_DEBRIS * 4];    // billboard quads
	vec3_t      debrisOrg[MAX_RAIL_DEBRIS];     // spawn positions (for gravity drift)
	vec3_t      debrisDelta[MAX_RAIL_DEBRIS];   // random drift velocity
	int         numDebris;

	polyVert_t  sparks[MAX_RAIL_SPARKS * 4];    // impact spark quads
	vec3_t      sparkOrg[MAX_RAIL_SPARKS];      // spawn positions
	vec3_t      sparkVel[MAX_RAIL_SPARKS];      // velocity (surface normal based)
	int         numSparks;

	vec3_t      impactPoint;
	vec3_t      impactNormal;

	qboolean    active;
} railTrail_t;
#define BRASS_TIME          2500

#define	POWERUP_BLINK_TIME	1000
#define	FADE_TIME			200
#define	PULSE_TIME			200
#define	DAMAGE_DEFLECT_TIME	100
#define	DAMAGE_RETURN_TIME	400
#define DAMAGE_TIME			500
#define	LAND_DEFLECT_TIME	150
#define	LAND_RETURN_TIME	300
#define	STEP_TIME			200
#define	DUCK_TIME			100
#define	PAIN_TWITCH_TIME	200
#define	WEAPON_SELECT_TIME	1400
#define	ITEM_SCALEUP_TIME	1000
#define	ZOOM_TIME			150
#define	ITEM_BLOB_TIME		200
#define	MUZZLE_FLASH_TIME	20
#define	GHOST_FLASH_TIME	300
#define	SINK_TIME			1000		// time for fragments to sink into ground before going away
#define	REWARD_TIME			3000

#define	PULSE_SCALE			1.5			// amount to scale up the icons when activating

#define	MAX_STEP_CHANGE		32

#define	MAX_VERTS_ON_POLY	10
#define	MAX_MARK_POLYS		256

#define STAT_MINUS			10	// num frame for '-' stats digit

#define	ICON_SIZE			48
#undef	CHAR_WIDTH				// ignore define from libc
#define	CHAR_WIDTH			32
#define	CHAR_HEIGHT			48
#define	TEXT_ICON_SPACE		4

#define	TEAMCHAT_WIDTH		80
#define TEAMCHAT_HEIGHT		8

// very large characters
#define	GIANT_WIDTH			32
#define	GIANT_HEIGHT		48

#define TEAM_OVERLAY_MAXNAME_WIDTH	12
#define TEAM_OVERLAY_MAXLOCATION_WIDTH	16


typedef enum {
	IMPACTSOUND_DEFAULT,
	IMPACTSOUND_METAL,
	IMPACTSOUND_FLESH
} impactSound_t;

//=================================================

// player entities need to track more information
// than any other type of entity.

// note that not every player entity is a client entity,
// because corpses after respawn are outside the normal
// client numbering range

// when changing animation, set animationTime to frameTime + lerping time
// The current lerp will finish out, then it will lerp to the new animation
typedef struct {
	int			oldFrame;
	int			oldFrameTime;		// time when ->oldFrame was exactly on

	int			frame;
	int			frameTime;			// time when ->frame will be exactly on

	float		backlerp;

	float		yawAngle;
	qboolean	yawing;
	float		pitchAngle;
	qboolean	pitching;

	int			animationNumber;	// may include ANIM_TOGGLEBIT
	animation_t	*animation;
	int			animationTime;		// time when the first frame of the animation will be exact
#if FEAT_SPEED_SCALING
	int			clock;				// virtual clock advances at frametime * speedScale
#endif
} lerpFrame_t;


typedef struct {
	lerpFrame_t		legs, torso, flag;
	int				painTime;
	int				painDirection;	// flip from 0 to 1
	int				lightningFiring;

	int				railFireTime;
    // railgun trail spawning
    vec3_t			railgunImpact;
    qboolean		railgunFlash;

	// machinegun spinning
	float			barrelAngle;
	int				barrelTime;
	qboolean		barrelSpinning;
	int				barrelWeapon;	// weapon whose spin state is stored above; WP_NONE = uninitialised

    int				grappleFiring;
} playerEntity_t;

//=================================================

#if FEAT_EARTHQUAKE_SYSTEM
typedef struct {
	vec3_t	origin;
	float	radius;		// negative = global (no distance attenuation)
	float	amplitude;
	int		startTime;
	int		endTime;
	int		fadeInTime;
	int		fadeOutTime;
} earthquake_t;
#define MAX_EARTHQUAKES 64
#endif

// centity_t have a direct corespondence with gentity_t in the game, but
// only the entityState_t is directly communicated to the cgame
typedef struct centity_s {
	entityState_t	currentState;	// from cg.frame
	entityState_t	nextState;		// from cg.nextFrame, if available
	qboolean		interpolate;	// true if next is valid to interpolate to
	qboolean		currentValid;	// true if cg.frame holds this entity

	int				muzzleFlashTime;	// move to playerEntity?
	int				previousEvent;
	int				teleportFlag;

	int				trailTime;		// so missile trails can handle dropped initial packets
	int				dustTrailTime;
	int				miscTime;

	int				snapShotTime;	// last time this entity was found in a snapshot

	playerEntity_t	pe;

	int				errorTime;		// decay the error from this time
	vec3_t			errorOrigin;
	vec3_t			errorAngles;

	qboolean		extrapolated;	// false if origin / angles is an interpolation
	vec3_t			rawOrigin;
	vec3_t			rawAngles;

	vec3_t			beamEnd;

	// exact interpolated position of entity on this frame
	vec3_t			lerpOrigin;
	vec3_t			lerpAngles;
} centity_t;


//======================================================================

// local entities are created as a result of events or predicted actions,
// and live independently from all server transmitted entities

typedef struct markPoly_s {
	struct markPoly_s	*prevMark, *nextMark;
	int			time;
	qhandle_t	markShader;
	qboolean	alphaFade;		// fade alpha instead of rgb
	float		color[4];
	poly_t		poly;
	polyVert_t	verts[MAX_VERTS_ON_POLY];
} markPoly_t;


typedef enum {
	LE_MARK,
	LE_EXPLOSION,
	LE_SPRITE_EXPLOSION,
	LE_FRAGMENT,
	LE_MOVE_SCALE_FADE,
	LE_FALL_SCALE_FADE,
	LE_FADE_RGB,
	LE_SCALE_FADE,
	LE_SCOREPLUM,
#if FEAT_DAMAGE_PLUMS
	LE_DAMAGEPLUM,			// floating damage number (2A)
#endif
#if FEAT_PING_LOCATION
	LE_PING_LOCATION,		// team ping marker (4G)
#endif
	LE_KAMIKAZE,
	LE_DEFLECTOR_IMPACT,
	LE_DEFLECTOR_JUICED,
	LE_SHOWREFENTITY
} leType_t;

typedef enum {
	LEF_PUFF_DONT_SCALE  = 0x0001,			// do not scale size over time
	LEF_TUMBLE			 = 0x0002,			// tumble over time, used for ejecting shells
	LEF_SOUND1			 = 0x0004,			// sound 1 for kamikaze
	LEF_SOUND2			 = 0x0008			// sound 2 for kamikaze
} leFlag_t;

typedef enum {
	LEMT_NONE,
	LEMT_BURN,
	LEMT_BLOOD
} leMarkType_t;			// fragment local entities can leave marks on walls

typedef enum {
	LEBS_NONE,
	LEBS_BLOOD,
	LEBS_BRASS
} leBounceSoundType_t;	// fragment local entities can make sounds on impacts

typedef struct localEntity_s {
	struct localEntity_s	*prev, *next;
	leType_t		leType;
	int				leFlags;

	int				startTime;
	int				endTime;
	int				fadeInTime;

	float			lifeRate;			// 1.0 / (endTime - startTime)

	trajectory_t	pos;
	trajectory_t	angles;

	float			bounceFactor;		// 0.0 = no bounce, 1.0 = perfect

	float			color[4];

	float			radius;

	float			light;
	vec3_t			lightColor;

	leMarkType_t		leMarkType;		// mark to leave on fragment impact
	leBounceSoundType_t	leBounceSoundType;

	refEntity_t		refEntity;
} localEntity_t;

//======================================================================


typedef struct {
	int				client;
	int				score;
	int				ping;
	int				time;
	int				scoreFlags;
	int				powerUps;
	int				accuracy;
	int				impressiveCount;
	int				excellentCount;
	int				guantletCount;
	int				defendCount;
	int				assistCount;
	int				captures;
	qboolean	perfect;
	int				team;
	int				deaths;
	int				killingSpreeCount;
	int				rampageCount;
	int				massacreCount;
	int				unstoppableCount;
} score_t;

// per-attack stats (received via bstats server command)
typedef struct {
	int hits;
	int shots;
	int kills;
	int deaths;
	int damage;
} cgAttackStat_t;

// each client has an associated clientInfo_t
// that contains media references necessary to present the
// client model and other color coded effects
// this is regenerated each time a client's configstring changes,
// usually as a result of a userinfo (name, model, etc) change
#define	MAX_CUSTOM_SOUNDS	32

typedef struct {
	qboolean		infoValid;

	char			name[MAX_QPATH];
	team_t			team;

	int				botSkill;		// 0 = not bot, 1-5 = bot

	vec3_t			color1;
	vec3_t			color2;

	byte c1RGBA[4];
	byte c2RGBA[4];

	int				score;			// updated by score servercmds
	int				location;		// location index for team mode
	int				health;			// you only get this info about your teammates
	int				armor;
	int				armorClass;
	int				curWeapon;

	int				wins, losses;	// in duel mode

	int				teamTask;		// task in teamplay (offence/defence)
	qboolean		teamLeader;		// true when this is a team leader

	int				powerups;		// so can display quad/flag status

	int				medkitUsageTime;
	int				deflectorStartTime;
	int				deflectorStopTime;

	int				breathPuffTime;
	int				bubblePuffTime;

	// when clientinfo is changed, the loading of models/skins/sounds
	// can be deferred until you are dead, to prevent hitches in
	// gameplay
	char			characterName[MAX_QPATH];
	char			skinName[MAX_QPATH];
	char			redTeam[MAX_TEAMNAME];
	char			blueTeam[MAX_TEAMNAME];
	qboolean		deferred;

	// FNV-1a hash of (characterName + skinName + team) — used as a fast-reject
	// signature for CG_ScanForExistingClientInfo / CG_SetDeferredClientInfo.
	// Recomputed in CG_NewClientInfo whenever the underlying fields change.
	unsigned int	infoHash;

	qboolean		newAnims;		// true if using the new mission pack animations
	qboolean		fixedlegs;		// true if legs yaw is always the same as torso yaw
	qboolean		fixedtorso;		// true if torso never changes yaw

	vec3_t			headOffset;		// move head in icon views
	gender_t		gender;			// from model

	qhandle_t		legsModel;
	qhandle_t		legsSkin;

	qhandle_t		torsoModel;
	qhandle_t		torsoSkin;

	qhandle_t		headModel;
	qhandle_t		headSkin;

	qhandle_t		skinHandle;    // character skin registry handle (0 = use legacy customSkin)

	qhandle_t		modelIcon;

	animation_t		animations[MAX_TOTALANIMATIONS];

	sfxHandle_t		sounds[MAX_CUSTOM_SOUNDS];

	// Per-entity effects and footstep sounds (populated by cg_characters.c).
	// Replaces global cgs.media.footsteps / cgs.media.gibSound / landSound / watr*.
	struct {
		sfxHandle_t gibSound;
		sfxHandle_t landSound;
		sfxHandle_t watrInSound;
		sfxHandle_t watrOutSound;
		sfxHandle_t watrUnSound;
	} effects;
	sfxHandle_t		footstepSounds[FOOTSTEP_TOTAL][4];

#if FEAT_IQM
	qboolean		iqmModel;		// true if using body.iqm single-mesh
	qhandle_t		bodyModel;		// body.iqm model handle
	qhandle_t		bodySkin;		// body.iqm skin handle
	qhandle_t		bodyShader;     // set when skin is single-path form (IQM path)
#endif // FEAT_IQM
} clientInfo_t;

// Per-bot directive display state, populated from CS_BOTDIRECTIVES configstrings.
// Mirrors the server-side directiveType_t enum (g_wiredbots.h) as raw ints.
typedef struct {
	int		type;				// directive type; 0 = none
	char	targetName[64];		// display name ("Heavy Armor", "Keel", etc.)
	int		updateTime;			// cg.time when last changed (for fade effects)
} botDirectiveDisplay_t;

// each WP_* weapon enum has an associated weaponInfo_t
// that contains media references necessary to present the
// weapon and its effects
typedef struct weaponInfo_s {
	qboolean		registered;
	gitem_t			*item;

	qhandle_t		handsModel;			// the hands don't actually draw, they just position the weapon
	qhandle_t		weaponModel;
	qhandle_t		barrelModel;
	qhandle_t		flashModel;

	vec3_t			weaponMidpoint;		// so it will rotate centered instead of by tag

	float			flashDlight;
	vec3_t			flashDlightColor;
	sfxHandle_t		flashSound[4];		// fast firing weapons randomly choose

	qhandle_t		weaponIcon;
	qhandle_t		ammoIcon;

	qhandle_t		ammoModel;

	qhandle_t		missileModel;
	sfxHandle_t		missileSound;
	void			(*missileTrailFunc)( centity_t *, const struct weaponInfo_s *wi );
	float			missileDlight;
	vec3_t			missileDlightColor;
	int				missileRenderfx;

	void			(*ejectBrassFunc)( centity_t * );

	float			trailRadius;
	float			wiTrailTime;

	sfxHandle_t		readySound;
	sfxHandle_t		firingPriSound;
	sfxHandle_t		firingSecSound;
} weaponInfo_t;


// each IT_* item has an associated itemInfo_t
// that constains media references necessary to present the
// item and its effects
typedef struct {
	qboolean		registered;
	qhandle_t		models[MAX_ITEM_MODELS];
	qhandle_t		icon;
} itemInfo_t;


typedef struct {
	int				itemNum;
} powerupInfo_t;


#define MAX_SKULLTRAIL		10

typedef struct {
	vec3_t positions[MAX_SKULLTRAIL];
	int numpositions;
} skulltrail_t;


#define MAX_REWARDSTACK		10
#define MAX_SOUNDBUFFER		20

//======================================================================

// all cg.stepTime, cg.duckTime, cg.landTime, etc are set to cg.time when the action
// occurs, and they will have visible effects for #define STEP_TIME or whatever msec after

#define MAX_PREDICTED_EVENTS	16

typedef struct {
	int			clientFrame;		// incremented each frame

	int			clientNum;

	qboolean	demoPlayback;
	qboolean	levelShot;			// taking a level menu screenshot
	int			deferredPlayerLoading;
	qboolean	loading;			// don't defer players at initial startup
	qboolean	intermissionStarted;	// don't play voice rewards, because game will end shortly

	// there are only one or two snapshot_t that are relevant at a time
	int			latestSnapshotNum;	// the number of snapshots the client system has received
	int			latestSnapshotTime;	// the time from latestSnapshotNum, so we don't need to read the snapshot yet

	snapshot_t	*snap;				// cg.snap->serverTime <= cg.time
	snapshot_t	*nextSnap;			// cg.nextSnap->serverTime > cg.time, or NULL
	snapshot_t	activeSnapshots[2];

	float		frameInterpolation;	// (float)( cg.time - cg.frame->serverTime ) / (cg.nextFrame->serverTime - cg.frame->serverTime)

	qboolean	thisFrameTeleport;
	qboolean	nextFrameTeleport;

	int			frametime;		// cg.time - cg.oldTime

	int			time;			// this is the time value that the client
								// is rendering at.
	int			oldTime;		// time at last frame, used for missile trails and prediction checking

	int			physicsTime;	// either cg.snap->time or cg.nextSnap->time

	int			timelimitWarnings;	// 5 min, 1 min, overtime
	int			scorelimitWarnings;

	qboolean	mapRestart;			// set on a map restart to set back the weapon

	qboolean	renderingThirdPerson;		// during deaths, chasecams, etc

	// prediction state
	qboolean	hyperspace;				// true if prediction has hit a trigger_teleport
	playerState_t	predictedPlayerState;
	centity_t		predictedPlayerEntity;
	qboolean	validPPS;				// clear until the first call to CG_PredictPlayerState
	int			predictedErrorTime;
	vec3_t		predictedError;

	int			eventSequence;
	int			predictableEvents[MAX_PREDICTED_EVENTS];

	float		stepChange;				// for stair up smoothing
	int			stepTime;

	float		stepDownChange;			// for stair descent smoothing (negative)
	int			stepDownTime;

	float		duckChange;				// for duck viewheight smoothing
	int			duckTime;

	float		landChange;				// for landing hard
	int			landTime;

	// input state sent to server
	int			weaponSelect;
	int			lastGrabbedWeapon;		// last weapon picked up (for weapongrabbed cmd)

	// auto rotating items
	vec3_t		autoAngles;
	vec3_t		autoAxis[3];
	vec3_t		autoAnglesFast;
	vec3_t		autoAxisFast[3];

	// view rendering
	refdef_t	refdef;
	vec3_t		refdefViewAngles;		// will be converted to refdef.viewaxis

	// zoom key
	qboolean	zoomed;
	float		zoomSensitivity;

	// FOV transition (unified for zoom, third-person, and normal)
	float		fovCurrent;			// current smoothly-interpolated FOV
	float		fovTarget;			// cached target to detect changes
	float		fovTransitionFrom;	// FOV value when transition started
	int			fovTransitionTime;	// timestamp when transition started

	// scoreboard
	int			scoresRequestTime;
	int			numScores;
	int			selectedScore;
	int			teamScores[2];
	score_t		scores[MAX_CLIENTS];
	qboolean	showScores;
	qboolean	scoreBoardShowing;
	int			scoreFadeTime;
	char		killerName[MAX_NAME_LENGTH];
#if FEAT_FOLLOW_KILLER
	int			killerClientNum;	// client who killed us (-1 if none/world)
	qboolean	followKillerPending;	// waiting to auto-follow killer
#endif
#if FEAT_AUTO_DEMO
	qboolean	playerRecord;		// auto-recording active
	qboolean	autoJoinChecked;	// have we done the auto-join check yet?
#endif
	char			spectatorList[MAX_STRING_CHARS];		// list of names
	int				spectatorLen;												// length of list
	float			spectatorWidth;											// width in device units
	int				spectatorTime;											// next time to offset
	int				spectatorPaintX;										// current paint x
	int				spectatorPaintX2;										// current paint x
	int				spectatorOffset;										// current offset from start
	int				spectatorPaintLen; 									// current offset from start

#if FEAT_HARVESTER
	// skull trails
	skulltrail_t	skulltrails[MAX_CLIENTS];
#endif

	// centerprinting
	int			centerPrintTime;
	int			centerPrintCharWidth;
	int			centerPrintY;
	char		centerPrint[1024];
	int			centerPrintLines;

	// low ammo warning state
	int			lowAmmoWarning;		// 1 = low, 2 = empty

	// crosshair client ID
	int			crosshairClientNum;
	int			crosshairClientTime;

	// powerup active flashing
	int			powerupActive;
	int			powerupTime;

	// attacking player
	int			attackerTime;
	int			voiceTime;

	// reward medals
	int			rewardStack;
	int			rewardTime;
	int			rewardCount[MAX_REWARDSTACK];
	qhandle_t	rewardShader[MAX_REWARDSTACK];
	qhandle_t	rewardSound[MAX_REWARDSTACK];

	// sound buffer mainly for announcer sounds
	int			soundBufferIn;
	int			soundBufferOut;
	int			soundTime;
	qhandle_t	soundBuffer[MAX_SOUNDBUFFER];

	// for voice chat buffer
	int			voiceChatTime;
	int			voiceChatBufferIn;
	int			voiceChatBufferOut;

	// warmup countdown
	int			warmup;
	int			warmupCount;

	//==========================

	int			itemPickup;
	int			itemPickupTime;
	int			itemPickupBlendTime;	// the pulse around the crosshair is timed separately

	int			weaponSelectTime;
	int			weaponAnimation;
	int			weaponAnimationTime;

	// blend blobs
	float		damageTime;
	float		damageX, damageY, damageValue;

	int			doubleBlastKickTime;	// time of last double-blast kick

	int			lastArcTarget;		// entity number of last chain arc target (-1 = none)
	int			lastArcTime;		// time of last arc connection

	// status bar head
	float		headYaw;
	float		headEndPitch;
	float		headEndYaw;
	int			headEndTime;
	float		headStartPitch;
	float		headStartYaw;
	int			headStartTime;

	vec3_t		kick_angles;	// weapon kicks
	vec3_t		kick_origin;

	// view movement
	float		v_dmg_time;
	float		v_dmg_pitch;
	float		v_dmg_roll;

	// temp working variables for player view
	float		bobfracsin;
	int			bobcycle;
	float		xyspeed;
	int     nextOrbitTime;

	//qboolean cameraMode;		// if rendering from a loaded camera


	// development tool
	refEntity_t		testModelEntity;
	char			testModelName[MAX_QPATH];
	qboolean		testGun;

#if FEAT_THIRD_PERSON
	qboolean	thirdPersonHeld;			// +thirdperson key held (XOR inverts cg_thirdPerson cvar)
	int			thirdPersonTransitionTime;	// timestamp when 1st<->3rd toggle happened
	qboolean	thirdPersonTransitionFrom;	// was rendering 3rd person before toggle?
	float		thirdPersonCurrentRange;	// current camera distance (lerps toward trace-clipped range)
#endif

#if FEAT_ZNUDGE
	vec3_t		smoothVelocities[MAX_CLIENTS];	// smoothed velocity per client
	float		smoothPing;						// smoothed ping for nudge calculation
#endif

#if FEAT_SHOTGUN_PUMP
	int			sgPumpTime;		// timestamp when pump animation started
#endif

#if FEAT_EARTHQUAKE_SYSTEM
	earthquake_t	earthquakes[MAX_EARTHQUAKES];
	float			additionalTremble;
#endif

#if FEAT_SCREENSHOT_TOOLS
	int			stopTime;		// server time when freeze was triggered, 0 if running
	int			timeOffset;		// serverTime - cg.time at freeze moment
	int			serverOffset;	// serverTime - snap->serverTime at freeze moment
#endif

#if FEAT_GAME_MEETING
	qboolean	meeting;		// pre-match lobby active
#endif

} cg_t;


// all of the model, shader, and sound references that are
// loaded at gamestate time are stored in cgMedia_t
// Other media that can be tied to clients, weapons, or items are
// stored in the clientInfo_t, itemInfo_t, weaponInfo_t, and powerupInfo_t
typedef struct {
	qhandle_t	whiteShader;

#if FEAT_HARVESTER
	qhandle_t	redCubeModel;
	qhandle_t	blueCubeModel;
	qhandle_t	redCubeIcon;
	qhandle_t	blueCubeIcon;
#endif
	qhandle_t	redFlagModel;
	qhandle_t	blueFlagModel;
	qhandle_t	neutralFlagModel;
	qhandle_t	redFlagShader[3];
	qhandle_t	blueFlagShader[3];
	qhandle_t	flagShader[4];

	qhandle_t	flagPoleModel;
	qhandle_t	flagFlapModel;

	qhandle_t	redFlagFlapSkin;
	qhandle_t	blueFlagFlapSkin;
	qhandle_t	neutralFlagFlapSkin;

	qhandle_t	redFlagBaseModel;
	qhandle_t	blueFlagBaseModel;
	qhandle_t	neutralFlagBaseModel;

#if FEAT_OVERLOAD
	qhandle_t	overloadBaseModel;
	qhandle_t	overloadTargetModel;
	qhandle_t	overloadLightsModel;
	qhandle_t	overloadEnergyModel;
#endif

#if FEAT_HARVESTER
	qhandle_t	harvesterModel;
	qhandle_t	harvesterRedSkin;
	qhandle_t	harvesterBlueSkin;
	qhandle_t	harvesterNeutralModel;
#endif

	qhandle_t	healthModel;
	qhandle_t	healthIcon;

    qhandle_t	heavyArmorModel;
    qhandle_t	heavyArmorIcon;

    qhandle_t	combatArmorModel;
    qhandle_t	combatArmorIcon;

    qhandle_t	jacketArmorModel;
    qhandle_t	jacketArmorIcon;

	qhandle_t	teamStatusBar;

	qhandle_t	deferShader;

	// gib explosions
	qhandle_t	gibAbdomen;
	qhandle_t	gibArm;
	qhandle_t	gibChest;
	qhandle_t	gibFist;
	qhandle_t	gibFoot;
	qhandle_t	gibForearm;
	qhandle_t	gibIntestine;
	qhandle_t	gibLeg;
	qhandle_t	gibSkull;
	qhandle_t	gibBrain;

	qhandle_t	smoke2;

	qhandle_t	machinegunBrassModel;
	qhandle_t	shotgunBrassModel;

	qhandle_t	railRingsShader;
	qhandle_t	railCoreShader;

	// Particle class handles for rail debris and impact sparks.
	// Registered via CG_RegisterRailParticleClasses() once the rail
	// shaders above are bound (handles cgs.media.railRingsShader and
	// cgs.media.whiteShader feed into the class definitions). Stored
	// as qhandle_t (= int) to match the surrounding rail-handle
	// convention; particleClassHandle_t is the same underlying type.
	qhandle_t	railDebrisClass;
	qhandle_t	railSparksClass;

	qhandle_t	lightningShaderPrim;
	qhandle_t	lightningArcShaderPrim;

	// Phase 5T: PTRAIL_PUSH visual assets — beam shader +
	// sparkle-stream particle class. Registered via
	// CG_RegisterGraphics + CG_RegisterPushParticleClasses; the
	// generic CG_RegisterPlayerTrailDefs binds these into the
	// PTRAIL_PUSH def table entry, and CG_AddPlayerTrails consumes
	// them per-frame via the transient re-submit pattern. Jumppads
	// are the current EV trigger; haste/speed extends the trail
	// per-frame from CG_UpdatePlayerTrailExtensions.
	qhandle_t	pushTrailShader;
	qhandle_t	pushStreamClass;

	// Particle class handle for Lightning Gun primary impact sparks.
	// Registered via CG_RegisterLightningParticleClasses() once
	// cgs.media.lightningSparkShader is bound; consumed by
	// CG_LightningSparks's GPU branch (cg_cpuEffects == 0).
	qhandle_t	lgSparksClass;

	qhandle_t	friendShader;
	qhandle_t	friendFlagShaderNeutral;
	qhandle_t	friendFlagShaderBlue;
	qhandle_t	friendFlagShaderRed;
	qhandle_t	friendColorShaders[6];

	qhandle_t	balloonShader;
	qhandle_t	connectionShader;

	qhandle_t	selectShader;
	qhandle_t	viewBloodShader;
	qhandle_t	tracerShader;
	qhandle_t	lagometerShader;
	qhandle_t	backTileShader;
	qhandle_t	noammoShader;

	qhandle_t	crosshairMeleeShader;
	qhandle_t	crosshairBulletShader;
	qhandle_t	crosshairBurstShader;
	qhandle_t	crosshairMissileShader;
	qhandle_t	crosshairDefaultShader;

	qhandle_t	smokePuffShader;
	qhandle_t	smokePuffRageProShader;
	qhandle_t	shotgunSmokePuffShader;
	qhandle_t	plasmaBallShader;
	qhandle_t	waterBubbleShader;
	sfxHandle_t	waterSplashSound;	// Phase 6.5.3: water-surface crossing impact (Q1 misc/h2ohit1, Q3 watr_in fallback; 0 if neither present)
	qhandle_t	bloodTrailShader;

	// qhandle_t	nailPuffShader;

	qhandle_t	sparkShader;
	qhandle_t	lightningSparkShader;

	qhandle_t	shadowMarkShader;

	qhandle_t	botSkillShaders[5];

	// wall mark shaders
	qhandle_t	wakeMarkShader;
	qhandle_t	bloodMarkShader;
	qhandle_t	bulletMarkShader;
	qhandle_t	burnMarkShader;
	qhandle_t	holeMarkShader;
	qhandle_t	energyMarkShader;

	// powerup shaders
	qhandle_t	quadShader;
	qhandle_t	redQuadShader;
	qhandle_t	quadWeaponShader;
	qhandle_t	berserkShader;
	qhandle_t	berserkWeaponShader;
	qhandle_t	invisShader;
	qhandle_t	regenShader;
	qhandle_t	battleSuitShader;
	qhandle_t	battleWeaponShader;
	qhandle_t	spawnProtectShader;
	qhandle_t	hastePuffShader;
	qhandle_t	redKamikazeShader;
	qhandle_t	blueKamikazeShader;

	// weapon effect models
	qhandle_t	bulletFlashModel;
	qhandle_t	ringFlashModel;
	qhandle_t	dishFlashModel;
	qhandle_t	lightningExplosionModel;

	// weapon effect shaders
	qhandle_t	railExplosionShader;
	qhandle_t	plasmaExplosionShader;
	qhandle_t	bulletExplosionShader;
	qhandle_t	rocketExplosionShader;
	qhandle_t	grenadeExplosionShader;
	qhandle_t	bloodExplosionShader;

	// special effects models
	qhandle_t	teleportEffectModel;
	qhandle_t	teleportEffectShader;
	qhandle_t	kamikazeEffectModel;
	qhandle_t	kamikazeShockWave;
	qhandle_t	kamikazeHeadModel;
	qhandle_t	kamikazeHeadTrail;
	qhandle_t	deflectorImpactModel;
	qhandle_t	deflectorJuicedModel;
	qhandle_t	deflectorPowerupModel;
	qhandle_t	medkitUsageModel;
	qhandle_t	dustPuffShader;
	qhandle_t	heartShader;

	// scoreboard headers
	qhandle_t	scoreboardName;
	qhandle_t	scoreboardPing;
	qhandle_t	scoreboardScore;
	qhandle_t	scoreboardTime;

	// medals shown during gameplay
	qhandle_t	medalImpressive;
	qhandle_t	medalExcellent;
	qhandle_t	medalGauntlet;
	qhandle_t	medalDefend;
	qhandle_t	medalAssist;
	qhandle_t	medalCapture;
	qhandle_t	medalKillingSpree;
	qhandle_t	medalRampage;
	qhandle_t	medalMassacre;
	qhandle_t	medalUnstoppable;

	// sounds
	sfxHandle_t	quadSound;
	sfxHandle_t	berserkSound;
	sfxHandle_t	tracerSound;
	sfxHandle_t	selectSound;
	sfxHandle_t	useNothingSound;
	sfxHandle_t	wearOffSound;
	sfxHandle_t	sfx_lghit1;
	sfxHandle_t	sfx_lghit2;
	sfxHandle_t	sfx_lghit3;
	sfxHandle_t	sfx_lightningArcLoop;
	sfxHandle_t	sfx_ric1;
	sfxHandle_t	sfx_ric2;
	sfxHandle_t	sfx_ric3;
	//sfxHandle_t	sfx_railg;
	sfxHandle_t	sfx_rockexp;
	sfxHandle_t	sfx_plasmaexp;
	sfxHandle_t kamikazeExplodeSound;
	sfxHandle_t kamikazeImplodeSound;
	sfxHandle_t kamikazeFarSound;
	sfxHandle_t	sfx_nghit;
	sfxHandle_t	sfx_nghitflesh;
	sfxHandle_t	sfx_nghitmetal;
	sfxHandle_t	sfx_chghit;
	sfxHandle_t	sfx_chghitflesh;
	sfxHandle_t	sfx_chghitmetal;
	sfxHandle_t useDeflectorSound;
	sfxHandle_t deflectorImpactSound1;
	sfxHandle_t deflectorImpactSound2;
	sfxHandle_t deflectorImpactSound3;
	sfxHandle_t deflectorJuicedSound;
#if FEAT_OVERLOAD
	sfxHandle_t obeliskHitSound1;
	sfxHandle_t obeliskHitSound2;
	sfxHandle_t obeliskHitSound3;
	sfxHandle_t	obeliskRespawnSound;
#endif
	sfxHandle_t	winnerSound;
	sfxHandle_t	loserSound;
	sfxHandle_t	gibBounce1Sound;
	sfxHandle_t	gibBounce2Sound;
	sfxHandle_t	gibBounce3Sound;
	sfxHandle_t	talkSound;
	sfxHandle_t	teleInSound;
	sfxHandle_t	teleOutSound;
	sfxHandle_t	noAmmoSound;
	sfxHandle_t	respawnSound;
	sfxHandle_t fallSound;
	sfxHandle_t gurpSound[2];
	sfxHandle_t jumpPadSound;
#if FEAT_EARTHQUAKE_SYSTEM
	sfxHandle_t earthquakeSound;
#endif

	sfxHandle_t oneMinuteSound;
	sfxHandle_t fiveMinuteSound;
	sfxHandle_t suddenDeathSound;

	sfxHandle_t threeFragSound;
	sfxHandle_t twoFragSound;
	sfxHandle_t oneFragSound;

	sfxHandle_t hitSoundNoArmor;
	sfxHandle_t hitSoundHeavyArmor;
	sfxHandle_t hitSoundCombatArmor;
	sfxHandle_t hitSoundJacketArmor;
	sfxHandle_t hitSoundFriendlyFire;
	sfxHandle_t impressiveSound;
	sfxHandle_t excellentSound;
	sfxHandle_t deniedSound;
	sfxHandle_t humiliationSound;
	sfxHandle_t assistSound;
	sfxHandle_t defendSound;
	sfxHandle_t killingSpreeSound;
	sfxHandle_t rampageSound;
	sfxHandle_t massacreSound;
	sfxHandle_t unstoppableSound;

	sfxHandle_t takenLeadSound;
	sfxHandle_t tiedLeadSound;
	sfxHandle_t lostLeadSound;

	sfxHandle_t voteNow;
	sfxHandle_t votePassed;
	sfxHandle_t voteFailed;

	sfxHandle_t flightSound;
	sfxHandle_t medkitSound;

	sfxHandle_t weaponHoverSound;

	// teamplay sounds
	sfxHandle_t captureAwardSound;
	sfxHandle_t redScoredSound;
	sfxHandle_t blueScoredSound;
	sfxHandle_t redLeadsSound;
	sfxHandle_t blueLeadsSound;
	sfxHandle_t teamsTiedSound;

	sfxHandle_t	captureYourTeamSound;
	sfxHandle_t	captureOpponentSound;
	sfxHandle_t	returnYourTeamSound;
	sfxHandle_t	returnOpponentSound;
	sfxHandle_t	takenYourTeamSound;
	sfxHandle_t	takenOpponentSound;

	sfxHandle_t redFlagReturnedSound;
	sfxHandle_t blueFlagReturnedSound;
	sfxHandle_t neutralFlagReturnedSound;
	sfxHandle_t	enemyTookYourFlagSound;
	sfxHandle_t yourTeamTookEnemyFlagSound;
	sfxHandle_t	youHaveFlagSound;
	sfxHandle_t	enemyTookTheFlagSound;
	sfxHandle_t yourTeamTookTheFlagSound;
#if FEAT_OVERLOAD
	sfxHandle_t yourBaseIsUnderAttackSound;
#endif
	sfxHandle_t holyShitSound;

	// tournament sounds
	sfxHandle_t	count3Sound;
	sfxHandle_t	count2Sound;
	sfxHandle_t	count1Sound;
	sfxHandle_t	countFightSound;
	sfxHandle_t	countPrepareSound;
	sfxHandle_t	countPrepareTeamSound;

#if FEAT_TA_UI
	// new stuff
	qhandle_t patrolShader;
	qhandle_t assaultShader;
	qhandle_t campShader;
	qhandle_t followShader;
	qhandle_t defendShader;
	qhandle_t teamLeaderShader;
	qhandle_t retrieveShader;
	qhandle_t escortShader;
	qhandle_t flagShaders[3];
#endif

#if FEAT_TA_UI
	qhandle_t cursor;
	qhandle_t selectCursor;
	qhandle_t sizeCursor;
#endif

	sfxHandle_t	regenSound;
	sfxHandle_t	protectSound;
	sfxHandle_t	n_healthSound;
	sfxHandle_t	hgrenb1aSound;
	sfxHandle_t	hgrenb2aSound;

    qhandle_t	backpackModel;
    qhandle_t	backpackIcon;
} cgMedia_t;


// The client game static (cgs) structure hold everything
// loaded or calculated from the gamestate.  It will NOT
// be cleared when a tournement restart is done, allowing
// all clients to begin playing instantly
typedef struct {
	gameState_t		gameState;			// gamestate from server
	glconfig_t		glconfig;			// rendering configuration
	float			screenXScale;		// derived from glconfig (legacy, used by proportional text)
	float			screenYScale;
	float			screenXBias;
	float			screenYBias;
	float			screenXScaleStretch;
	float			screenYScaleStretch;

	// normalized-space equivalents (0.0-1.0 screen space)
	float			normXScale;			// aspect-preserving scale factor in norm space
	float			normYScale;
	float			normXBias;			// centering bias in norm space
	float			normYBias;
	float			normXScaleStretch;	// stretch-to-fill scale factor
	float			normYScaleStretch;

	int				serverCommandSequence;	// reliable command stream counter
	int				processedSnapshotNum;// the number of snapshots cgame has requested

	qboolean		localServer;		// detected on startup by checking sv_running

	// parsed from serverinfo
	gametype_t		gametype;
	int             gameflags;
	qboolean		gametypeIsTeamGame;
	int				noFootsteps;
	int             kothGhosts;
	int				scorelimit;
	int				timelimit;
	int				maxclients;
	char			mapname[MAX_QPATH];
	char			redTeam[MAX_QPATH];
	char			blueTeam[MAX_QPATH];
#if FEAT_ATMOSPHERIC
	char			weather[16];		// server-controlled: "rain", "snow", or ""
#endif

	int				voteTime;
	int				voteYes;
	int				voteNo;
	qboolean		voteModified;			// beep whenever changed
	char			voteString[MAX_STRING_TOKENS];

	int				teamVoteTime[2];
	int				teamVoteYes[2];
	int				teamVoteNo[2];
	qboolean		teamVoteModified[2];	// beep whenever changed
	char			teamVoteString[2][MAX_STRING_TOKENS];

	int				levelStartTime;

	int				scores1, scores2;		// from configstrings
	int				redflag, blueflag;		// flag status from configstrings
	int				flagStatus;

	qboolean  newHud;

	//
	// locally derived information from gamestate
	//
	qhandle_t		gameModels[MAX_MODELS];
	sfxHandle_t		gameSounds[MAX_SOUNDS];

	int				numInlineModels;
	qhandle_t		inlineDrawModel[MAX_MODELS];
	vec3_t			inlineModelMidpoints[MAX_MODELS];

	clientInfo_t	clientinfo[MAX_CLIENTS];

	// teamchat width is *3 because of embedded color codes
	char			teamChatMsgs[TEAMCHAT_HEIGHT][TEAMCHAT_WIDTH*3+1];
	int				teamChatMsgTimes[TEAMCHAT_HEIGHT];
	int				teamChatPos;
	int				teamLastChatPos;

	int cursorX;
	int cursorY;
	qboolean eventHandling;
	qboolean mouseCaptured;
	qboolean sizingHud;
	void *capturedItem;
	qhandle_t activeCursor;

	// orders
	int currentOrder;
	qboolean orderPending;
	int orderTime;
	int currentVoiceClient;
	int acceptOrderTime;
	int acceptTask;
	int acceptLeader;
	char acceptVoice[MAX_NAME_LENGTH];

	// media
	cgMedia_t		media;

	// per-attack stats (populated by bstats server command)
	cgAttackStat_t	attackStats[MAX_CLIENTS][ATT_NUM_ATTACKS];

	// Phase 6.5.3: this is a Quake-1 BSP (com_mapBspVersion == 29). Gates the
	// Q1-fidelity water-surface FX (splash sprite/sound, underwater-impact
	// bubble burst). Q3 maps keep their existing visuals. Set in CG_Init.
	qboolean		q1Map;

} cgs_t;

//==============================================================================

extern	cgs_t			cgs;
extern	cg_t			cg;
extern	centity_t		cg_entities[MAX_GENTITIES];
extern	weaponInfo_t	cg_weapons[MAX_WEAPONS];
extern	itemInfo_t		cg_items[MAX_ITEMS];
extern	botDirectiveDisplay_t	cg_botDirectives[MAX_CLIENTS];
extern	markPoly_t		cg_markPolys[MAX_MARK_POLYS];

extern	vmCvar_t		cg_centertime;
extern	vmCvar_t		cg_cpuEffects;
extern	vmCvar_t		cg_runpitch;
extern	vmCvar_t		cg_runroll;
extern	vmCvar_t		cg_bobup;
extern	vmCvar_t		cg_bobpitch;
extern	vmCvar_t		cg_bobroll;
extern	vmCvar_t		cg_swingSpeed;
extern	vmCvar_t		cg_shadows;
extern	vmCvar_t		cg_gibs;
extern	vmCvar_t		cg_drawSnapshot;
extern	vmCvar_t		cg_draw3dIcons;
extern	vmCvar_t		cg_drawIcons;
extern	vmCvar_t		cg_drawAmmoWarning;
extern	vmCvar_t		cg_drawCrosshairNames;
extern	vmCvar_t		cg_drawRewards;
extern	vmCvar_t		cg_teamOverlayUserinfo;
extern	vmCvar_t		cg_crosshairSize;
extern	vmCvar_t		cg_crosshairAlpha;
extern	vmCvar_t		cg_crosshairHealth;
extern	vmCvar_t		cg_crosshairColor;
extern	vmCvar_t		cg_draw2D;
extern	vmCvar_t		cg_animSpeed;
extern	vmCvar_t		cg_debugAnim;
extern	vmCvar_t		cg_debugPosition;
extern	vmCvar_t		cg_debugEvents;
extern	vmCvar_t		cg_debugCharacterSkin;
extern	vmCvar_t		cg_errorDecay;
extern	vmCvar_t		cg_nopredict;
extern	vmCvar_t		cg_noPlayerAnims;
extern	vmCvar_t		cg_showmiss;
extern	vmCvar_t		cg_footsteps;
extern	vmCvar_t		cg_addMarks;
extern	vmCvar_t		cg_gun_frame;
extern	vmCvar_t		cg_gunX;
extern	vmCvar_t		cg_gunY;
extern	vmCvar_t		cg_gunZ;
extern	vmCvar_t		cg_drawGun;
extern	vmCvar_t		cg_tracerChance;
extern	vmCvar_t		cg_tracerWidth;
extern	vmCvar_t		cg_tracerLength;
extern	vmCvar_t		cg_autoswitch;
extern	vmCvar_t		cg_ignore;
extern	vmCvar_t		cg_simpleItems;
extern	vmCvar_t		cg_fov;
extern	vmCvar_t		cg_zoomFov;
extern	vmCvar_t		cg_thirdPersonRange;
extern	vmCvar_t		cg_thirdPersonAngle;
extern	vmCvar_t		cg_thirdPerson;
#if FEAT_THIRD_PERSON
extern	vmCvar_t		cg_thirdPersonFadeStart;
extern	vmCvar_t		cg_thirdPersonFadeEnd;
extern	vmCvar_t		cg_thirdPersonSide;
extern	vmCvar_t		cg_thirdPersonFov;
extern	vmCvar_t		cg_thirdPersonAlpha;
extern	vmCvar_t		cg_fovTransitionTime;
#endif
extern	vmCvar_t		cg_lagometer;
extern	vmCvar_t		cg_synchronousClients;
extern	vmCvar_t		cg_teamChatTime;
extern	vmCvar_t		cg_teamChatHeight;
extern	vmCvar_t		cg_stats;
extern	vmCvar_t 		cg_forceSameCharacter;
extern	vmCvar_t 		cg_buildScript;
extern	vmCvar_t		cg_paused;
extern	vmCvar_t		cg_blood;
extern	vmCvar_t		cg_predictItems;
extern	vmCvar_t		cg_deferPlayers;
extern	vmCvar_t		cg_drawFriend;
extern	vmCvar_t		cg_teamChatsOnly;
extern	vmCvar_t		cg_noVoiceChats;
extern	vmCvar_t		cg_noVoiceText;
extern  vmCvar_t		cg_scorePlums;
extern	vmCvar_t		cg_smoothClients;
extern	vmCvar_t		pmove_fixed;
extern	vmCvar_t		pmove_msec;
#if FEAT_FAST_WEAPON_SWITCH
extern	vmCvar_t		cg_fastWeaponSwitch;
#endif
#if FEAT_ATMOSPHERIC
extern	vmCvar_t		cg_atmosphericEffects;
void	CG_AtmosphericInit( void );
void	CG_AddAtmosphericEffects( void );
#endif
#if FEAT_ENV_LIGHTS
extern	vmCvar_t		cg_envLights;
void	CG_AddEnvironmentLights( void );
#endif

#if FEAT_MUSIC_PLAYLIST
extern	vmCvar_t		cg_music;
void	CG_InitPlayList( void );
void	CG_ParsePlayList( void );
void	CG_StopPlayList( void );
void	CG_ContinuePlayList( void );
void	CG_ResetPlayList( void );
void	CG_RunPlayListFrame( void );
#endif

#if FEAT_LENS_FLARES
typedef enum {
	LFM_reflexion,
	LFM_glare,
	LFM_star
} lensFlareMode_t;

#define MAX_LENSFLARES_PER_EFFECT       16
#define MAX_MISSILE_LENSFLARE_EFFECTS   16

typedef struct {
	qhandle_t        shader;
	lensFlareMode_t  mode;
	float            pos;
	float            size;
	float            rgba[4];
	float            rotationOffset;
	float            rotationYawFactor;
	float            rotationPitchFactor;
	float            rotationRollFactor;
	float            fadeAngleFactor;
	float            entityAngleFactor;
	float            intensityThreshold;
} lensFlare_t;

typedef struct {
	char             name[64];
	float            range;
	float            rangeSqr;
	float            fadeAngle;
	int              numLensFlares;
	lensFlare_t      lensFlares[MAX_LENSFLARES_PER_EFFECT];
} lensFlareEffect_t;

extern	vmCvar_t		cg_lensFlare;
extern	vmCvar_t		cg_missileFlare;
extern	vmCvar_t		cg_powerupFlares;
void	CG_InitLensFlares( void );
void	CG_AddLensFlares( void );
void	CG_AddMissileFlare( centity_t *cent );
void	CG_AddPowerupFlare( centity_t *cent, int powerupTag );
#endif
#if FEAT_SPECTATOR_OUTLINES
extern	vmCvar_t		cg_specOutlines;
#endif
//extern	vmCvar_t		cg_pmove_fixed;
extern	vmCvar_t		cg_cameraOrbit;
extern	vmCvar_t		cg_cameraOrbitDelay;
extern	vmCvar_t		cg_timescaleFadeEnd;
extern	vmCvar_t		cg_timescaleFadeSpeed;
extern	vmCvar_t		cg_timescale;
extern	vmCvar_t		cg_cameraMode;
extern	vmCvar_t		cg_noTaunt;
extern	vmCvar_t		cg_noProjectileTrail;
#if FEAT_TA_UI
extern	vmCvar_t		cg_currentSelectedPlayer;
extern	vmCvar_t		cg_currentSelectedPlayerName;
#endif
extern	vmCvar_t		cg_envGroundDusty;
extern	vmCvar_t		cg_envTemperature;
#if FEAT_OVERLOAD
extern	vmCvar_t		cg_obeliskRespawnDelay;
#endif
extern  vmCvar_t        cg_switchToEmpty;
extern	vmCvar_t		cg_stretch;
extern	vmCvar_t		cg_fovAspectAdjust;
extern	vmCvar_t		cg_viewbob;
extern	vmCvar_t		cg_viewkick;
extern	vmCvar_t		cg_drawSpeed;
extern	vmCvar_t		cg_drawBotDirectives;
extern	vmCvar_t		cg_hitSounds;
#if FEAT_FOLLOW_KILLER
extern	vmCvar_t		cg_followKiller;
#endif
#if FEAT_AUTO_DEMO
extern	vmCvar_t		cg_autoRecord;
extern	vmCvar_t		cg_autoJoin;
#endif
#if FEAT_IMPACT_SPARKS
extern	vmCvar_t		cg_impactSparks;
#endif
#if FEAT_CHAT_FILTER
void		CG_ChatFilterIgnore_f( void );
void		CG_ChatFilterUnignore_f( void );
qboolean	CG_ChatFilterIsMuted( int clientNum );
#endif

// cg_wired_bridge.c — Wired UI: push game state to client for HUD rendering
#if FEAT_WIRED_UI
void	CG_WiredHudPushState( void );
#endif

//
// cg_characters.c
//
qboolean CG_LoadCharacter( clientInfo_t *ci, const char *charName );
qboolean trap_GetValue( char *value, int valueSize, const char *key );

// cg_alloc.c — cgame-local memory allocator
void	*CG_Alloc( int size );
void	CG_Free( void *ptr );
void	CG_AllocReset( void );
void	CG_AllocSetPermanent( int permanent );
#define Z_Malloc( size )    CG_Alloc( size )
#define Z_Free( ptr )       CG_Free( ptr )
#define OSP_MEMORY_CHECK( ptr ) \
	if ( !(ptr) ) { Com_Terminate( TERM_CLIENT_DROP, "%s:%d: out of memory\n", __FILE__, __LINE__ ); }

//
// cg_main.c
//
const char *CG_ConfigString( int index );
const char *CG_Argv( int arg );

void CG_StartMusic( void );

void CG_UpdateCvars( void );

void CG_KeyEvent(int key, qboolean down);
void CG_MouseEvent(int x, int y);
void CG_EventHandling(int type);
void CG_RankRunFrame( void );
void CG_BuildSpectatorString( void );


//
// cg_view.c
//
void CG_TestModel_f (void);
void CG_TestGun_f (void);
void CG_TestModelNextFrame_f (void);
void CG_TestModelPrevFrame_f (void);
void CG_TestModelNextSkin_f (void);
void CG_TestModelPrevSkin_f (void);
void CG_ZoomDown_f( void );
void CG_ZoomUp_f( void );
void CG_AddBufferedSound( sfxHandle_t sfx);

void CG_DrawActiveFrame( int serverTime, stereoFrame_t stereoView, qboolean demoPlayback );

// Normalized-coordinate draw helpers (0.0-1.0 screen space)
void CG_FillRectNorm( float nx, float ny, float nw, float nh, const float *color );

// Virtual-to-normalized conversion scales
#define NORM_HSCALE  0.001562500f
#define NORM_VSCALE  0.002083333f

//
// cg_draw.c, cg_newDraw.c
//
extern	int sortedTeamPlayers[TEAM_MAXOVERLAY];
extern	int	numSortedTeamPlayers;
extern	int drawTeamOverlayModificationCount;

void CG_AddLagometerFrameInfo( void );
void CG_AddLagometerSnapshotInfo( snapshot_t *snap );
void CG_CenterPrint( const char *str, int y, int charWidth );
void CG_DrawActive( stereoFrame_t stereoView );
void CG_SelectPrevPlayer( void );
void CG_SelectNextPlayer( void );
#if FEAT_TA_UI
void CG_ShowResponseHead( void );
#endif
qboolean CG_YourTeamHasFlag( void );
qboolean CG_OtherTeamHasFlag( void );

//
// cg_utils.c
//
qboolean CG_IsFollowing( void );
qboolean CG_IsSpectator( void );
qboolean CG_IsPlayerInvisible( centity_t *cent );
int CG_CrosshairPlayer( void );
int CG_LastAttacker( void );
void CG_ModernDrawFrame( float x, float y, float w, float h, const float *border, const float *borderColor, qboolean filled );


//
// cg_player.c
//
void CG_Player( centity_t *cent );
void CG_ResetPlayerEntity( centity_t *cent );
void CG_AddRefEntityWithPowerups( centity_t *cent, refEntity_t *ent, entityState_t *state, qboolean isPlayerPart, int team );
void CG_NewClientInfo( int clientNum );

//
// cg_predict.c
//
void CG_BuildSolidList( void );
int	CG_PointContents( const vec3_t point, int passEntityNum );
void CG_Trace( trace_t *result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
					 int skipNumber, int mask );
void CG_PredictPlayerState( void );
void CG_LoadDeferredPlayers( void );


//
// cg_events.c
//
void CG_CheckEvents( centity_t *cent );
const char	*CG_PlaceString( int rank );
const char	*CG_ClientName( const clientInfo_t *ci );
const char	*CG_ClientNameByNum( int clientNum );
void CG_EntityEvent( centity_t *cent, vec3_t position );
void CG_PainEvent( centity_t *cent, int health );

#if FEAT_EARTHQUAKE_SYSTEM
void CG_AddEarthquake( const vec3_t origin, float radius, float duration, float fadeIn, float fadeOut, float amplitude );
void CG_AdjustEarthquakes( const vec3_t delta );
#endif


//
// cg_ents.c
//
void CG_SetEntitySoundPosition( centity_t *cent );
void CG_AddPacketEntities( void );
void CG_Beam( centity_t *cent );
void CG_AdjustPositionForMover(const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out, vec3_t angles_in, vec3_t angles_out);

void CG_PositionEntityOnTag( refEntity_t *entity, const refEntity_t *parent,
							qhandle_t parentModel, char *tagName );
void CG_PositionRotatedEntityOnTag( refEntity_t *entity, const refEntity_t *parent,
							qhandle_t parentModel, char *tagName );



//
// cg_weapons.c
//
void CG_NextWeapon_f( void );
void CG_PrevWeapon_f( void );
void CG_Weapon_f( void );
void CG_WeaponGrabbed_f( void );

void CG_RegisterWeapon( int weaponNum );
void CG_RegisterItemVisuals( int itemNum );

void CG_FireWeapon( centity_t *cent );
void CG_MissileHitWall( int pType, int clientNum, vec3_t origin, vec3_t dir, impactSound_t soundType, int sourceEntityNum );
void CG_MissileHitPlayer( int pType, vec3_t origin, vec3_t dir, int entityNum );
void CG_ShotgunFire( entityState_t *es );
void CG_ShotgunFireWide( entityState_t *es );
void CG_Bullet( vec3_t origin, int sourceEntityNum, vec3_t normal, qboolean flesh, int fleshEntityNum );
// eser - explosions
void CG_ExplosionParticles(int pType, vec3_t origin);
// eser - explosions

void CG_RailTrail( clientInfo_t *ci, vec3_t start, vec3_t end );
void CG_AddRailTrails( void );
void CG_ClearRailTrails( void );
void CG_RegisterRailParticleClasses( void );      // cg_wired_particles.c
void CG_RegisterLightningParticleClasses( void ); // cg_wired_particles.c

// Phase 5T: generic player-trail infrastructure.
// Multiple trail types per player can be active concurrently
// (e.g., a haste-carrying flag runner shows both PUSH and FLAG
// trails). Each (client, type) pair has its own expiry timestamp;
// trails stack visually with independent fade.
//
// State is publicly accessible (cg_playerTrails[][]) so
// third-party hooks (game-mode logic, mod code) can extend trails
// directly. CG_TriggerPlayerTrail is the preferred API but not
// mandatory.
//
// Engine sees only generic primitive shader / particle handles;
// effect-specific naming (PTRAIL_PUSH, "pushTrail", etc.) lives
// only here in cgame and in shader.script.
typedef enum {
	PTRAIL_PUSH = 0,   // jumppad + haste/speed
	PTRAIL_COUNT       // sentinel — never use as an index
} playerTrailType_t;

// Color-resolution callback. NULL → def->defaultColor is used.
// Non-NULL → called per-render to compute color (e.g., flag
// carrier color from team membership).
typedef void (*CG_TrailColorFn)( int clientNum, vec3_t outColor );

typedef struct {
	qhandle_t           *shaderPtrPtr;          // address of cgs.media.* slot
	qhandle_t           *particleClassPtrPtr;   // address of cgs.media.* slot, may be NULL
	vec3_t               defaultColor;
	CG_TrailColorFn      colorResolveFn;        // NULL → defaultColor
	float                alphaCeiling;
	float                startWidth;
	float                endWidth;
	int                  axialCopies;
	int                  fadeWindowMs;
	float                particleRateMin;
	float                particleRateMax;
} playerTrailDef_t;

// Jumppad velocity → duration scaling (PTRAIL_PUSH-specific,
// shared between the EV_JUMP_PAD trigger in cg_event.c and the
// particle-rate scaling in cg_effects.c).
#define PTRAIL_JUMPPAD_DURATION_MIN_MS 200
#define PTRAIL_JUMPPAD_DURATION_MAX_MS 800
#define PTRAIL_JUMPPAD_SPEED_NORM      1500.0f

// Per-(client, type) expiry timestamp (cg.time-based).
// cg_playerTrails[client][type] > cg.time means the trail is
// active and renders this frame.
extern int cg_playerTrails[MAX_CLIENTS][PTRAIL_COUNT];

// Per-frame entry point. Called from cg_view.c after the other
// persistent-effect render calls.
void CG_AddPlayerTrails( void );

// Startup: populate the def table after cgs.media.* shader and
// particle handles are registered.
void CG_RegisterPlayerTrailDefs( void );

// Preferred API for triggering / extending a trail. Sets
// cg_playerTrails[clientNum][type] to max(current, cg.time +
// durationMs). Multiple sources merge naturally — highest
// expiry wins.
void CG_TriggerPlayerTrail( int clientNum,
                            playerTrailType_t type,
                            int durationMs );

void CG_RegisterPushParticleClasses( void );   // cg_wired_particles.c
void CG_GrappleTrail( centity_t *ent, const weaponInfo_t *wi );
void CG_AddViewWeapon (playerState_t *ps);
void CG_AddPlayerWeapon( refEntity_t *parent, playerState_t *ps, centity_t *cent, int team );

void CG_OutOfAmmoChange( void );	// should this be in pmove?

//
// cg_marks.c
//
void	CG_InitMarkPolys( void );
void	CG_AddMarks( void );
void	CG_ImpactMark( qhandle_t markShader,
				    const vec3_t origin, const vec3_t dir,
					float orientation,
				    float r, float g, float b, float a,
					qboolean alphaFade,
					float radius, qboolean temporary );

//
// cg_localents.c
//
void	CG_InitLocalEntities( void );
localEntity_t	*CG_AllocLocalEntity( void );
void	CG_AddLocalEntities( void );
void	CG_DrawPlumOverlays( void );

//
// cg_effects.c
//
localEntity_t *CG_SmokePuff( const vec3_t p,
				   const vec3_t vel,
				   float radius,
				   float r, float g, float b, float a,
				   float duration,
				   int startTime,
				   int fadeInTime,
				   int leFlags,
				   qhandle_t hShader );
void CG_BubbleTrail( vec3_t start, vec3_t end, float spacing );
// Phase 6.5.3: a small spray-of-droplets burst + watr_in sound at a point on a
// liquid surface. Pure helper — callers gate on cgs.q1Map.
void CG_WaterSplash( vec3_t point );
// Phase 6.5.3: given a hitscan segment [start,end], emit CG_WaterSplash() at
// wherever it pierces a liquid surface (entry and/or exit). No-op unless cgs.q1Map.
void CG_WaterCrossingSplashes( vec3_t start, vec3_t end );
void CG_SpawnEffect( vec3_t org );
void CG_KamikazeEffect( vec3_t org );
#if FEAT_OVERLOAD
void CG_ObeliskExplode( vec3_t org, int entityNum );
void CG_ObeliskPain( vec3_t org );
#endif
void CG_DeflectorImpact( vec3_t org, vec3_t angles );
void CG_DeflectorJuiced( vec3_t org );
void CG_LightningBoltBeam( vec3_t start, vec3_t end );
void CG_LightningArcBeam( vec3_t start, vec3_t end );
void CG_ScorePlum( int client, vec3_t org, int score );
#if FEAT_DAMAGE_PLUMS
void CG_DamagePlum( int client, vec3_t org, int damage );	// 2A
#endif
#if FEAT_PING_LOCATION
void CG_PingLocation( centity_t *cent );	// 4G
#endif
#if FEAT_IMPACT_SPARKS
void CG_ImpactSparks( vec3_t origin, vec3_t dir );	// 11A
#endif
void CG_LightningSparks( vec3_t origin, vec3_t dir );

void CG_GibPlayer( vec3_t playerOrigin );
void CG_BigExplode( vec3_t playerOrigin );

void CG_Bleed( vec3_t origin, int entityNum );

localEntity_t *CG_MakeExplosion( vec3_t origin, vec3_t dir,
								qhandle_t hModel, qhandle_t shader, int msec,
								qboolean isSprite );

// eser - lightning discharge
void CG_Lightning_Discharge(vec3_t origin, int msec);
// eser - lightning discharge

//
// cg_q1_particles.c
//
void CG_Q1_MaybeEmitTrail( centity_t *cent );

//
// cg_snapshot.c
//
void CG_ProcessSnapshots( void );


//
// cg_consolecmds.c
//
qboolean CG_ConsoleCommand( void );
void CG_InitConsoleCommands( void );

//
// cg_players.c
//
qboolean CG_WorldToScreen( vec3_t point, float *x, float *y );
void CG_Draw2DBotDirectives( void );

//
// cg_servercmds.c
//
void CG_ExecuteNewServerCommands( int latestSequence );
void CG_ParseServerinfo( void );
void CG_SetConfigValues( void );
void CG_ShaderStateChanged(void);
void CG_VoiceChatLocal( int mode, qboolean voiceOnly, int clientNum, int color, const char *cmd );
void CG_PlayBufferedVoiceChats( void );
//
// cg_playerstate.c
//
void CG_Respawn( void );
void CG_TransitionPlayerState( playerState_t *ps, playerState_t *ops );
void CG_CheckChangedPredictableEvents( playerState_t *ps );


//===============================================

//
// system traps
// These functions are how the cgame communicates with the main game system
//

// print message on the local console
void		trap_Print( const char *fmt );

// abort the game
void		trap_Error(const char *fmt) NORETURN;

// severity-preserving log and terminate
void		trap_Log( log_severity_t severity, const char *text );
void		NORETURN trap_Terminate( terminationReason_t reason, const char *text );

// milliseconds should only be used for performance tuning, never
// for anything game related.  Get time from the CG_DrawActiveFrame parameter
int			trap_Milliseconds( void );

// console variable interaction
void		trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
void		trap_Cvar_Update( vmCvar_t *vmCvar );
void		trap_Cvar_Set( const char *var_name, const char *value );
void		trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );

// ServerCommand and ConsoleCommand parameter access
int			trap_Argc( void );
void		trap_Argv( int n, char *buffer, int bufferLength );
void		trap_Args( char *buffer, int bufferLength );

// filesystem access
// returns length of file
int			trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
void		trap_FS_Read( void *buffer, int len, fileHandle_t f );
void		trap_FS_Write( const void *buffer, int len, fileHandle_t f );
void		trap_FS_FCloseFile( fileHandle_t f );
int			trap_FS_Seek( fileHandle_t f, long offset, int origin ); // fsOrigin_t

// add commands to the local console as if they were typed in
// for map changing, etc.  The command is not executed immediately,
// but will be executed in order the next time console commands
// are processed
void		trap_SendConsoleCommand( const char *text );

// register a command name so the console can perform command completion.
// FIXME: replace this with a normal console command "defineCommand"?
void		trap_AddCommand( const char *cmdName );
void		trap_RemoveCommand( const char *cmdName );

// send a string to the server over the network
void		trap_SendClientCommand( const char *s );

// force a screen update, only used during gamestate load
void		trap_UpdateScreen( void );

// model collision
void		trap_CM_LoadMap( const char *mapname );
int			trap_CM_NumInlineModels( void );
clipHandle_t trap_CM_InlineModel( int index );		// 0 = world, 1+ = bmodels
clipHandle_t trap_CM_TempBoxModel( const vec3_t mins, const vec3_t maxs );
int			trap_CM_PointContents( const vec3_t p, clipHandle_t model );
int			trap_CM_TransformedPointContents( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles );
void		trap_CM_BoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
					  const vec3_t mins, const vec3_t maxs,
					  clipHandle_t model, int brushmask );
void		trap_CM_CapsuleTrace( trace_t *results, const vec3_t start, const vec3_t end,
					  const vec3_t mins, const vec3_t maxs,
					  clipHandle_t model, int brushmask );
void		trap_CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
					  const vec3_t mins, const vec3_t maxs,
					  clipHandle_t model, int brushmask,
					  const vec3_t origin, const vec3_t angles );
void		trap_CM_TransformedCapsuleTrace( trace_t *results, const vec3_t start, const vec3_t end,
					  const vec3_t mins, const vec3_t maxs,
					  clipHandle_t model, int brushmask,
					  const vec3_t origin, const vec3_t angles );

// Returns the projection of a polygon onto the solid brushes in the world
int			trap_CM_MarkFragments( int numPoints, const vec3_t *points,
			const vec3_t projection,
			int maxPoints, vec3_t pointBuffer,
			int maxFragments, markFragment_t *fragmentBuffer );

// normal sounds will have their volume dynamically changed as their entity
// moves and the listener moves
void		trap_S_StartSound( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx );
void		trap_S_StopLoopingSound(int entnum);

// a local sound is always played full volume
void		trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum );
void		trap_S_ClearLoopingSounds( qboolean killall );
void		trap_S_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
void		trap_S_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
void		trap_S_UpdateEntityPosition( int entityNum, const vec3_t origin );

// respatialize recalculates the volumes of sound as they should be heard by the
// given entityNum and position
void		trap_S_Respatialize( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater );
sfxHandle_t	trap_S_RegisterSound( const char *sample, qboolean compressed );		// returns buzz if not found
int			trap_S_SoundDuration( sfxHandle_t handle );	// Phase 6.2: returns sound length in milliseconds
void		trap_S_StartBackgroundTrack( const char *intro, const char *loop );	// empty name stops music
void	trap_S_StopBackgroundTrack( void );


void		trap_R_LoadWorldMap( const char *mapname );

// all media should be registered during level startup to prevent
// hitches during gameplay
qhandle_t	trap_R_RegisterModel( const char *name );			// returns rgb axis if not found
qhandle_t	trap_R_RegisterSkin( const char *name );			// returns all white if not found
qhandle_t	trap_R_RegisterShader( const char *name );			// returns all white if not found
qhandle_t	trap_R_RegisterShaderNoMip( const char *name );			// returns all white if not found
qhandle_t	trap_R_RegisterPrimitiveShader( const char *name );		// returns all white if not found; primitive consumers (ribbon/beam) sample the shader's image by handle

// a scene is built up by calls to R_ClearScene and the various R_Add functions.
// Nothing is drawn until R_RenderScene is called.
void		trap_R_ClearScene( void );
void		trap_R_AddRefEntityToScene( const refEntity_t *re );

// polys are intended for simple wall marks, not really for doing
// significant construction
void		trap_R_AddPolyToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts );
void		trap_R_AddPolysToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts, int numPolys );
void		trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void		trap_R_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b );
int			trap_R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
void		trap_R_RenderScene( const refdef_t *fd );
void		trap_R_SetColor( const float *rgba );	// NULL = 1,1,1,1
/* Legacy stretch-pic wrapper removed -- all callers use trap_R_DrawStretchPicNorm */
void		trap_R_DrawStretchPicNorm( float nx, float ny, float nw, float nh,
			float s1, float t1, float s2, float t2, qhandle_t hShader );
void		trap_R_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs );
int			trap_R_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame,
					   float frac, const char *tagName );
void		trap_R_RemapShader( const char *oldShader, const char *newShader, const char *timeOffset );
qboolean	trap_R_inPVS( const vec3_t p1, const vec3_t p2 );
#if FEAT_IQM
int		trap_R_GetIQMAnimations( qhandle_t model, iqmAnimInfo_t *anims, int maxAnims );
#endif // FEAT_IQM
void		trap_R_SetLightstylePattern( int style, const char *pattern );

// The glconfig_t will not change during the life of a cgame.
// If it needs to change, the entire cgame will be restarted, because
// all the qhandle_t are then invalid.
void		trap_GetGlconfig( glconfig_t *glconfig );

// the gamestate should be grabbed at startup, and whenever a
// configstring changes
void		trap_GetGameState( gameState_t *gamestate );

// cgame will poll each frame to see if a newer snapshot has arrived
// that it is interested in.  The time is returned separately so that
// snapshot latency can be calculated.
void		trap_GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime );

// a snapshot get can fail if the snapshot (or the entties it holds) is so
// old that it has fallen out of the client system queue
qboolean	trap_GetSnapshot( int snapshotNumber, snapshot_t *snapshot );

// retrieve a text command from the server stream
// the current snapshot will hold the number of the most recent command
// qfalse can be returned if the client system handled the command
// argc() / argv() can be used to examine the parameters of the command
qboolean	trap_GetServerCommand( int serverCommandNumber );

// returns the most recent command number that can be passed to GetUserCmd
// this will always be at least one higher than the number in the current
// snapshot, and it may be quite a few higher if it is a fast computer on
// a lagged connection
int			trap_GetCurrentCmdNumber( void );

qboolean	trap_GetUserCmd( int cmdNumber, usercmd_t *ucmd );

// used for the weapon select and zoom
void		trap_SetUserCmdValue( int stateValue, float sensitivityScale );

// aids for VM testing
void		testPrintInt( char *string, int i );
void		testPrintFloat( char *string, float f );

int			trap_MemoryRemaining( void );
void		trap_R_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);
qboolean	trap_Key_IsDown( int keynum );
int			trap_Key_GetCatcher( void );
void		trap_Key_SetCatcher( int catcher );
int			trap_Key_GetKey( const char *binding );


typedef enum {
  SYSTEM_PRINT,
  CHAT_PRINT,
  TEAMCHAT_PRINT
} q3print_t;


int trap_CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits);
e_status trap_CIN_StopCinematic(int handle);
e_status trap_CIN_RunCinematic (int handle);
void trap_CIN_DrawCinematic (int handle);
void trap_CIN_SetExtents (int handle, int x, int y, int w, int h);

int			trap_RealTime(qtime_t *qtime);
void		trap_SnapVector( float *v );

qboolean	trap_loadCamera(const char *name);
void		trap_startCamera(int time);
qboolean	trap_getCameraInfo(int time, vec3_t *origin, vec3_t *angles);

qboolean	trap_GetEntityToken( char *buffer, int bufferSize );

void	CG_ClearParticles (void);
void	CG_AddParticles (void);
void	CG_ParticleSnow (qhandle_t pshader, vec3_t origin, vec3_t origin2, int turb, float range, int snum);
void	CG_ParticleSmoke (qhandle_t pshader, centity_t *cent);
void	CG_AddParticleShrapnel (localEntity_t *le);
void	CG_ParticleSnowFlurry (qhandle_t pshader, centity_t *cent);
void	CG_ParticleBulletDebris (vec3_t	org, vec3_t vel, int duration);
void	CG_ParticleSparks (vec3_t org, vec3_t vel, int duration, float x, float y, float speed);
void	CG_ParticleDust (centity_t *cent, vec3_t origin, vec3_t dir);
void	CG_ParticleMisc (qhandle_t pshader, vec3_t origin, int size, int duration, float alpha);
void	CG_ParticleExplosion (char *animStr, vec3_t origin, vec3_t vel, int duration, int sizeStart, int sizeEnd);
extern qboolean		initparticles;
int CG_NewParticleArea ( int num );

//
// cg_znudge.c — client-side forward extrapolation
//
#if FEAT_ZNUDGE
extern	vmCvar_t	cg_znudge;
extern	vmCvar_t	cg_znSmoothweight;
extern	vmCvar_t	cg_znProjectiles;
extern	vmCvar_t	cg_znOffset;
extern	vmCvar_t	cg_znPingWeight;
extern	vmCvar_t	cg_znGravity;
extern	vmCvar_t	cg_znMaxclips;
extern	vmCvar_t	cg_znClimbheight;
extern	vmCvar_t	cg_znRunningspeed;
extern	vmCvar_t	cg_znDrawball;

float	ZN_GetNudge( void );
void	ZN_GetVelocity( centity_t *cent, vec3_t velocity );
void	ZN_PredictSimple( vec3_t origin, vec3_t velocity, float gravity, float nudge, vec3_t predicted );
void	ZN_PredictPlayer( centity_t *cent, float nudge, vec3_t predictedOrigin );
void	ZN_PredictMissile( centity_t *cent, float nudge, vec3_t predictedOrigin );
void	ZN_PredictGrenade( centity_t *cent, float nudge, vec3_t predictedOrigin );
#endif

#endif // CG_LOCAL_H
