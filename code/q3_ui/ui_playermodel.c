/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
#include "ui_local.h"

#define MODEL_BACK0			"menu/art/back_0"
#define MODEL_BACK1			"menu/art/back_1"
#define MODEL_SELECT		"menu/art/opponents_select"
#define MODEL_SELECTED		"menu/art/opponents_selected"
#define MODEL_FRAMEL		"menu/art/frame1_l"
#define MODEL_FRAMER		"menu/art/frame1_r"
#define MODEL_PORTS			"menu/art/player_models_ports"
#define MODEL_ARROWS		"menu/art/gs_arrows_0"
#define MODEL_ARROWSL		"menu/art/gs_arrows_l"
#define MODEL_ARROWSR		"menu/art/gs_arrows_r"

#define LOW_MEMORY			(5 * 1024 * 1024)

static char* playermodel_artlist[] =
{
	MODEL_BACK0,
	MODEL_BACK1,
	MODEL_SELECT,
	MODEL_SELECTED,
	MODEL_FRAMEL,
	MODEL_FRAMER,
	MODEL_PORTS,
	MODEL_ARROWS,
	MODEL_ARROWSL,
	MODEL_ARROWSR,
	NULL
};

// model grid: 4 columns × 3 rows = 12 models per page
// the 4th row is used for the skin strip
#define PLAYERGRID_COLS		4
#define PLAYERGRID_ROWS		3
#define MAX_MODELSPERPAGE	(PLAYERGRID_ROWS*PLAYERGRID_COLS)

#define MAX_PLAYERMODELS	256

// skin strip: up to 4 skins visible at a time (same column layout as model grid)
#define MAX_SKINSPERPAGE	4
#define MAX_PLAYERSKINS		64
#define SKINROW_Y			275

// animation preview
#define NUM_ANIM_BUTTONS	5

// --- control IDs ---
#define ID_PLAYERPIC0		0
// ID_PLAYERPIC1 through ID_PLAYERPIC11 = 1..11
#define ID_SKINPIC0			20
// ID_SKINPIC1 through ID_SKINPIC3 = 21..23
#define ID_PREVPAGE			100
#define ID_NEXTPAGE			101
#define ID_BACK				102
#define ID_ANIM0			110
// ID_ANIM1 through ID_ANIM4 = 111..114

// animation definitions for preview
typedef struct {
	const char	*label;
	int			legsAnim;
	int			torsoAnim;
} animDef_t;

static animDef_t s_animDefs[NUM_ANIM_BUTTONS] = {
	{ "STAND",   LEGS_IDLE,   TORSO_STAND },
	{ "RUN",     LEGS_RUN,    TORSO_STAND },
	{ "ATTACK",  LEGS_IDLE,   TORSO_ATTACK },
	{ "CROUCH",  LEGS_IDLECR, TORSO_STAND },
	{ "TAUNT",   LEGS_IDLE,   TORSO_GESTURE },
};

typedef struct
{
	menuframework_s	menu;

	// model grid (4×3 = 12 per page)
	menubitmap_s	pics[MAX_MODELSPERPAGE];
	menubitmap_s	picbuttons[MAX_MODELSPERPAGE];

	// skin strip (4 per page, single-layer bitmaps)
	menubitmap_s	skins[MAX_SKINSPERPAGE];

	// decoration and controls
	menubitmap_s	framel;
	menubitmap_s	framer;
	menubitmap_s	ports;
	menutext_s		banner;
	menubitmap_s	back;
	menubitmap_s	player;
	menubitmap_s	arrows;
	menubitmap_s	left;
	menubitmap_s	right;
	menutext_s		modelname;
	menutext_s		skinname;
	menutext_s		playername;

	// animation preview text buttons
	menutext_s		animItems[NUM_ANIM_BUTTONS];

	playerInfo_t	playerinfo;

	// model list (one entry per unique model, using default icon)
	int				nummodels;
	char			modelnames[MAX_PLAYERMODELS][128];
	int				modelpage;
	int				numpages;
	int				selectedmodel;

	// skin list (all skins for the currently selected model)
	int				numskins;
	char			skinnames[MAX_PLAYERSKINS][128];
	int				skinpage;
	int				numskinpages;
	int				selectedskin;

	// combined model/skin for cvar and preview
	char			modelskin[64];

	// animation preview state
	int				currentAnim;	// index into s_animDefs, -1 = default

	// model rotation
	float			spinAngle;
} playermodel_t;

static playermodel_t s_playermodel;

// static text buffers for menu text items
static char s_playername[32];
static char s_modelname[32];
static char s_skinname[32];
static char s_animLabels[NUM_ANIM_BUTTONS][16];


/*
=================
PlayerModel_GetModelDir

Extract just the model directory name from a full icon path.
"models/players/sarge/icon_default" → "sarge"
=================
*/
static void PlayerModel_GetModelDir( const char *fullpath, char *out, int outSize )
{
	const char *p;
	const char *slash;
	int len;

	p = fullpath + strlen("models/players/");
	slash = strchr(p, '/');
	if (slash) {
		len = slash - p;
		if (len >= outSize) len = outSize - 1;
		Q_strncpyz(out, p, len + 1);
	} else {
		Q_strncpyz(out, p, outSize);
	}
}


