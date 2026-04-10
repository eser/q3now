//===========================================================================
//
// Name:			fw_items.c
// Function:		file to start with for fuzzy logic interbreeding sessions
// Programmer:		Mr Elusive
// Tab Size:		4 (real tabs)
// Notes:			-
//===========================================================================


//#define WEAPONS_STAY

//balance range
#define BR_ARMOR				30
#define BR_HEALTH				30
#define BR_WEAPON				30
#define BR_AMMO					30
#define BR_POWERUP				30

//balance macros
#define MZ(value)				(value) < 0 ? 0 : (value)
#define ARMOR_SCALE(v)			balance($evalfloat(MZ(FS_ARMOR*v)), $evalfloat(MZ(FS_ARMOR*v-BR_ARMOR)), $evalfloat(MZ(FS_ARMOR*v+BR_ARMOR)))
#define HEALTH_SCALE(v)			balance($evalfloat(MZ(FS_HEALTH*v)), $evalfloat(MZ(FS_HEALTH*v-BR_HEALTH)), $evalfloat(MZ(FS_HEALTH*v+BR_HEALTH)))
#define WEAPON_SCALE(v)			balance($evalfloat(MZ(v)), $evalfloat(MZ(v-BR_WEAPON)), $evalfloat(MZ(v+BR_WEAPON)))
#define AMMO_SCALE(v)			balance($evalfloat(MZ(v)), $evalfloat(MZ(v-BR_AMMO)), $evalfloat(MZ(v+BR_AMMO)))
#define POWERUP_SCALE(v)		balance($evalfloat(MZ(v)), $evalfloat(MZ(v-BR_POWERUP)), $evalfloat(MZ(v+BR_POWERUP)))

#ifndef W_NAILGUN
#define W_NAILGUN			40
#define GWW_NAILGUN			40
#endif
#ifndef W_PROXLAUNCHER
#define W_PROXLAUNCHER		40
#define GWW_PROXLAUNCHER	40
#endif
#ifndef W_CHAINGUN
#define W_CHAINGUN			40
#define GWW_CHAINGUN		40
#endif
#ifndef W_KAMIKAZE
#define W_KAMIKAZE			200
#endif
#ifndef W_INVULNERABILITY
#define W_INVULNERABILITY	100
#endif
#ifndef W_PORTAL
#define W_PORTAL			40
#endif
#ifndef W_SCOUT
#define W_SCOUT				40
#endif
#ifndef W_GUARD
#define W_GUARD				40
#endif
#ifndef W_DOUBLER
#define W_DOUBLER			40
#endif
#ifndef W_AMMOREGEN
#define W_AMMOREGEN			40
#endif
#ifndef W_REDCUBE
#define W_REDCUBE			100
#endif
#ifndef W_BLUECUBE
#define W_BLUECUBE			100
#endif

//=============================================
// ARMOR
//=============================================

weight "item_armor_shard"
{
	switch(INVENTORY_SCOUT)
	{
		case 1:
		{
			switch(INVENTORY_ARMOR)
			{
				case 10: return ARMOR_SCALE(20);
				case 20: return ARMOR_SCALE(20);
				case 30: return ARMOR_SCALE(20);
				case 40: return ARMOR_SCALE(20);
				case 50: return ARMOR_SCALE(20);
				case 60: return ARMOR_SCALE(15);
				case 70: return ARMOR_SCALE(15);
				case 80: return ARMOR_SCALE(15);
				case 90: return ARMOR_SCALE(15);
				case 100: return ARMOR_SCALE(15);
				case 110: return ARMOR_SCALE(10);
				case 120: return ARMOR_SCALE(10);
				case 130: return ARMOR_SCALE(10);
				case 140: return ARMOR_SCALE(10);
				case 150: return ARMOR_SCALE(10);
				case 160: return ARMOR_SCALE(10);
				case 170: return ARMOR_SCALE(10);
				case 180: return ARMOR_SCALE(10);
				case 190: return ARMOR_SCALE(10);
				case 200: return ARMOR_SCALE(10);
				default: return ARMOR_SCALE(1);
			} //end switch
		} //end case
		default: return 0;
	} //end switch
} //end weight

