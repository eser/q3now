//===========================================================================
//
// Name:			xaero_i.c
// Function:		
// Programmer:		Mr Elusive (MrElusive@idsoftware.com)
// Last update:		1999-09-08
// Tab Size:		4 (real tabs)
//===========================================================================

#include "inv.h"

//initial health/armor states
#define FS_HEALTH			1
#define FS_ARMOR			1

//initial weapon weights
#define W_SHOTGUN				120
#define W_MACHINEGUN			70
#define W_GRENADELAUNCHER		40
#define W_ROCKETLAUNCHER		285
#define W_RAILGUN				185
#define W_BFG10K				200
#define W_LIGHTNING				180
#define W_PLASMAGUN				240
#define W_NAILGUN				140
#define W_PROXLAUNCHER			100
#define W_CHAINGUN				140

//the bot has the weapons, so the weights change a little bit
#define GWW_SHOTGUN				135
#define GWW_MACHINEGUN			50
#define GWW_GRENADELAUNCHER		30
#define GWW_ROCKETLAUNCHER		180
#define GWW_RAILGUN				125
#define GWW_BFG10K				181
#define GWW_LIGHTNING			80
#define GWW_PLASMAGUN			100
#define GWW_NAILGUN				70
#define GWW_PROXLAUNCHER		100
#define GWW_CHAINGUN			140

//initial powerup weights
#define W_TELEPORTER			40
#define W_MEDKIT				40
#define W_QUAD					80
#define W_ENVIRO				40
#define W_HASTE					40
#define W_INVISIBILITY			80
#define W_REGEN					140
#define W_FLIGHT				40
#define W_KAMIKAZE				200
#define W_INVULNERABILITY		100
#define W_PORTAL				40
#define W_SCOUT					170
#define W_GUARD					40
#define W_DOUBLER				160
#define W_AMMOREGEN				140

#define W_REDCUBE				250
#define W_BLUECUBE				250


//flag weight
#define FLAG_WEIGHT				250

//
#include "fw_items.c"