/*
=================
PlayerModel_GetSkinFromIcon

Extract skin name from an icon path.
"models/players/sarge/icon_blue" → "blue"
=================
*/
static const char *PlayerModel_GetSkinFromIcon( const char *fullpath )
{
	const char *p = strstr(fullpath, "icon_");
	if (p) return p + 5;
	return "default";
}


/*
=================
PlayerModel_UpdateGrid
=================
*/
static void PlayerModel_UpdateGrid( void )
{
	int	i;
	int	j;

	j = s_playermodel.modelpage * MAX_MODELSPERPAGE;
	for (i=0; i<MAX_MODELSPERPAGE; i++,j++)
	{
		if (j < s_playermodel.nummodels)
		{
			// model portrait (default skin icon)
			s_playermodel.pics[i].generic.name         = s_playermodel.modelnames[j];
			s_playermodel.picbuttons[i].generic.flags &= ~QMF_INACTIVE;
		}
		else
		{
			// dead slot
			s_playermodel.pics[i].generic.name         = NULL;
			s_playermodel.picbuttons[i].generic.flags |= QMF_INACTIVE;
		}

		s_playermodel.pics[i].generic.flags       &= ~QMF_HIGHLIGHT;
		s_playermodel.pics[i].shader               = 0;
		s_playermodel.picbuttons[i].generic.flags |= QMF_PULSEIFFOCUS;
	}

	if (s_playermodel.selectedmodel/MAX_MODELSPERPAGE == s_playermodel.modelpage)
	{
		// set selected model
		i = s_playermodel.selectedmodel % MAX_MODELSPERPAGE;

		s_playermodel.pics[i].generic.flags       |= QMF_HIGHLIGHT;
		s_playermodel.picbuttons[i].generic.flags &= ~QMF_PULSEIFFOCUS;
	}

	if (s_playermodel.numpages > 1)
	{
		if (s_playermodel.modelpage > 0)
			s_playermodel.left.generic.flags &= ~QMF_INACTIVE;
		else
			s_playermodel.left.generic.flags |= QMF_INACTIVE;

		if (s_playermodel.modelpage < s_playermodel.numpages-1)
			s_playermodel.right.generic.flags &= ~QMF_INACTIVE;
		else
			s_playermodel.right.generic.flags |= QMF_INACTIVE;
	}
	else
	{
		// hide left/right markers
		s_playermodel.left.generic.flags |= QMF_INACTIVE;
		s_playermodel.right.generic.flags |= QMF_INACTIVE;
	}
}


/*
=================
PlayerModel_LoadSkins

Load all skins for the model at the given index in modelnames[].
Populates skinnames[], numskins, numskinpages.
Orders: icon_default first, then icon_blue, icon_red, then rest.
=================
*/
static void PlayerModel_LoadSkins( int modelIndex )
{
	char	modelDir[64];
	int		numfiles;
	char	filelist[2048];
	char	skinname[MAX_QPATH];
	char*	fileptr;
	int		i, j;
	int		filelen;
	int		defaultIdx, blueIdx, redIdx;
	char	temp[128];

	s_playermodel.numskins = 0;
	s_playermodel.skinpage = 0;

	if (modelIndex < 0 || modelIndex >= s_playermodel.nummodels)
		return;

	PlayerModel_GetModelDir(s_playermodel.modelnames[modelIndex], modelDir, sizeof(modelDir));

	// enumerate all icon_* files
	numfiles = trap_FS_GetFileList(va("models/players/%s", modelDir), "tga", filelist, 2048);
	fileptr = filelist;
	for (i=0; i<numfiles && s_playermodel.numskins < MAX_PLAYERSKINS; i++, fileptr+=filelen+1)
	{
		filelen = strlen(fileptr);
		COM_StripExtension(fileptr, skinname, sizeof(skinname));

		if (!Q_stricmpn(skinname, "icon_", 5))
		{
			Com_sprintf(s_playermodel.skinnames[s_playermodel.numskins++],
				sizeof(s_playermodel.skinnames[0]),
				"models/players/%s/%s", modelDir, skinname);
		}
	}

	// order: default first, blue second, red third
	defaultIdx = -1;
	blueIdx = -1;
	redIdx = -1;
	for (i=0; i<s_playermodel.numskins; i++)
	{
		if (strstr(s_playermodel.skinnames[i], "icon_default")) defaultIdx = i;
		else if (strstr(s_playermodel.skinnames[i], "icon_blue")) blueIdx = i;
		else if (strstr(s_playermodel.skinnames[i], "icon_red")) redIdx = i;
	}

	// bubble preferred skins to front
	j = 0;
	if (defaultIdx >= 0 && defaultIdx != j) {
		Q_strncpyz(temp, s_playermodel.skinnames[j], sizeof(temp));
		Q_strncpyz(s_playermodel.skinnames[j], s_playermodel.skinnames[defaultIdx], sizeof(s_playermodel.skinnames[0]));
		Q_strncpyz(s_playermodel.skinnames[defaultIdx], temp, sizeof(s_playermodel.skinnames[0]));
		// fix up indices after swap
		if (blueIdx == j) blueIdx = defaultIdx;
		if (redIdx == j) redIdx = defaultIdx;
		j++;
	} else if (defaultIdx == j) {
		j++;
	}

	if (blueIdx >= 0 && blueIdx != j) {
		Q_strncpyz(temp, s_playermodel.skinnames[j], sizeof(temp));
		Q_strncpyz(s_playermodel.skinnames[j], s_playermodel.skinnames[blueIdx], sizeof(s_playermodel.skinnames[0]));
		Q_strncpyz(s_playermodel.skinnames[blueIdx], temp, sizeof(s_playermodel.skinnames[0]));
		if (redIdx == j) redIdx = blueIdx;
		j++;
	} else if (blueIdx == j) {
		j++;
	}

	if (redIdx >= 0 && redIdx != j) {
		Q_strncpyz(temp, s_playermodel.skinnames[j], sizeof(temp));
		Q_strncpyz(s_playermodel.skinnames[j], s_playermodel.skinnames[redIdx], sizeof(s_playermodel.skinnames[0]));
		Q_strncpyz(s_playermodel.skinnames[redIdx], temp, sizeof(s_playermodel.skinnames[0]));
	}

	s_playermodel.numskinpages = s_playermodel.numskins / MAX_SKINSPERPAGE;
	if (s_playermodel.numskins % MAX_SKINSPERPAGE)
		s_playermodel.numskinpages++;
}