weight "item_armor_combat"
{
	switch(INVENTORY_SCOUT)
	{
		case 1:
		{
			switch(INVENTORY_ARMOR)
			{
				case 10: return ARMOR_SCALE(60);
				case 20: return ARMOR_SCALE(60);
				case 30: return ARMOR_SCALE(60);
				case 40: return ARMOR_SCALE(60);
				case 50: return ARMOR_SCALE(60);
				case 60: return ARMOR_SCALE(50);
				case 70: return ARMOR_SCALE(50);
				case 80: return ARMOR_SCALE(50);
				case 90: return ARMOR_SCALE(50);
				case 100: return ARMOR_SCALE(50);
				case 110: return ARMOR_SCALE(40);
				case 120: return ARMOR_SCALE(40);
				case 130: return ARMOR_SCALE(40);
				case 140: return ARMOR_SCALE(40);
				case 150: return ARMOR_SCALE(40);
				case 160: return ARMOR_SCALE(40);
				case 170: return ARMOR_SCALE(40);
				case 180: return ARMOR_SCALE(40);
				case 190: return ARMOR_SCALE(40);
				case 200: return ARMOR_SCALE(40);
				default: return ARMOR_SCALE(1);
			} //end switch
		} //end case
		default: return 0;
	} //end switch
} //end weight

weight "item_armor_body"
{
	switch(INVENTORY_SCOUT)
	{
		case 1:
		{
			switch(INVENTORY_ARMOR)
			{
				case 10: return ARMOR_SCALE(90);
				case 20: return ARMOR_SCALE(90);
				case 30: return ARMOR_SCALE(90);
				case 40: return ARMOR_SCALE(90);
				case 50: return ARMOR_SCALE(90);
				case 60: return ARMOR_SCALE(50);
				case 70: return ARMOR_SCALE(50);
				case 80: return ARMOR_SCALE(50);
				case 90: return ARMOR_SCALE(50);
				case 100: return ARMOR_SCALE(50);
				case 110: return ARMOR_SCALE(20);
				case 120: return ARMOR_SCALE(20);
				case 130: return ARMOR_SCALE(20);
				case 140: return ARMOR_SCALE(20);
				case 150: return ARMOR_SCALE(20);
				case 160: return ARMOR_SCALE(20);
				case 170: return ARMOR_SCALE(20);
				case 180: return ARMOR_SCALE(20);
				case 190: return ARMOR_SCALE(20);
				case 200: return ARMOR_SCALE(20);
				default: return ARMOR_SCALE(1);
			} //end switch
		} //end case
		default: return 0;
	} //end switch
} //end weight

//=============================================
// HEALTH
//=============================================

weight "item_health_small"
{
	switch(INVENTORY_HEALTH)
	{
		case 10: return HEALTH_SCALE(80);
		case 20: return HEALTH_SCALE(80);
		case 30: return HEALTH_SCALE(80);
		case 40: return HEALTH_SCALE(80);
		case 50: return HEALTH_SCALE(70);
		case 60: return HEALTH_SCALE(70);
		case 70: return HEALTH_SCALE(60);
		case 80: return HEALTH_SCALE(60);
		case 90: return HEALTH_SCALE(50);
		case 100: return HEALTH_SCALE(50);
		default: return 0;
	} //end switch
} //end weight

weight "item_health"
{
	switch(INVENTORY_HEALTH)
	{
		case 10: return HEALTH_SCALE(90);
		case 20: return HEALTH_SCALE(90);
		case 30: return HEALTH_SCALE(90);
		case 40: return HEALTH_SCALE(90);
		case 50: return HEALTH_SCALE(80);
		case 60: return HEALTH_SCALE(80);
		case 70: return HEALTH_SCALE(60);
		case 80: return HEALTH_SCALE(60);
		case 90: return HEALTH_SCALE(50);
		case 100: return HEALTH_SCALE(50);
		default: return 0;
	} //end switch
} //end weight

weight "item_health_large"
{
	switch(INVENTORY_HEALTH)
	{
		case 10: return HEALTH_SCALE(100);
		case 20: return HEALTH_SCALE(100);
		case 30: return HEALTH_SCALE(100);
		case 40: return HEALTH_SCALE(100);
		case 50: return HEALTH_SCALE(90);
		case 60: return HEALTH_SCALE(90);
		case 70: return HEALTH_SCALE(80);
		case 80: return HEALTH_SCALE(80);
		case 90: return HEALTH_SCALE(50);
		case 100: return HEALTH_SCALE(50);
		default: return 0;
	} //end switch
} //end weight

weight "item_health_mega"
{
	switch(INVENTORY_HEALTH)
	{
		case 10: return HEALTH_SCALE(110);
		case 20: return HEALTH_SCALE(110);
		case 30: return HEALTH_SCALE(110);
		case 40: return HEALTH_SCALE(110);
		case 50: return HEALTH_SCALE(100);
		case 60: return HEALTH_SCALE(100);
		case 70: return HEALTH_SCALE(90);
		case 80: return HEALTH_SCALE(90);
		case 90: return HEALTH_SCALE(80);
		case 100: return HEALTH_SCALE(80);
		default: return HEALTH_SCALE(50);
	} //end switch
} //end weight

//=============================================
// WEAPONS
//=============================================

weight "weapon_shotgun"
{
	switch(INVENTORY_SHOTGUN)
	{
		case 1:
		{
			switch(INVENTORY_SHELLS)
			{
				case 10: return WEAPON_SCALE(W_SHOTGUN - 10);
				default: return WEAPON_SCALE(W_SHOTGUN);
			} //end switch
		} //end case
		default:
		{
#ifdef WEAPONS_STAY
			switch(INVENTORY_SHELLS)
			{
				case 10: return WEAPON_SCALE(GWW_SHOTGUN);
				case 100: return WEAPON_SCALE(GWW_SHOTGUN - 10);
				default: return balance(5, 3, 7);
			} //end switch
#else
			return 1;
#endif			
		} //end default
	} //end switch
} //end weight

weight "weapon_machinegun"
{
	switch(INVENTORY_MACHINEGUN)
	{
		case 1:
		{
			switch(INVENTORY_BULLETS)
			{
				case 40: return WEAPON_SCALE(W_MACHINEGUN - 10);
				default: return WEAPON_SCALE(W_MACHINEGUN);
			} //end switch
		} //end case
		default:
		{
#ifdef WEAPONS_STAY
			switch(INVENTORY_BULLETS)
			{
				case 50: return WEAPON_SCALE(GWW_MACHINEGUN);
				case 200: return WEAPON_SCALE(GWW_MACHINEGUN - 10);
				default: return balance(5, 3, 7);
			} //end switch
#else
			return 1;
#endif			
		} //end default
	} //switch
} //end weight

weight "weapon_grenadelauncher"
{
	switch(INVENTORY_GRENADELAUNCHER)
	{
		case 1: return WEAPON_SCALE(W_GRENADELAUNCHER);
		default:
		{
#ifdef WEAPONS_STAY
			switch(INVENTORY_GRENADES)
			{
				case 16: return WEAPON_SCALE(GWW_GRENADELAUNCHER);
				case 50: return WEAPON_SCALE(GWW_GRENADELAUNCHER - 10);
				default: return balance(5, 3, 7);
			} //end switch
#else
			return 1;
#endif			
		} //end default
	} //end switch
} //end weight

weight "weapon_rocketlauncher"
{
	switch(INVENTORY_ROCKETLAUNCHER)
	{
		case 1: return WEAPON_SCALE(W_ROCKETLAUNCHER);
		default:
		{
#ifdef WEAPONS_STAY
			switch(INVENTORY_ROCKETS)
			{
				case 16: return WEAPON_SCALE(GWW_ROCKETLAUNCHER);
				case 50: return WEAPON_SCALE(GWW_ROCKETLAUNCHER - 10);
				default: return balance(5, 3, 7);
			} //end switch
#else
			return 1;
#endif			
		} //end default
	} //end switch
} //end weight