/*
=================
PlayerModel_UpdateSkinGrid
=================
*/
static void PlayerModel_UpdateSkinGrid( void )
{
	int i;
	int j;

	j = s_playermodel.skinpage * MAX_SKINSPERPAGE;
	for (i=0; i<MAX_SKINSPERPAGE; i++, j++)
	{
		if (j < s_playermodel.numskins)
		{
			s_playermodel.skins[i].generic.name = s_playermodel.skinnames[j];
			s_playermodel.skins[i].generic.flags &= ~QMF_INACTIVE;
		}
		else
		{
			s_playermodel.skins[i].generic.name = NULL;
			s_playermodel.skins[i].generic.flags |= QMF_INACTIVE;
		}

		s_playermodel.skins[i].generic.flags &= ~QMF_HIGHLIGHT;
		s_playermodel.skins[i].shader = 0;
		s_playermodel.skins[i].generic.flags |= QMF_PULSEIFFOCUS;
	}

	// highlight selected skin
	if (s_playermodel.selectedskin >= 0 &&
		s_playermodel.selectedskin / MAX_SKINSPERPAGE == s_playermodel.skinpage)
	{
		i = s_playermodel.selectedskin % MAX_SKINSPERPAGE;
		s_playermodel.skins[i].generic.flags |= QMF_HIGHLIGHT;
		s_playermodel.skins[i].generic.flags &= ~QMF_PULSEIFFOCUS;
	}
}


/*
=================
PlayerModel_UpdateModel
=================
*/
static void PlayerModel_UpdateModel( void )
{
	vec3_t	viewangles;
	vec3_t	moveangles;
	int		legsAnim;
	int		torsoAnim;

	memset( &s_playermodel.playerinfo, 0, sizeof(playerInfo_t) );

	viewangles[YAW]   = 180 - 30 + s_playermodel.spinAngle;
	viewangles[PITCH] = 0;
	viewangles[ROLL]  = 0;
	VectorClear( moveangles );

	// choose animation
	if (s_playermodel.currentAnim >= 0 && s_playermodel.currentAnim < NUM_ANIM_BUTTONS) {
		legsAnim  = s_animDefs[s_playermodel.currentAnim].legsAnim;
		torsoAnim = s_animDefs[s_playermodel.currentAnim].torsoAnim;
	} else {
		legsAnim  = LEGS_IDLE;
		torsoAnim = TORSO_STAND;
	}

	UI_PlayerInfo_SetModel( &s_playermodel.playerinfo, s_playermodel.modelskin );
	UI_PlayerInfo_SetInfo( &s_playermodel.playerinfo, legsAnim, torsoAnim, viewangles, moveangles, WP_MACHINEGUN, qfalse );
}


/*
=================
PlayerModel_SaveChanges
=================
*/
static void PlayerModel_SaveChanges( void )
{
	trap_Cvar_Set( "model", s_playermodel.modelskin );
	trap_Cvar_Set( "headmodel", s_playermodel.modelskin );
	trap_Cvar_Set( "team_model", s_playermodel.modelskin );
	trap_Cvar_Set( "team_headmodel", s_playermodel.modelskin );
}


/*
=================
PlayerModel_SetModelskin

Build the "model/skin" string from the selected model and skin.
Updates modelskin, modelname.string, and skinname.string.
=================
*/
static void PlayerModel_SetModelskin( void )
{
	char	modelDir[64];
	const char	*skin;
	int		maxlen;

	if (s_playermodel.selectedmodel < 0)
		return;

	PlayerModel_GetModelDir(s_playermodel.modelnames[s_playermodel.selectedmodel], modelDir, sizeof(modelDir));

	if (s_playermodel.selectedskin >= 0 && s_playermodel.selectedskin < s_playermodel.numskins) {
		skin = PlayerModel_GetSkinFromIcon(s_playermodel.skinnames[s_playermodel.selectedskin]);
	} else {
		skin = "default";
	}

	Com_sprintf(s_playermodel.modelskin, sizeof(s_playermodel.modelskin), "%s/%s", modelDir, skin);

	// update display strings
	maxlen = sizeof(s_modelname) - 1;
	Q_strncpyz(s_modelname, modelDir, maxlen + 1);
	Q_strupr(s_modelname);

	maxlen = sizeof(s_skinname) - 1;
	Q_strncpyz(s_skinname, skin, maxlen + 1);
	Q_strupr(s_skinname);
}