weight "weapon_lightning"
{
	switch(INVENTORY_LIGHTNING)
	{
		case 1: return WEAPON_SCALE(W_LIGHTNING);
		default:
		{
#ifdef WEAPONS_STAY
			switch(INVENTORY_LIGHTNINGAMMO)
			{
				case 16: return WEAPON_SCALE(GWW_LIGHTNING);
				case 50: return WEAPON_SCALE(GWW_LIGHTNING - 10);
				default: return balance(5, 3, 7);
			} //end switch
#else
			return 1;
#endif			
		} //end default
	} //end switch
} //end weight

weight "weapon_railgun"
{
	switch(INVENTORY_RAILGUN)
	{
		case 1: return WEAPON_SCALE(W_RAILGUN);
		default:
		{
#ifdef WEAPONS_STAY
			switch(INVENTORY_SLUGS)
			{
				case 16: return WEAPON_SCALE(GWW_RAILGUN);
				case 50: return WEAPON_SCALE(GWW_RAILGUN - 10);
				default: return balance(5, 3, 7);
			} //end switch
#else
			return 1;
#endif			
		} //end default
	} //end switch
} //end weight

weight "weapon_plasmagun"
{
	switch(INVENTORY_PLASMAGUN)
	{
		case 1: return WEAPON_SCALE(W_PLASMAGUN);
		default:
		{
#ifdef WEAPONS_STAY
			switch(INVENTORY_CELLS)
			{
				case 16: return WEAPON_SCALE(GWW_PLASMAGUN);
				case 50: return WEAPON_SCALE(GWW_PLASMAGUN - 10);
				default: return balance(5, 3, 7);
			} //end switch
#else
			return 1;
#endif			
		} //end default
	} //end switch
} //end weight

weight "weapon_bfg"
{
	switch(INVENTORY_BFG10K)
	{
		case 1: return WEAPON_SCALE(W_BFG10K);
		default:
		{
#ifdef WEAPONS_STAY
			switch(INVENTORY_BFGAMMO)
			{
				case 16: return WEAPON_SCALE(GWW_BFG10K);
				case 200: return WEAPON_SCALE(GWW_BFG10K - 10);
				default: return balance(5, 3, 7);
			} //end switch
#else
			return 1;
#endif			
		} //end default
	} //end switch
} //end weight

weight "weapon_nailgun"
{
	switch(INVENTORY_NAILGUN)
	{
		case 1: return WEAPON_SCALE(W_NAILGUN);
		default:
		{
#ifdef WEAPONS_STAY
			switch(INVENTORY_NAILS)
			{
				case 16: return WEAPON_SCALE(GWW_NAILGUN);
				case 200: return WEAPON_SCALE(GWW_NAILGUN - 10);
				default: return balance(5, 3, 7);
			} //end switch
#else
			return 1;
#endif			
		} //end default
	} //end switch
} //end weight

weight "weapon_prox_launcher"
{
	switch(INVENTORY_PROXLAUNCHER)
	{
	case 1: return WEAPON_SCALE(W_PROXLAUNCHER);
		default:
		{
#ifdef WEAPONS_STAY
			switch(INVENTORY_MINES)
			{
				case 16: return WEAPON_SCALE(GWW_PROXLAUNCHER);
				case 200: return WEAPON_SCALE(GWW_PROXLAUNCHER - 10);
				default: return balance(5, 3, 7);
			} //end switch
#else
			return 1;
#endif			
		} //end default
	} //end switch
} //end weight

weight "weapon_chaingun"
{
	switch(INVENTORY_CHAINGUN)
	{
		case 1: return WEAPON_SCALE(W_CHAINGUN);
		default:
		{
#ifdef WEAPONS_STAY
			switch(INVENTORY_BELT)
			{
				case 16: return WEAPON_SCALE(GWW_CHAINGUN);
				case 200: return WEAPON_SCALE(GWW_CHAINGUN - 10);
				default: return balance(5, 3, 7);
			} //end switch
#else
			return 1;
#endif			
		} //end default
	} //end switch
} //end weight

//=============================================
// AMMO
//=============================================

weight "ammo_shells"
{
	switch(INVENTORY_SHELLS)
	{
		case 20: return AMMO_SCALE(20);
		case 40: return AMMO_SCALE(20);
		case 60: return AMMO_SCALE(20);
		case 80: return AMMO_SCALE(20);
		case 100: return AMMO_SCALE(20);
		case 120: return AMMO_SCALE(20);
		case 140: return AMMO_SCALE(20);
		case 160: return AMMO_SCALE(20);
		case 180: return AMMO_SCALE(20);
		case 200: return AMMO_SCALE(20);
		default: return 0;
	} //end switch
} //end weight

weight "ammo_bullets"
{
	switch(INVENTORY_BULLETS)
	{
		case 20: return AMMO_SCALE(20);
		case 40: return AMMO_SCALE(20);
		case 60: return AMMO_SCALE(20);
		case 80: return AMMO_SCALE(20);
		case 100: return AMMO_SCALE(20);
		case 120: return AMMO_SCALE(20);
		case 140: return AMMO_SCALE(20);
		case 160: return AMMO_SCALE(20);
		case 180: return AMMO_SCALE(20);
		case 200: return AMMO_SCALE(20);
		default: return 0;
	} //end switch
} //end weight

weight "ammo_grenades"
{
	switch(INVENTORY_GRENADES)
	{
		case 20: return AMMO_SCALE(20);
		case 40: return AMMO_SCALE(20);
		case 60: return AMMO_SCALE(20);
		case 80: return AMMO_SCALE(20);
		case 100: return AMMO_SCALE(20);
		case 120: return AMMO_SCALE(20);
		case 140: return AMMO_SCALE(20);
		case 160: return AMMO_SCALE(20);
		case 180: return AMMO_SCALE(20);
		case 200: return AMMO_SCALE(20);
		default: return 0;
	} //end switch
} //end weight

weight "ammo_cells"
{
	switch(INVENTORY_CELLS)
	{
		case 20: return AMMO_SCALE(20);
		case 40: return AMMO_SCALE(20);
		case 60: return AMMO_SCALE(20);
		case 80: return AMMO_SCALE(20);
		case 100: return AMMO_SCALE(20);
		case 120: return AMMO_SCALE(20);
		case 140: return AMMO_SCALE(20);
		case 160: return AMMO_SCALE(20);
		case 180: return AMMO_SCALE(20);
		case 200: return AMMO_SCALE(20);
		default: return 0;
	} //end switch
} //end weight

weight "ammo_lightning"
{
	switch(INVENTORY_LIGHTNINGAMMO)
	{
		case 20: return AMMO_SCALE(20);
		case 40: return AMMO_SCALE(20);
		case 60: return AMMO_SCALE(20);
		case 80: return AMMO_SCALE(20);
		case 100: return AMMO_SCALE(20);
		case 120: return AMMO_SCALE(20);
		case 140: return AMMO_SCALE(20);
		case 160: return AMMO_SCALE(20);
		case 180: return AMMO_SCALE(20);
		case 200: return AMMO_SCALE(20);
		default: return 0;
	} //end switch
} //end weight

weight "ammo_rockets"
{
	switch(INVENTORY_ROCKETS)
	{
		case 20: return AMMO_SCALE(20);
		case 40: return AMMO_SCALE(20);
		case 60: return AMMO_SCALE(20);
		case 80: return AMMO_SCALE(20);
		case 100: return AMMO_SCALE(20);
		case 120: return AMMO_SCALE(20);
		case 140: return AMMO_SCALE(20);
		case 160: return AMMO_SCALE(20);
		case 180: return AMMO_SCALE(20);
		case 200: return AMMO_SCALE(20);
		default: return 0;
	} //end switch
} //end weight

weight "ammo_slugs"
{
	switch(INVENTORY_SLUGS)
	{
		case 20: return AMMO_SCALE(20);
		case 40: return AMMO_SCALE(20);
		case 60: return AMMO_SCALE(20);
		case 80: return AMMO_SCALE(20);
		case 100: return AMMO_SCALE(20);
		case 120: return AMMO_SCALE(20);
		case 140: return AMMO_SCALE(20);
		case 160: return AMMO_SCALE(20);
		case 180: return AMMO_SCALE(20);
		case 200: return AMMO_SCALE(20);
		default: return 0;
	} //end switch
} //end weight