/*
=================
PlayerModel_MenuEvent
=================
*/
static void PlayerModel_MenuEvent( void* ptr, int event )
{
	if (event != QM_ACTIVATED)
		return;

	switch (((menucommon_s*)ptr)->id)
	{
		case ID_PREVPAGE:
			if (s_playermodel.modelpage > 0)
			{
				s_playermodel.modelpage--;
				PlayerModel_UpdateGrid();
			}
			break;

		case ID_NEXTPAGE:
			if (s_playermodel.modelpage < s_playermodel.numpages-1)
			{
				s_playermodel.modelpage++;
				PlayerModel_UpdateGrid();
			}
			break;

		case ID_BACK:
			PlayerModel_SaveChanges();
			UI_PopMenu();
			break;
	}
}


/*
=================
PlayerModel_AnimEvent
=================
*/
static void PlayerModel_AnimEvent( void* ptr, int event )
{
	int id;

	if (event != QM_ACTIVATED)
		return;

	id = ((menucommon_s*)ptr)->id - ID_ANIM0;
	if (id < 0 || id >= NUM_ANIM_BUTTONS)
		return;

	// toggle: clicking same animation again returns to default (STAND)
	if (s_playermodel.currentAnim == id) {
		s_playermodel.currentAnim = 0;	// back to STAND
	} else {
		s_playermodel.currentAnim = id;
	}

	if( trap_MemoryRemaining() > LOW_MEMORY ) {
		PlayerModel_UpdateModel();
	}
}


/*
=================
PlayerModel_MenuKey
=================
*/
static sfxHandle_t PlayerModel_MenuKey( int key )
{
	menucommon_s*	m;
	int				picnum;

	// mouse wheel or [ ] keys to spin the preview
	if ( key == K_MWHEELUP || key == '[' ) {
		s_playermodel.spinAngle -= 15;
		PlayerModel_UpdateModel();
		return 0;
	}

	if ( key == K_MWHEELDOWN || key == ']' ) {
		s_playermodel.spinAngle += 15;
		PlayerModel_UpdateModel();
		return 0;
	}

	switch (key)
	{
		case K_KP_LEFTARROW:
		case K_LEFTARROW:
			m = Menu_ItemAtCursor(&s_playermodel.menu);
			picnum = m->id - ID_PLAYERPIC0;
			if (picnum >= 0 && picnum < MAX_MODELSPERPAGE)
			{
				if (picnum > 0)
				{
					Menu_SetCursor(&s_playermodel.menu, s_playermodel.menu.cursor-1);
					return (menu_move_sound);
				}
				else if (s_playermodel.modelpage > 0)
				{
					s_playermodel.modelpage--;
					Menu_SetCursor(&s_playermodel.menu, s_playermodel.menu.cursor + MAX_MODELSPERPAGE - 1);
					PlayerModel_UpdateGrid();
					return (menu_move_sound);
				}
				else
					return (menu_buzz_sound);
			}
			break;

		case K_KP_RIGHTARROW:
		case K_RIGHTARROW:
			m = Menu_ItemAtCursor(&s_playermodel.menu);
			picnum = m->id - ID_PLAYERPIC0;
			if (picnum >= 0 && picnum < MAX_MODELSPERPAGE)
			{
				if ((picnum < MAX_MODELSPERPAGE - 1) && (s_playermodel.modelpage*MAX_MODELSPERPAGE + picnum+1 < s_playermodel.nummodels))
				{
					Menu_SetCursor(&s_playermodel.menu, s_playermodel.menu.cursor+1);
					return (menu_move_sound);
				}
				else if ((picnum == MAX_MODELSPERPAGE - 1) && (s_playermodel.modelpage < s_playermodel.numpages-1))
				{
					s_playermodel.modelpage++;
					Menu_SetCursor(&s_playermodel.menu, s_playermodel.menu.cursor - (MAX_MODELSPERPAGE - 1));
					PlayerModel_UpdateGrid();
					return (menu_move_sound);
				}
				else
					return (menu_buzz_sound);
			}
			break;

		case K_MOUSE2:
		case K_ESCAPE:
			PlayerModel_SaveChanges();
			break;
	}

	return ( Menu_DefaultKey( &s_playermodel.menu, key ) );
}