weight "ammo_bfg"
{
	switch(INVENTORY_BFGAMMO)
	{
		case 20: return AMMO_SCALE(20);
		case 40: return AMMO_SCALE(20);
		case 60: return AMMO_SCALE(20);
		case 80: return AMMO_SCALE(20);
		case 100: return AMMO_SCALE(20);
		case 120: return AMMO_SCALE(20);
		case 140: return AMMO_SCALE(20);
		case 160: return AMMO_SCALE(20);
		case 180: return AMMO_SCALE(20);
		case 200: return AMMO_SCALE(20);
		default: return 0;
	} //end switch
} //end weight

weight "ammo_nails"
{
	switch(INVENTORY_NAILS)
	{
		case 20: return AMMO_SCALE(20);
		case 40: return AMMO_SCALE(20);
		case 60: return AMMO_SCALE(20);
		case 80: return AMMO_SCALE(20);
		case 100: return AMMO_SCALE(20);
		case 120: return AMMO_SCALE(20);
		case 140: return AMMO_SCALE(20);
		case 160: return AMMO_SCALE(20);
		case 180: return AMMO_SCALE(20);
		case 200: return AMMO_SCALE(20);
		default: return 0;
	} //end switch
} //end weight

weight "ammo_nails"
{
	switch(INVENTORY_NAILS)
	{
		case 20: return AMMO_SCALE(20);
		case 40: return AMMO_SCALE(20);
		case 60: return AMMO_SCALE(20);
		case 80: return AMMO_SCALE(20);
		case 100: return AMMO_SCALE(20);
		case 120: return AMMO_SCALE(20);
		case 140: return AMMO_SCALE(20);
		case 160: return AMMO_SCALE(20);
		case 180: return AMMO_SCALE(20);
		case 200: return AMMO_SCALE(20);
		default: return 0;
	} //end switch
} //end weight

weight "ammo_mines"
{
	switch(INVENTORY_MINES)
	{
		case 20: return AMMO_SCALE(20);
		case 40: return AMMO_SCALE(20);
		case 60: return AMMO_SCALE(20);
		case 80: return AMMO_SCALE(20);
		case 100: return AMMO_SCALE(20);
		case 120: return AMMO_SCALE(20);
		case 140: return AMMO_SCALE(20);
		case 160: return AMMO_SCALE(20);
		case 180: return AMMO_SCALE(20);
		case 200: return AMMO_SCALE(20);
		default: return 0;
	} //end switch
} //end weight

weight "ammo_belt"
{
	switch(INVENTORY_BELT)
	{
		case 20: return AMMO_SCALE(20);
		case 40: return AMMO_SCALE(20);
		case 60: return AMMO_SCALE(20);
		case 80: return AMMO_SCALE(20);
		case 100: return AMMO_SCALE(20);
		case 120: return AMMO_SCALE(20);
		case 140: return AMMO_SCALE(20);
		case 160: return AMMO_SCALE(20);
		case 180: return AMMO_SCALE(20);
		case 200: return AMMO_SCALE(20);
		default: return 0;
	} //end switch
} //end weight


//=============================================
// POWERUPS
//=============================================

weight "holdable_teleporter"
{
	switch(INVENTORY_TELEPORTER)
	{
		case 1:
		{
			switch(INVENTORY_MEDKIT)
			{
				case 1:
				{
					switch(INVENTORY_PORTAL)
					{
						case 1:
						{
							switch(INVENTORY_KAMIKAZE)
							{
								case 1:
								{
									switch(INVENTORY_INVULNERABILITY)
									{
										case 1: return POWERUP_SCALE(W_TELEPORTER);
										default: return 0; //already has a medkit
									} //end switch
								} //end case
								default: return 0; //already has a holdable
							} //end switch
						} //end case
						default: return 0; //already has a holdable
					} //end switch
				} //end case
				default: return 0; //already has a holdable
			} //end switch
		} //end case
		default: return 0; //already has a holdable
	} //end switch
} //end weight

weight "holdable_medkit"
{
	switch(INVENTORY_TELEPORTER)
	{
		case 1:
		{
			switch(INVENTORY_MEDKIT)
			{
				case 1:
				{
					switch(INVENTORY_PORTAL)
					{
						case 1:
						{
							switch(INVENTORY_KAMIKAZE)
							{
								case 1:
								{
									switch(INVENTORY_INVULNERABILITY)
									{
										case 1: return POWERUP_SCALE(W_MEDKIT);
										default: return 0; //already has a holdable
									} //end switch
								} //end case
								default: return 0; //already has a holdable
							} //end switch
						} //end case
						default: return 0; //already has a holdable
					} //end switch
				} //end case
				default: return 0; //already has a holdable
			} //end switch
		} //end case
		default: return 0; //already has a holdable
	} //end switch
} //end weight

weight "holdable_kamikaze"
{
	switch(INVENTORY_TELEPORTER)
	{
		case 1:
		{
			switch(INVENTORY_MEDKIT)
			{
				case 1:
				{
					switch(INVENTORY_PORTAL)
					{
						case 1:
						{
							switch(INVENTORY_KAMIKAZE)
							{
								case 1:
								{
									switch(INVENTORY_INVULNERABILITY)
									{
										case 1: return POWERUP_SCALE(W_KAMIKAZE);
										default: return 0; //already has a holdable
									} //end switch
								} //end case
								default: return 0; //already has a holdable
							} //end switch
						} //end case
						default: return 0; //already has a holdable
					} //end switch
				} //end case
				default: return 0; //already has a holdable
			} //end switch
		} //end case
		default: return 0; //already has a holdable
	} //end switch
} //end weight

weight "holdable_portal"
{
	switch(INVENTORY_TELEPORTER)
	{
		case 1:
		{
			switch(INVENTORY_MEDKIT)
			{
				case 1:
				{
					switch(INVENTORY_PORTAL)
					{
						case 1:
						{
							switch(INVENTORY_KAMIKAZE)
							{
								case 1:
								{
									switch(INVENTORY_INVULNERABILITY)
									{
										case 1: return POWERUP_SCALE(W_PORTAL);
										default: return 0; //already has a holdable
									} //end switch
								} //end case
								default: return 0; //already has a holdable
							} //end switch
						} //end case
						default: return 0; //already has a holdable
					} //end switch
				} //end case
				default: return 0; //already has a holdable
			} //end switch
		} //end case
		default: return 0; //already has a holdable
	} //end switch
} //end weight

weight "holdable_invulnerability"
{
	switch(INVENTORY_TELEPORTER)
	{
		case 1:
		{
			switch(INVENTORY_MEDKIT)
			{
				case 1:
				{
					switch(INVENTORY_PORTAL)
					{
						case 1:
						{
							switch(INVENTORY_KAMIKAZE)
							{
								case 1:
								{
									switch(INVENTORY_INVULNERABILITY)
									{
										case 1: return POWERUP_SCALE(W_INVULNERABILITY);
										default: return 0; //already has a hodable
									} //end switch
								} //end case
								default: return 0; //already has a holdable
							} //end switch
						} //end case
						default: return 0; //already has a holdable
					} //end switch
				} //end case
				default: return 0; //already has a holdable
			} //end switch
		} //end case
		default: return 0; //already has a holdable
	} //end switch
} //end weight

weight "item_quad"
{
	return POWERUP_SCALE(W_QUAD);
} //end weight

weight "item_enviro"
{
	return POWERUP_SCALE(W_ENVIRO);
} //end weight

weight "item_haste"
{
	return POWERUP_SCALE(W_HASTE);
} //end weight

weight "item_invisibility"
{
	return POWERUP_SCALE(W_INVISIBILITY);
} //end weight

weight "item_regen"
{
	return POWERUP_SCALE(W_REGEN);
} //end weight

weight "item_flight"
{
	return 0;//POWERUP_SCALE(W_FLIGHT);
} //end weight

weight "item_scout"
{
	switch(INVENTORY_SCOUT)
	{
		case 1:
		{
			switch(INVENTORY_GUARD)
			{
				case 1:
				{
					switch(INVENTORY_DOUBLER)
					{
						case 1:
						{
							switch(INVENTORY_AMMOREGEN)
							{
								case 1: return POWERUP_SCALE(W_SCOUT);
								default: return 0; //already has a persistant powerup
							} //end switch
						} //end case
						default: return 0; //already has a persistant powerup
					} //end switch
				} //end case
				default: return 0; //already has a persistant powerup
			} //end switch
		} //end case
		default: return 0; //already has a persistant powerup
	} //end switch
} //end weight