/*
=================
PlayerModel_PicEvent

Called when a model icon in the grid is clicked.
Selects the model, loads its skins, picks the default skin.
=================
*/
static void PlayerModel_PicEvent( void* ptr, int event )
{
	int		modelnum;
	int		i;

	if (event != QM_ACTIVATED)
		return;

	// reset all model highlights
	for (i=0; i<MAX_MODELSPERPAGE; i++)
	{
		s_playermodel.pics[i].generic.flags       &= ~QMF_HIGHLIGHT;
		s_playermodel.picbuttons[i].generic.flags |= QMF_PULSEIFFOCUS;
	}

	// set selected
	i = ((menucommon_s*)ptr)->id - ID_PLAYERPIC0;
	s_playermodel.pics[i].generic.flags       |= QMF_HIGHLIGHT;
	s_playermodel.picbuttons[i].generic.flags &= ~QMF_PULSEIFFOCUS;

	modelnum = s_playermodel.modelpage*MAX_MODELSPERPAGE + i;
	s_playermodel.selectedmodel = modelnum;

	// load skins for this model
	PlayerModel_LoadSkins(modelnum);

	// select first skin (icon_default) by default
	s_playermodel.selectedskin = 0;
	s_playermodel.skinpage = 0;

	// try to match current skin if switching between models
	if (s_playermodel.numskins > 0) {
		const char *currentSkin;
		const char *testSkin;
		int j;

		currentSkin = strrchr(s_playermodel.modelskin, '/');
		if (currentSkin) {
			currentSkin++;	// skip the /
			for (j=0; j<s_playermodel.numskins; j++) {
				testSkin = PlayerModel_GetSkinFromIcon(s_playermodel.skinnames[j]);
				if (!Q_stricmp(currentSkin, testSkin)) {
					s_playermodel.selectedskin = j;
					s_playermodel.skinpage = j / MAX_SKINSPERPAGE;
					break;
				}
			}
		}
	}

	PlayerModel_UpdateSkinGrid();
	PlayerModel_SetModelskin();

	if( trap_MemoryRemaining() > LOW_MEMORY ) {
		PlayerModel_UpdateModel();
	}
}


/*
=================
PlayerModel_SkinPicEvent

Called when a skin icon in the strip is clicked.
Updates the skin selection and model preview.
=================
*/
static void PlayerModel_SkinPicEvent( void* ptr, int event )
{
	int		skinnum;
	int		i;

	if (event != QM_ACTIVATED)
		return;

	// reset all skin highlights
	for (i=0; i<MAX_SKINSPERPAGE; i++)
	{
		s_playermodel.skins[i].generic.flags &= ~QMF_HIGHLIGHT;
		s_playermodel.skins[i].generic.flags |= QMF_PULSEIFFOCUS;
	}

	// set selected
	i = ((menucommon_s*)ptr)->id - ID_SKINPIC0;
	s_playermodel.skins[i].generic.flags |= QMF_HIGHLIGHT;
	s_playermodel.skins[i].generic.flags &= ~QMF_PULSEIFFOCUS;

	skinnum = s_playermodel.skinpage * MAX_SKINSPERPAGE + i;
	s_playermodel.selectedskin = skinnum;

	PlayerModel_SetModelskin();

	if( trap_MemoryRemaining() > LOW_MEMORY ) {
		PlayerModel_UpdateModel();
	}
}


/*
=================
PlayerModel_DrawPlayer
=================
*/
static void PlayerModel_DrawPlayer( void *self )
{
	menubitmap_s*	b;

	b = (menubitmap_s*) self;

	if( trap_MemoryRemaining() <= LOW_MEMORY ) {
		UI_DrawProportionalString( b->generic.x, b->generic.y + b->height / 2, "LOW MEMORY", UI_LEFT, color_red );
		return;
	}

	UI_DrawPlayer( b->generic.x, b->generic.y, b->width, b->height, &s_playermodel.playerinfo, uis.realtime/2 );
}


/*
=================
PlayerModel_BuildList

Builds the model list with ONE entry per unique model.
Instead of listing every icon_*, we find the default (or first) icon for each model directory.
=================
*/
static void PlayerModel_BuildList( void )
{
	int		numdirs;
	int		numfiles;
	char	dirlist[2048];
	char	filelist[2048];
	char	skinname[MAX_QPATH];
	char*	dirptr;
	char*	fileptr;
	int		i;
	int		j;
	int		dirlen;
	int		filelen;
	qboolean precache;
	char*	defaultSkin;

	precache = trap_Cvar_VariableValue("com_buildscript");

	s_playermodel.modelpage = 0;
	s_playermodel.nummodels = 0;

	// iterate directory of all player models
	numdirs = trap_FS_GetFileList("models/players", "/", dirlist, 2048 );
	dirptr  = dirlist;
	for (i=0; i<numdirs && s_playermodel.nummodels < MAX_PLAYERMODELS; i++,dirptr+=dirlen+1)
	{
		dirlen = strlen(dirptr);

		if (dirlen && dirptr[dirlen-1]=='/') dirptr[dirlen-1]='\0';

		if (!strcmp(dirptr,".") || !strcmp(dirptr,".."))
			continue;

		// find the best icon for this model directory
		defaultSkin = NULL;
		numfiles = trap_FS_GetFileList( va("models/players/%s",dirptr), "tga", filelist, 2048 );
		fileptr  = filelist;
		for (j=0; j<numfiles; j++, fileptr+=filelen+1)
		{
			filelen = strlen(fileptr);
			COM_StripExtension(fileptr, skinname, sizeof(skinname));

			if (!Q_stricmpn(skinname, "icon_", 5))
			{
				// prefer icon_default
				if (!Q_stricmp(skinname, "icon_default")) {
					defaultSkin = fileptr;
					break;	// can't do better
				}
				if (!defaultSkin)
					defaultSkin = fileptr;
			}

			if( precache ) {
				trap_S_RegisterSound( va( "sound/player/announce/%s_wins.wav", skinname), qfalse );
			}
		}

		// store one entry per model (the default/first icon)
		if (defaultSkin) {
			COM_StripExtension(defaultSkin, skinname, sizeof(skinname));
			Com_sprintf( s_playermodel.modelnames[s_playermodel.nummodels++],
				sizeof( s_playermodel.modelnames[s_playermodel.nummodels] ),
				"models/players/%s/%s", dirptr, skinname );
		}
	}

	s_playermodel.numpages = s_playermodel.nummodels/MAX_MODELSPERPAGE;
	if (s_playermodel.nummodels % MAX_MODELSPERPAGE)
		s_playermodel.numpages++;
}