weight "item_guard"
{
	switch(INVENTORY_SCOUT)
	{
		case 1:
		{
			switch(INVENTORY_GUARD)
			{
				case 1:
				{
					switch(INVENTORY_DOUBLER)
					{
						case 1:
						{
							switch(INVENTORY_AMMOREGEN)
							{
								case 1: return POWERUP_SCALE(W_GUARD);
								default: return 0; //already has a persistant powerup
							} //end switch
						} //end case
						default: return 0; //already has a persistant powerup
					} //end switch
				} //end case
				default: return 0; //already has a persistant powerup
			} //end switch
		} //end case
		default: return 0; //already has a persistant powerup
	} //end switch
} //end weight

weight "item_doubler"
{
	switch(INVENTORY_SCOUT)
	{
		case 1:
		{
			switch(INVENTORY_GUARD)
			{
				case 1:
				{
					switch(INVENTORY_DOUBLER)
					{
						case 1:
						{
							switch(INVENTORY_AMMOREGEN)
							{
								case 1: return POWERUP_SCALE(W_DOUBLER);
								default: return 0; //already has a persistant powerup
							} //end switch
						} //end case
						default: return 0; //already has a persistant powerup
					} //end switch
				} //end case
				default: return 0; //already has a persistant powerup
			} //end switch
		} //end case
		default: return 0; //already has a persistant powerup
	} //end switch
} //end weight

weight "item_ammoregen"
{
	switch(INVENTORY_SCOUT)
	{
		case 1:
		{
			switch(INVENTORY_GUARD)
			{
				case 1:
				{
					switch(INVENTORY_DOUBLER)
					{
						case 1:
						{
							switch(INVENTORY_AMMOREGEN)
							{
								case 1: return POWERUP_SCALE(W_AMMOREGEN);
								default: return 0; //already has a persistant powerup
							} //end switch
						} //end case
						default: return 0; //already has a persistant powerup
					} //end switch
				} //end case
				default: return 0; //already has a persistant powerup
			} //end switch
		} //end case
		default: return 0; //already has a persistant powerup
	} //end switch
} //end weight

//=============================================
// This is only used to pickup dropped CTF
// flags now. The logic in here makes no
// sense since the bot has specific CTF AI.
//=============================================

weight "team_CTF_redflag" //Red Flag
{
	switch(INVENTORY_REDFLAG)
	{
		case 1: //not carrying the red flag
		{
			switch(INVENTORY_BLUEFLAG)
			{
				case 1: //not carrying the blue flag
				{
					return FLAG_WEIGHT;
				} //end case
				default: //bot carrying the blue flag so go back to the base
				{
					return 200;
				} //end case
			} //end switch
		} //end case
		default: //bot carrying the red flag, so don't go back
		{
			return 0;
		} //end default
	} //end switch
} //end weight

weight "team_CTF_blueflag" //Blue Flag
{
	switch(INVENTORY_BLUEFLAG)
	{
		case 1: //not carrying the blue flag
		{
			switch(INVENTORY_REDFLAG)
			{
				case 1: //not carrying the red flag
				{
					return FLAG_WEIGHT;
				} //end case
				default: //bot is carrying the red flag so go back to the base
				{
					return 200;
				} //end case
			} //end switch
		} //end case
		default: //bot is carrying the blue flag, so don't go back
		{
			return 0;
		} //end default
	} //end switch
} //end weight

weight "team_CTF_neutralflag" //Neutral Flag
{
	return FLAG_WEIGHT;
} //end weight

weight "item_redcube" //Red Cube
{
	return W_REDCUBE;
} //end weight

weight "item_bluecube" //Blue Cube
{
	return W_BLUECUBE;
} //end weight

//=============================================
// bot roam goal
//=============================================

weight "item_botroam"
{
	return 1;
} //end weight