/*
=================
PlayerModel_SetMenuItems
=================
*/
static void PlayerModel_SetMenuItems( void )
{
	int				i;
	char			modelDir[64];
	char			cvarModelDir[64];
	const char		*cvarSkin;
	const char		*testSkin;

	// name
	trap_Cvar_VariableStringBuffer( "name", s_playername, 16 );
	Q_CleanStr( s_playername );

	// model
	trap_Cvar_VariableStringBuffer( "model", s_playermodel.modelskin, 64 );

	// use default skin if none is set
	if (!strchr(s_playermodel.modelskin, '/')) {
		Q_strcat(s_playermodel.modelskin, 64, "/default");
	}

	// extract cvar model directory and skin name
	{
		char *slash = strchr(s_playermodel.modelskin, '/');
		if (slash) {
			int len = slash - s_playermodel.modelskin;
			if (len >= (int)sizeof(cvarModelDir)) len = sizeof(cvarModelDir) - 1;
			Q_strncpyz(cvarModelDir, s_playermodel.modelskin, len + 1);
			cvarSkin = slash + 1;
		} else {
			Q_strncpyz(cvarModelDir, s_playermodel.modelskin, sizeof(cvarModelDir));
			cvarSkin = "default";
		}
	}

	// find model in our list
	s_playermodel.selectedmodel = 0;
	for (i=0; i<s_playermodel.nummodels; i++)
	{
		PlayerModel_GetModelDir(s_playermodel.modelnames[i], modelDir, sizeof(modelDir));
		if (!Q_stricmp(modelDir, cvarModelDir))
		{
			s_playermodel.selectedmodel = i;
			s_playermodel.modelpage = i / MAX_MODELSPERPAGE;
			break;
		}
	}

	// load skins for selected model
	PlayerModel_LoadSkins(s_playermodel.selectedmodel);

	// find matching skin
	s_playermodel.selectedskin = 0;
	for (i=0; i<s_playermodel.numskins; i++)
	{
		testSkin = PlayerModel_GetSkinFromIcon(s_playermodel.skinnames[i]);
		if (!Q_stricmp(testSkin, cvarSkin))
		{
			s_playermodel.selectedskin = i;
			s_playermodel.skinpage = i / MAX_SKINSPERPAGE;
			break;
		}
	}

	// set display strings
	PlayerModel_SetModelskin();
}


/*
=================
PlayerModel_MenuInit
=================
*/
static void PlayerModel_MenuInit( void )
{
	int			i;
	int			j;
	int			k;
	int			x;
	int			y;

	// zero set all our globals
	memset( &s_playermodel, 0 ,sizeof(playermodel_t) );
	s_playermodel.currentAnim = 0;	// STAND by default

	PlayerModel_Cache();

	s_playermodel.menu.key        = PlayerModel_MenuKey;
	s_playermodel.menu.wrapAround = qtrue;
	s_playermodel.menu.fullscreen = qtrue;

	s_playermodel.banner.generic.type  = MTYPE_BTEXT;
	s_playermodel.banner.generic.x     = 320;
	s_playermodel.banner.generic.y     = 16;
	s_playermodel.banner.string        = "PLAYER MODEL";
	s_playermodel.banner.color         = color_white;
	s_playermodel.banner.style         = UI_CENTER;

	s_playermodel.framel.generic.type  = MTYPE_BITMAP;
	s_playermodel.framel.generic.name  = MODEL_FRAMEL;
	s_playermodel.framel.generic.flags = QMF_LEFT_JUSTIFY|QMF_INACTIVE;
	s_playermodel.framel.generic.x     = 0;
	s_playermodel.framel.generic.y     = 78;
	s_playermodel.framel.width         = 256;
	s_playermodel.framel.height        = 329;

	s_playermodel.framer.generic.type  = MTYPE_BITMAP;
	s_playermodel.framer.generic.name  = MODEL_FRAMER;
	s_playermodel.framer.generic.flags = QMF_LEFT_JUSTIFY|QMF_INACTIVE;
	s_playermodel.framer.generic.x     = 376;
	s_playermodel.framer.generic.y     = 76;
	s_playermodel.framer.width         = 256;
	s_playermodel.framer.height        = 334;

	s_playermodel.ports.generic.type  = MTYPE_BITMAP;
	s_playermodel.ports.generic.name  = MODEL_PORTS;
	s_playermodel.ports.generic.flags = QMF_LEFT_JUSTIFY|QMF_INACTIVE;
	s_playermodel.ports.generic.x     = 50;
	s_playermodel.ports.generic.y     = 59;
	s_playermodel.ports.width         = 274;
	s_playermodel.ports.height        = 274;

	// model grid: 4 columns × 3 rows
	y =	59;
	for (i=0,k=0; i<PLAYERGRID_ROWS; i++)
	{
		x =	50;
		for (j=0; j<PLAYERGRID_COLS; j++,k++)
		{
			s_playermodel.pics[k].generic.type	   = MTYPE_BITMAP;
			s_playermodel.pics[k].generic.flags    = QMF_LEFT_JUSTIFY|QMF_INACTIVE;
			s_playermodel.pics[k].generic.x		   = x;
			s_playermodel.pics[k].generic.y		   = y;
			s_playermodel.pics[k].width  		   = 64;
			s_playermodel.pics[k].height  		   = 64;
			s_playermodel.pics[k].focuspic         = MODEL_SELECTED;
			s_playermodel.pics[k].focuscolor       = colorRed;

			s_playermodel.picbuttons[k].generic.type	 = MTYPE_BITMAP;
			s_playermodel.picbuttons[k].generic.flags    = QMF_LEFT_JUSTIFY|QMF_NODEFAULTINIT|QMF_PULSEIFFOCUS;
			s_playermodel.picbuttons[k].generic.id	     = ID_PLAYERPIC0+k;
			s_playermodel.picbuttons[k].generic.callback = PlayerModel_PicEvent;
			s_playermodel.picbuttons[k].generic.x    	 = x - 16;
			s_playermodel.picbuttons[k].generic.y		 = y - 16;
			s_playermodel.picbuttons[k].generic.left	 = x;
			s_playermodel.picbuttons[k].generic.top		 = y;
			s_playermodel.picbuttons[k].generic.right	 = x + 64;
			s_playermodel.picbuttons[k].generic.bottom   = y + 64;
			s_playermodel.picbuttons[k].width  		     = 128;
			s_playermodel.picbuttons[k].height  		 = 128;
			s_playermodel.picbuttons[k].focuspic  		 = MODEL_SELECT;
			s_playermodel.picbuttons[k].focuscolor  	 = colorRed;

			x += 64+6;
		}
		y += 64+6;
	}

	// skin strip: 4 icons in a row below the model grid
	// positioned at the same column x values, at SKINROW_Y
	x = 50;
	y = SKINROW_Y;
	for (i=0; i<MAX_SKINSPERPAGE; i++)
	{
		s_playermodel.skins[i].generic.type      = MTYPE_BITMAP;
		s_playermodel.skins[i].generic.flags     = QMF_LEFT_JUSTIFY|QMF_NODEFAULTINIT|QMF_PULSEIFFOCUS;
		s_playermodel.skins[i].generic.id        = ID_SKINPIC0 + i;
		s_playermodel.skins[i].generic.callback  = PlayerModel_SkinPicEvent;
		s_playermodel.skins[i].generic.x         = x;
		s_playermodel.skins[i].generic.y         = y;
		s_playermodel.skins[i].generic.left      = x;
		s_playermodel.skins[i].generic.top       = y;
		s_playermodel.skins[i].generic.right     = x + 64;
		s_playermodel.skins[i].generic.bottom    = y + 64;
		s_playermodel.skins[i].width             = 64;
		s_playermodel.skins[i].height            = 64;
		s_playermodel.skins[i].focuspic          = MODEL_SELECTED;
		s_playermodel.skins[i].focuscolor        = colorRed;

		x += 64+6;
	}

	s_playermodel.playername.generic.type  = MTYPE_PTEXT;
	s_playermodel.playername.generic.flags = QMF_CENTER_JUSTIFY|QMF_INACTIVE;
	s_playermodel.playername.generic.x	   = 320;
	s_playermodel.playername.generic.y	   = 440;
	s_playermodel.playername.string	       = s_playername;
	s_playermodel.playername.style		   = UI_CENTER;
	s_playermodel.playername.color         = text_color_normal;

	s_playermodel.modelname.generic.type  = MTYPE_PTEXT;
	s_playermodel.modelname.generic.flags = QMF_CENTER_JUSTIFY|QMF_INACTIVE;
	s_playermodel.modelname.generic.x	  = 497;
	s_playermodel.modelname.generic.y	  = 54;
	s_playermodel.modelname.string	      = s_modelname;
	s_playermodel.modelname.style		  = UI_CENTER;
	s_playermodel.modelname.color         = text_color_normal;

	s_playermodel.skinname.generic.type   = MTYPE_PTEXT;
	s_playermodel.skinname.generic.flags  = QMF_CENTER_JUSTIFY|QMF_INACTIVE;
	s_playermodel.skinname.generic.x	  = 497;
	s_playermodel.skinname.generic.y	  = 394;
	s_playermodel.skinname.string	      = s_skinname;
	s_playermodel.skinname.style		  = UI_CENTER;
	s_playermodel.skinname.color          = text_color_normal;

	s_playermodel.player.generic.type      = MTYPE_BITMAP;
	s_playermodel.player.generic.flags     = QMF_INACTIVE;
	s_playermodel.player.generic.ownerdraw = PlayerModel_DrawPlayer;
	s_playermodel.player.generic.x	       = 400;
	s_playermodel.player.generic.y	       = -40;
	s_playermodel.player.width	           = 32*10;
	s_playermodel.player.height            = 56*10;

	// model page arrows (below skin strip)
	s_playermodel.arrows.generic.type		= MTYPE_BITMAP;
	s_playermodel.arrows.generic.name		= MODEL_ARROWS;
	s_playermodel.arrows.generic.flags		= QMF_INACTIVE;
	s_playermodel.arrows.generic.x			= 125;
	s_playermodel.arrows.generic.y			= 345;
	s_playermodel.arrows.width				= 128;
	s_playermodel.arrows.height				= 32;

	s_playermodel.left.generic.type			= MTYPE_BITMAP;
	s_playermodel.left.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_playermodel.left.generic.callback		= PlayerModel_MenuEvent;
	s_playermodel.left.generic.id			= ID_PREVPAGE;
	s_playermodel.left.generic.x			= 125;
	s_playermodel.left.generic.y			= 345;
	s_playermodel.left.width  				= 64;
	s_playermodel.left.height  				= 32;
	s_playermodel.left.focuspic				= MODEL_ARROWSL;

	s_playermodel.right.generic.type	    = MTYPE_BITMAP;
	s_playermodel.right.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_playermodel.right.generic.callback	= PlayerModel_MenuEvent;
	s_playermodel.right.generic.id			= ID_NEXTPAGE;
	s_playermodel.right.generic.x			= 125+61;
	s_playermodel.right.generic.y			= 345;
	s_playermodel.right.width  				= 64;
	s_playermodel.right.height  		    = 32;
	s_playermodel.right.focuspic			= MODEL_ARROWSR;

	s_playermodel.back.generic.type	    = MTYPE_BITMAP;
	s_playermodel.back.generic.name     = MODEL_BACK0;
	s_playermodel.back.generic.flags    = QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_playermodel.back.generic.callback = PlayerModel_MenuEvent;
	s_playermodel.back.generic.id	    = ID_BACK;
	s_playermodel.back.generic.x		= 0;
	s_playermodel.back.generic.y		= 480-64;
	s_playermodel.back.width  		    = 128;
	s_playermodel.back.height  		    = 64;
	s_playermodel.back.focuspic         = MODEL_BACK1;

	// animation preview buttons along the bottom
	{
		int animX = 385;
		int animY = 430;
		int animSpacing = 50;

		for (i=0; i<NUM_ANIM_BUTTONS; i++)
		{
			Q_strncpyz(s_animLabels[i], s_animDefs[i].label, sizeof(s_animLabels[i]));

			s_playermodel.animItems[i].generic.type   = MTYPE_PTEXT;
			s_playermodel.animItems[i].generic.flags  = QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS|QMF_SMALLFONT;
			s_playermodel.animItems[i].generic.x      = animX + i * animSpacing;
			s_playermodel.animItems[i].generic.y      = animY;
			s_playermodel.animItems[i].generic.id     = ID_ANIM0 + i;
			s_playermodel.animItems[i].generic.callback = PlayerModel_AnimEvent;
			s_playermodel.animItems[i].string         = s_animLabels[i];
			s_playermodel.animItems[i].style          = UI_CENTER|UI_SMALLFONT;
			s_playermodel.animItems[i].color          = text_color_normal;
		}
	}

	// --- add items to menu ---
	Menu_AddItem( &s_playermodel.menu,	&s_playermodel.banner );
	Menu_AddItem( &s_playermodel.menu,	&s_playermodel.framel );
	Menu_AddItem( &s_playermodel.menu,	&s_playermodel.framer );
	Menu_AddItem( &s_playermodel.menu,	&s_playermodel.ports );
	Menu_AddItem( &s_playermodel.menu,	&s_playermodel.playername );
	Menu_AddItem( &s_playermodel.menu,	&s_playermodel.modelname );
	Menu_AddItem( &s_playermodel.menu,	&s_playermodel.skinname );

	for (i=0; i<MAX_MODELSPERPAGE; i++)
	{
		Menu_AddItem( &s_playermodel.menu,	&s_playermodel.pics[i] );
		Menu_AddItem( &s_playermodel.menu,	&s_playermodel.picbuttons[i] );
	}

	for (i=0; i<MAX_SKINSPERPAGE; i++)
	{
		Menu_AddItem( &s_playermodel.menu,	&s_playermodel.skins[i] );
	}

	Menu_AddItem( &s_playermodel.menu,	&s_playermodel.player );
	Menu_AddItem( &s_playermodel.menu,	&s_playermodel.arrows );
	Menu_AddItem( &s_playermodel.menu,	&s_playermodel.left );
	Menu_AddItem( &s_playermodel.menu,	&s_playermodel.right );
	Menu_AddItem( &s_playermodel.menu,	&s_playermodel.back );

	for (i=0; i<NUM_ANIM_BUTTONS; i++)
	{
		Menu_AddItem( &s_playermodel.menu,	&s_playermodel.animItems[i] );
	}

	// set initial states
	PlayerModel_SetMenuItems();

	// update user interface
	PlayerModel_UpdateGrid();
	PlayerModel_UpdateSkinGrid();
	PlayerModel_UpdateModel();
}


/*
=================
PlayerModel_Cache
=================
*/
void PlayerModel_Cache( void )
{
	int	i;

	for( i = 0; playermodel_artlist[i]; i++ ) {
		trap_R_RegisterShaderNoMip( playermodel_artlist[i] );
	}

	PlayerModel_BuildList();
	for( i = 0; i < s_playermodel.nummodels; i++ ) {
		trap_R_RegisterShaderNoMip( s_playermodel.modelnames[i] );
	}
}

void UI_PlayerModelMenu(void)
{
	PlayerModel_MenuInit();

	UI_PushMenu( &s_playermodel.menu );

	Menu_SetCursorToItem( &s_playermodel.menu, &s_playermodel.picbuttons[s_playermodel.selectedmodel % MAX_MODELSPERPAGE] );
}
