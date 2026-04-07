#ifndef CL_WIRED_HUD_PRIVATE_H
#define CL_WIRED_HUD_PRIVATE_H

// Client-side version: no cg_local.h dependency
// Types from q_shared.h and bg_public.h are sufficient
#include "../../../qcommon/q_shared.h"
#include "../../../game/bg_public.h"

#ifdef __cplusplus
extern "C" {
#endif

// Modern text rendering types
#define OSP_TEXT_CMD_MAX 2048

/* text_command_t removed — bitmap text compiler dead after MSDF migration */

#ifndef FS_INVALID_HANDLE
#define FS_INVALID_HANDLE 0
#endif

/* generic weapon buffer size — large enough for any game configuration.
   Client does not know how many weapons exist; it uses this as a buffer. */
#define WIRED_WEAPON_BUFFER_SIZE 32

// q3now compatibility macros
#ifndef Vector4Clear
#define Vector4Clear(a)  ((a)[0]=(a)[1]=(a)[2]=(a)[3]=0)
#endif
#ifndef Vector4Subtract
#define Vector4Subtract(a,b,c) ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2],(c)[3]=(a)[3]-(b)[3])
#endif
#ifndef Vector4MA
#define Vector4MA(v,s,b,o) ((o)[0]=(v)[0]+(b)[0]*(s),(o)[1]=(v)[1]+(b)[1]*(s),(o)[2]=(v)[2]+(b)[2]*(s),(o)[3]=(v)[3]+(b)[3]*(s))
#endif

// OSP2 draw style flags
#ifndef DS_HLEFT
#define DS_HLEFT        0x0000
#define DS_HCENTER      0x0001
#define DS_HRIGHT       0x0002
#define DS_VTOP         0x0000
#define DS_VCENTER      0x0004
#define DS_VBOTTOM      0x0008
#define DS_PROPORTIONAL 0x0010
#define DS_SHADOW       0x0020
#define DS_EMOJI        0x0040
#define DS_FORCE_COLOR  0x0080
#define DS_MAX_WIDTH_IS_CHARS 0x0100
#endif

// Modern text/font system (implemented in cl_wired_fonts.c)
int  WiredFont_IdFromName( const char *name );
int  WiredFont_ToAlignment( int dsFlags );
int  WiredFont_ToTextFlags( int dsFlags );
qboolean CG_Hex16GetColor( const char *str, float *color );

// OSP2 compatibility functions — provided by cl_wired_hud_compat.h in client context
// (declarations removed to avoid macro conflicts with compat layer)

// OSP2 cvar stubs
extern vmCvar_t cg_MaxlocationWidth;

#ifndef DRAW_REWARDS_NOSOUND
#define DRAW_REWARDS_NOSOUND  0x02
#define DRAW_REWARDS_NOICON   0x04
#endif

#define SUPERHUD_DEFAULT_FADEDELAY 1000.0

typedef enum
{
	SUPERHUD_ALIGNH_LEFT,
	SUPERHUD_ALIGNH_CENTER,
	SUPERHUD_ALIGNH_RIGHT,
} superhudAlignH_t;

typedef enum
{
	SUPERHUD_ALIGNV_TOP,
	SUPERHUD_ALIGNV_CENTER,
	SUPERHUD_ALIGNV_BOTTOM,
} superhudAlignV_t;

typedef enum
{
	SUPERHUD_COLOR_RGBA,
	SUPERHUD_COLOR_T,
	SUPERHUD_COLOR_E,
	SUPERHUD_COLOR_I,
} superhudColorType_t;

typedef struct
{
	superhudColorType_t type;
	vec4_t rgba;
} superhudColor_t;

typedef enum
{
	SUPERHUD_DIR_LEFT_TO_RIGHT,
	SUPERHUD_DIR_RIGHT_TO_LEFT,
	SUPERHUD_DIR_TOP_TO_BOTTOM,
	SUPERHUD_DIR_BOTTOM_TO_TOP,
} superhudDirection_t;

typedef enum
{
	SUPERHUD_ITSIDE_BLUE,
	SUPERHUD_ITSIDE_RED,
	SUPERHUD_ITSIDE_NEUTRAL,
	SUPERHUD_ITSIDE_OWN,
	SUPERHUD_ITSIDE_ENEMY,
} superhudItTeam_t;

typedef struct
{
	struct
	{
		superhudAlignH_t value;
		qboolean isSet;
	} alignH;
	struct
	{
		superhudAlignV_t value;
		qboolean isSet;
	} alignV;
	struct
	{
		vec4_t value;
		qboolean isSet;
	} angles;
	struct
	{
		superhudColor_t value;
		qboolean isSet;
	} bgcolor;
	struct
	{
		superhudColor_t value;
		qboolean isSet;
	} bgcolor2;
	struct
	{
		vec4_t value;
		qboolean isSet;
	} border;
	struct
	{
		superhudColor_t value;
		qboolean isSet;
	} borderColor;
	struct
	{
		superhudColor_t value;
		qboolean isSet;
	} borderColor2;
	struct
	{
		vec4_t value;
		qboolean isSet;
	} hlcolor;
	struct
	{
		superhudColor_t value;
		qboolean isSet;
	} color;
	struct
	{
		superhudColor_t value;
		qboolean isSet;
	} color2;
	struct
	{
		superhudDirection_t value;
		qboolean isSet;
	} direction;
	struct
	{
		qboolean isSet;
	} doublebar;
	struct
	{
		vec4_t value;
		qboolean isSet;
	} fade;
	struct
	{
		int value;
		qboolean isSet;
	} fadedelay;
	struct
	{
		qboolean isSet;
	} fill;
	struct
	{
		char value[MAX_QPATH];
		qboolean isSet;
	} font;
	struct
	{
		vec2_t value;
		qboolean isSet;
	} fontsize;
	struct
	{
		char value[MAX_QPATH];
		qboolean isSet;
	} image;
	struct
	{
		vec4_t value;
		qboolean isSet;
	} imagetc;
	struct
	{
		superhudItTeam_t value;
		qboolean isSet;
	} itTeam;
	struct
	{
		vec4_t value;
		qboolean isSet;
	} margins;
	struct
	{
		char value[MAX_QPATH];
		qboolean isSet;
	} model;
	struct
	{
		qboolean isSet;
	} monospace;
	struct
	{
		vec3_t value;
		qboolean isSet;
	} offset;
	struct
	{
		vec4_t value;
		qboolean isSet;
	} rect;
	struct
	{
		vec4_t value;
		qboolean isSet;
	} shadowColor;
	struct
	{
		int value;
		qboolean isSet;
	} style;
	struct
	{
		char value[MAX_QPATH];
		qboolean isSet;
	} text;
	struct
	{
		superhudAlignH_t value;
		qboolean isSet;
	} textAlign;
	struct
	{
		vec2_t value;
		qboolean isSet;
	} textOffset;
	struct
	{
		int value;
		qboolean isSet;
	} textStyle;
	struct
	{
		int value;
		qboolean isSet;
	} time;
	struct
	{
		int value;
		qboolean isSet;
	} visflags;
	struct
	{
		int value;
		qboolean isSet;
	} hlsize;
	struct
	{
		int value;
		qboolean isSet;
	} visiblity;
	struct
	{
		char value[32];
		qboolean isSet;
	} bind;
} superhudConfig_t;

typedef superhudConfig_t superhudElementDefault_t;

typedef enum
{
	SUPERHUD_ELEMENT_TYPE_DEFAULTS,
}
superhudElementType_t;

typedef struct
{
	union
	{
		superhudElementDefault_t def; //todo remove it, default have no content
	} value;
	superhudElementType_t type;
} superhudElementContent_t;

typedef struct configFileLine_s
{
	char* line;
	int size;
	int line_number;
	struct configFileLine_s* next;
} configFileLine_t;

typedef struct configFileInfo_s
{
	int pos; /* position in line */
	configFileLine_t* root;
	configFileLine_t* last_line;
} configFileInfo_t;

typedef enum
{
	SUPERHUD_CONFIG_OK,
	SUPERHUD_CONFIG_UNEXPECTED_CHARACTER,
	SUPERHUD_CONFIG_END_OF_FILE,
	SUPERHUD_CONFIG_END_OF_ELEMENT,
	SUPERHUD_CONFIG_WRONG_ELEMENT_NAME,
	SUPERHUD_CONFIG_WRONG_COMMAND_NAME,
	SUPERHUD_CONFIG_LOST_ELEMENT_BODY,
} superhudConfigParseStatus_t;

#define SE_IM         (1 << 0)  // 0x00000001
#define SE_IM_STR "im"
#define SE_SIDES_ONLY  (1 << 1)  // 0x00000002
#define SE_SIDES_ONLY_STR "teamonly"
#define SE_SPECT      (1 << 2)  // 0x00000004
#define SE_SPECT_STR      "spectator"
#define SE_DEAD       (1 << 3)  // 0x00000008
#define SE_DEAD_STR       "dead"
#define SE_DEMO_HIDE  (1 << 4)  // 0x00000010
#define SE_DEMO_HIDE_STR  "demohide"
#define SE_SCORES_HIDE  (1 << 5)  // 0x00000020
#define SE_SCORES_HIDE_STR  "scoreshide"
#define SE_KEY1_SHOW  (1 << 6)  // 0x00000040
#define SE_KEY1_SHOW_STR  "key1show"
#define SE_KEY2_SHOW  (1 << 7)  // 0x00000080
#define SE_KEY2_SHOW_STR  "key2show"
#define SE_KEY3_SHOW  (1 << 8)  // 0x00000100
#define SE_KEY3_SHOW_STR  "key3show"
#define SE_KEY4_SHOW  (1 << 9)  // 0x00000200
#define SE_KEY4_SHOW_STR  "key4show"
#define SE_SHOW_EMPTY  (1 << 10) // 0x00000400
#define SE_SHOW_EMPTY_STR  "showempty"
// gametype visibility flags -- derived from bg_gametypelist[].hudToken
// bit positions are sequential from SE_MODE_BASE_BIT, one per unique hudToken
#define SE_MODE_BASE_BIT  11

#define SHUD_CHECK_SHOW_EMPTY(element) ( \
    ((element) != NULL) && \
    ((element)->config.visflags.isSet) && \
    (((element)->config.visflags.value & SE_SHOW_EMPTY) != 0) \
)

typedef struct superHUDConfigElement_s
{
	const char* name;
	int visibility;
	void* (*create)(const superhudConfig_t* config);
	void (*routine)(void* context);
	void (*destroy)(void* context);
	void* context;
	int order;
} superHUDConfigElement_t;

typedef struct superhudElementDictMember_s
{
	const superHUDConfigElement_t* element;
	struct superhudElementDictMember_s* next;
}
superhudElementDictMember_t;

typedef struct superhudElement_s
{
	superHUDConfigElement_t element;
	superhudConfig_t config;
	superhudElementContent_t content;
	struct superhudElement_s* next;
}
superhudElement_t;

typedef struct superHUDConfigCommand_s
{
	const char* name;
	superhudConfigParseStatus_t (*parse)(configFileInfo_t* finfo, superhudConfig_t* config);
	struct superHUDConfigCommand_s* next;
} superHUDConfigCommand_t;

typedef struct
{
	const superHUDConfigCommand_t* item;
	superhudConfigParseStatus_t status;
} superhudConfigParseCommand_t;

typedef struct
{
	const superHUDConfigElement_t* item;
	superhudConfigParseStatus_t status;
} superhudConfigParseElement_t;

#define SHUD_ELEMENT_INIT(E, CFG)                     \
    do{                                                 \
        E = Z_Malloc(sizeof(*E));                         \
        OSP_MEMORY_CHECK(E);                              \
        memset(E, 0, sizeof(*E));                         \
        memcpy(&E->config, CFG, sizeof(element->config)); \
    }while(0)

void CG_SHUDParserInit(void);
const superHUDConfigElement_t* CG_SHUDFindConfigElementItem(const char* name);
const superHUDConfigCommand_t* CG_SHUDFindConfigCommandItem(const char* name);

qboolean CG_SHUDFileInfoInit(configFileInfo_t* info, const char* fileContent);
void CG_SHUDFileInfoTeardown(configFileInfo_t* cfi);

qboolean CG_SHUDFileInfoGoToChar(configFileInfo_t* cfi, char to, qboolean next);
void CG_SHUDFileInfoSkipSpaces(configFileInfo_t* cfi);
qboolean CG_SHUDFileInfoSkipCommandEnd(configFileInfo_t* cfi);
superhudConfigParseElement_t CG_SHUDFileInfoGetElementItem(configFileInfo_t* cfi);
superhudConfigParseCommand_t CG_SHUDFileInfoGetCommandItem(configFileInfo_t* cfi);

void* CG_SHUDElementFPSCreate(const superhudConfig_t* config);
void CG_SHUDElementFPSRoutine(void* context);
void CG_SHUDElementFPSDestroy(void* context);

#if FEAT_MOVEMENT_KEYS
// shared routine/destroy for all key elements
void CG_SHUDElementKeyRoutine(void* context);
void CG_SHUDElementKeyDestroy(void* context);
// keydown factories
void* CG_SHUDElementKeyDownForwardCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyDownBackCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyDownLeftCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyDownRightCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyDownJumpCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyDownCrouchCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyDownAttackCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyDownUseCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyDownWalkCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyDownGestureCreate(const superhudConfig_t* c);
// keyup factories
void* CG_SHUDElementKeyUpForwardCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyUpBackCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyUpLeftCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyUpRightCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyUpJumpCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyUpCrouchCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyUpAttackCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyUpUseCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyUpWalkCreate(const superhudConfig_t* c);
void* CG_SHUDElementKeyUpGestureCreate(const superhudConfig_t* c);
#endif

#if FEAT_UNLAGGED
void* CG_SHUDElementNetStatsCreate(const superhudConfig_t* config);
void CG_SHUDElementNetStatsRoutine(void* context);
void CG_SHUDElementNetStatsDestroy(void* context);
#endif

void* CG_SHUDElementSBHCCreate(const superhudConfig_t* config);
void CG_SHUDElementSBHCRoutine(void* context);
void CG_SHUDElementSBHCDestroy(void* context);

void* CG_SHUDElementSBHBCreate(const superhudConfig_t* config);
void CG_SHUDElementSBHBRoutine(void* context);
void CG_SHUDElementSBHBDestroy(void* context);

void* CG_SHUDElementSBHICreate(const superhudConfig_t* config);
void CG_SHUDElementSBHIRoutine(void* context);
void CG_SHUDElementSBHIDestroy(void* context);

void* CG_SHUDElementSBACCreate(const superhudConfig_t* config);
void CG_SHUDElementSBACRoutine(void* context);
void CG_SHUDElementSBACDestroy(void* context);

void* CG_SHUDElementSBABCreate(const superhudConfig_t* config);
void CG_SHUDElementSBABRoutine(void* context);
void CG_SHUDElementSBABDestroy(void* context);

void* CG_SHUDElementSBAICreate(const superhudConfig_t* config);
void CG_SHUDElementSBAIRoutine(void* context);
void CG_SHUDElementSBAIDestroy(void* context);

void* CG_SHUDElementSBAmBCreate(const superhudConfig_t* config);
void CG_SHUDElementSBAmBRoutine(void* context);
void CG_SHUDElementSBAmBDestroy(void* context);

void* CG_SHUDElementSBAmCCreate(const superhudConfig_t* config);
void CG_SHUDElementSBAmCRoutine(void* context);
void CG_SHUDElementSBAmCDestroy(void* context);

void* CG_SHUDElementSBAmICreate(const superhudConfig_t* config);
void CG_SHUDElementSBAmIRoutine(void* context);
void CG_SHUDElementSBAmIDestroy(void* context);

void* CG_SHUDElementTargetNameCreate(const superhudConfig_t* config);
void CG_SHUDElementTargetNameRoutine(void* context);
void CG_SHUDElementTargetNameDestroy(void* context);

void* CG_SHUDElementTargetStatusCreate(const superhudConfig_t* config);
void CG_SHUDElementTargetStatusRoutine(void* context);
void CG_SHUDElementTargetStatusDestroy(void* context);

void* CG_SHUDElementVMWCreate(const superhudConfig_t* config);
void CG_SHUDElementVMWRoutine(void* context);
void CG_SHUDElementVMWDestroy(void* context);

void* CG_SHUDElementFragMessageCreate(const superhudConfig_t* config);
void CG_SHUDElementFragMessageRoutine(void* context);
void CG_SHUDElementFragMessageDestroy(void* context);

void* CG_SHUDElementRankMessageCreate(const superhudConfig_t* config);
void CG_SHUDElementRankMessageRoutine(void* context);
void CG_SHUDElementRankMessageDestroy(void* context);

void* CG_SHUDElementNGPCreate(const superhudConfig_t* config);
void CG_SHUDElementNGPRoutine(void* context);
void CG_SHUDElementNGPDestroy(void* context);

void* CG_SHUDElementNGCreate(const superhudConfig_t* config);
void CG_SHUDElementNGRoutine(void* context);
void CG_SHUDElementNGDestroy(void* context);

void* CG_SHUDElementDecorCreate(const superhudConfig_t* config);
void CG_SHUDElementDecorRoutine(void* context);
void CG_SHUDElementDecorDestroy(void* context);

void* CG_SHUDElementPlayerSpeedCreate(const superhudConfig_t* config);
void CG_SHUDElementPlayerSpeedRoutine(void* context);
void CG_SHUDElementPlayerSpeedDestroy(void* context);

void* CG_SHUDElementLocalTimeCreate(const superhudConfig_t* config);
void* CG_SHUDElementLocalDateCreate(const superhudConfig_t* config);
void CG_SHUDElementLocalTimeRoutine(void* context);
void CG_SHUDElementLocalTimeDestroy(void* context);

void* CG_SHUDElementAmmoMessageCreate(const superhudConfig_t* config);
void CG_SHUDElementAmmoMessageRoutine(void* context);
void CG_SHUDElementAmmoMessageDestroy(void* context);

void* CG_SHUDElementChat1Create(const superhudConfig_t* config);
void* CG_SHUDElementChat2Create(const superhudConfig_t* config);
void* CG_SHUDElementChat3Create(const superhudConfig_t* config);
void* CG_SHUDElementChat4Create(const superhudConfig_t* config);
void* CG_SHUDElementChat5Create(const superhudConfig_t* config);
void* CG_SHUDElementChat6Create(const superhudConfig_t* config);
void* CG_SHUDElementChat7Create(const superhudConfig_t* config);
void* CG_SHUDElementChat8Create(const superhudConfig_t* config);
void* CG_SHUDElementChat9Create(const superhudConfig_t* config);
void* CG_SHUDElementChat10Create(const superhudConfig_t* config);
void* CG_SHUDElementChat11Create(const superhudConfig_t* config);
void* CG_SHUDElementChat12Create(const superhudConfig_t* config);
void* CG_SHUDElementChat13Create(const superhudConfig_t* config);
void* CG_SHUDElementChat14Create(const superhudConfig_t* config);
void* CG_SHUDElementChat15Create(const superhudConfig_t* config);
void* CG_SHUDElementChat16Create(const superhudConfig_t* config);
void CG_SHUDElementChatRoutine(void* context);
void CG_SHUDElementChatDestroy(void* context);

void* CG_SHUDElementSpecMessageCreate(const superhudConfig_t* config);
void CG_SHUDElementSpecMessageRoutine(void* context);
void CG_SHUDElementSpecMessageDestroy(void* context);

void* CG_SHUDElementSpectatorsCreate(const superhudConfig_t* config);
void CG_SHUDElementSpectatorsRoutine(void* context);
void CG_SHUDElementSpectatorsDestroy(void* context);

void* CG_SHUDElementFollowMessageCreate(const superhudConfig_t* config);
void CG_SHUDElementFollowMessageRoutine(void* context);
void CG_SHUDElementFollowMessageDestroy(void* context);

void* CG_SHUDElementGameTimeCreate(const superhudConfig_t* config);
void CG_SHUDElementGameTimeRoutine(void* context);
void CG_SHUDElementGameTimeDestroy(void* context);

void* CG_SHUDElementItemPickupCreate(const superhudConfig_t* config);
void CG_SHUDElementItemPickupRoutine(void* context);
void CG_SHUDElementItemPickupDestroy(void* context);

void* CG_SHUDElementItemPickupIconCreate(const superhudConfig_t* config);
void CG_SHUDElementItemPickupIconRoutine(void* context);
void CG_SHUDElementItemPickupIconDestroy(void* context);

void* CG_SHUDElementFlagStatusNMECreate(const superhudConfig_t* config);
void* CG_SHUDElementFlagStatusOWNCreate(const superhudConfig_t* config);
void CG_SHUDElementFlagStatusRoutine(void* context);
void CG_SHUDElementFlagStatusDestroy(void* context);

void* CG_SHUDElementPlayerNameCreate(const superhudConfig_t* config);
void CG_SHUDElementPlayerNameRoutine(void* context);
void CG_SHUDElementPlayerNameDestroy(void* context);

#define SUPERHUD_UPDATE_TIME 50

void* CG_SHUDElementPwTime1Create(const superhudConfig_t* config);
void* CG_SHUDElementPwTime2Create(const superhudConfig_t* config);
void* CG_SHUDElementPwTime3Create(const superhudConfig_t* config);
void* CG_SHUDElementPwTime4Create(const superhudConfig_t* config);
void* CG_SHUDElementPwTime5Create(const superhudConfig_t* config);
void* CG_SHUDElementPwTime6Create(const superhudConfig_t* config);
void* CG_SHUDElementPwTime7Create(const superhudConfig_t* config);
void* CG_SHUDElementPwTime8Create(const superhudConfig_t* config);
void* CG_SHUDElementPwIcon1Create(const superhudConfig_t* config);
void* CG_SHUDElementPwIcon2Create(const superhudConfig_t* config);
void* CG_SHUDElementPwIcon3Create(const superhudConfig_t* config);
void* CG_SHUDElementPwIcon4Create(const superhudConfig_t* config);
void* CG_SHUDElementPwIcon5Create(const superhudConfig_t* config);
void* CG_SHUDElementPwIcon6Create(const superhudConfig_t* config);
void* CG_SHUDElementPwIcon7Create(const superhudConfig_t* config);
void* CG_SHUDElementPwIcon8Create(const superhudConfig_t* config);
void CG_SHUDElementPwRoutine(void* context);
void CG_SHUDElementPwDestroy(void* context);

void* CG_SHUDElementNameNMECreate(const superhudConfig_t* config);
void* CG_SHUDElementNameOWNCreate(const superhudConfig_t* config);
void CG_SHUDElementNameRoutine(void* context);
void CG_SHUDElementNameDestroy(void* context);

void* CG_SHUDElementScoreNMECreate(const superhudConfig_t* config);
void* CG_SHUDElementScoreOWNCreate(const superhudConfig_t* config);
void* CG_SHUDElementScoreMAXCreate(const superhudConfig_t* config);
void CG_SHUDElementScoreRoutine(void* context);
void CG_SHUDElementScoreDestroy(void* context);

void* CG_SHUDElementRewardIconCreate(const superhudConfig_t* config);
void* CG_SHUDElementRewardCountCreate(const superhudConfig_t* config);
void CG_SHUDElementRewardRoutine(void* context);
void CG_SHUDElementRewardDestroy(void* context);

void* CG_SHUDElementTeamCountOWNCreate(const superhudConfig_t* config);
void* CG_SHUDElementTeamCountNMECreate(const superhudConfig_t* config);
void CG_SHUDElementTeamCountRoutine(void* context);
void CG_SHUDElementTeamCountDestroy(void* context);

void* CG_SHUDElementTeam1Create(const superhudConfig_t* config);
void* CG_SHUDElementTeam2Create(const superhudConfig_t* config);
void* CG_SHUDElementTeam3Create(const superhudConfig_t* config);
void* CG_SHUDElementTeam4Create(const superhudConfig_t* config);
void* CG_SHUDElementTeam5Create(const superhudConfig_t* config);
void* CG_SHUDElementTeam6Create(const superhudConfig_t* config);
void* CG_SHUDElementTeam7Create(const superhudConfig_t* config);
void* CG_SHUDElementTeam8Create(const superhudConfig_t* config);
void* CG_SHUDElementTeam9Create(const superhudConfig_t* config);
void* CG_SHUDElementTeam10Create(const superhudConfig_t* config);
void* CG_SHUDElementTeam11Create(const superhudConfig_t* config);
void* CG_SHUDElementTeam12Create(const superhudConfig_t* config);
void* CG_SHUDElementTeam13Create(const superhudConfig_t* config);
void* CG_SHUDElementTeam14Create(const superhudConfig_t* config);
void* CG_SHUDElementTeam15Create(const superhudConfig_t* config);
void* CG_SHUDElementTeam16Create(const superhudConfig_t* config);
void CG_SHUDElementTeamRoutine(void* context);
void CG_SHUDElementTeamDestroy(void* context);

void* CG_SHUDElementWeaponListCreate(const superhudConfig_t* config);
void CG_SHUDElementWeaponListRoutine(void* context);
void CG_SHUDElementWeaponListDestroy(void* context);

void* CG_SHUDElementObituaries1Create(const superhudConfig_t* config);
void* CG_SHUDElementObituaries2Create(const superhudConfig_t* config);
void* CG_SHUDElementObituaries3Create(const superhudConfig_t* config);
void* CG_SHUDElementObituaries4Create(const superhudConfig_t* config);
void* CG_SHUDElementObituaries5Create(const superhudConfig_t* config);
void* CG_SHUDElementObituaries6Create(const superhudConfig_t* config);
void* CG_SHUDElementObituaries7Create(const superhudConfig_t* config);
void* CG_SHUDElementObituaries8Create(const superhudConfig_t* config);
void CG_SHUDElementObituariesRoutine(void* context);
void CG_SHUDElementObituariesDestroy(void* context);


void* CG_SHUDElementTempAccTextCreate(const superhudConfig_t* config);
void* CG_SHUDElementTempAccIconCreate(const superhudConfig_t* config);
void CG_SHUDElementTempAccRoutine(void* context);
void CG_SHUDElementTempAccDestroy(void* context);

void* CG_SHUDElementWarmupInfoCreate(const superhudConfig_t* config);
void CG_SHUDElementWarmupInfoRoutine(void* context);
void CG_SHUDElementWarmupInfoDestroy(void* context);
void* CG_SHUDElementGameTypeCreate(const superhudConfig_t* config);
void CG_SHUDElementGameTypeRoutine(void* context);
void CG_SHUDElementGameTypeDestroy(void* context);

void* CG_SHUDElementLocationCreate(const superhudConfig_t* config);
void CG_SHUDElementLocationRoutine(void* context);
void CG_SHUDElementLocationDestroy(void* context);

void* CG_SHUDElementCreateCurrentWeapon(const superhudConfig_t* config);
void* CG_SHUDElementWeaponStatsCreateMG(const superhudConfig_t* config);
void* CG_SHUDElementWeaponStatsCreateSG(const superhudConfig_t* config);
void* CG_SHUDElementWeaponStatsCreateGL(const superhudConfig_t* config);
void* CG_SHUDElementWeaponStatsCreateRL(const superhudConfig_t* config);
void* CG_SHUDElementWeaponStatsCreateLG(const superhudConfig_t* config);
void* CG_SHUDElementWeaponStatsCreateRG(const superhudConfig_t* config);
void* CG_SHUDElementWeaponStatsCreatePG(const superhudConfig_t* config);
void* CG_SHUDElementIconCreateCurrentWeapon(const superhudConfig_t* config);
void* CG_SHUDElementIconCreateMG(const superhudConfig_t* config);
void* CG_SHUDElementIconCreateSG(const superhudConfig_t* config);
void* CG_SHUDElementIconCreateGL(const superhudConfig_t* config);
void* CG_SHUDElementIconCreateRL(const superhudConfig_t* config);
void* CG_SHUDElementIconCreateLG(const superhudConfig_t* config);
void* CG_SHUDElementIconCreateRG(const superhudConfig_t* config);
void* CG_SHUDElementIconCreatePG(const superhudConfig_t* config);
void CG_SHUDElementWeaponStatsRoutine(void* context);
void CG_SHUDElementWeaponStatsDestroy(void* context);


void* CG_SHUDElementCreatePlayerStatsDG(const superhudConfig_t* config);
void* CG_SHUDElementCreatePlayerStatsDR(const superhudConfig_t* config);
void* CG_SHUDElementCreatePlayerStatsDamageRatio(const superhudConfig_t* config);
void* CG_SHUDElementCreatePlayerStatsDRIcon(const superhudConfig_t* config);
void* CG_SHUDElementCreatePlayerStatsDGIcon(const superhudConfig_t* config);
void CG_SHUDElementPlayerStatsRoutine(void* context);
void CG_SHUDElementPlayerStatsDestroy(void* context);

void* CG_SHUDElementGridCreate(const superhudConfig_t* config);
void CG_SHUDElementGridRoutine(void* context);
void CG_SHUDElementGridDestroy(void* context);

/*
 * cg_superhud_util.c
 */
typedef union
{
	struct
	{
		float x;
		float y;
		float w;
		float h;
	} named;
	vec4_t arr;
} superhudCoord_t;

typedef struct
{
	superhudCoord_t coord;
	int flags;
	vec4_t color_origin;
	vec4_t color;
	vec4_t shadowColor;
	vec4_t background;
	vec4_t border;
	vec4_t borderColor;
	int width;
	int fontId;
	const char* text;
} superhudTextContext_t;

typedef struct
{
	superhudCoord_t coord;
	superhudCoord_t coordPicture;
	qhandle_t image;
	vec4_t color;
	vec4_t color_origin;
} superhudDrawContext_t;

typedef struct
{
	superhudDirection_t direction;
	float max; // maximum coord for bar
	float koeff; //multiplier
	vec4_t bar[2]; // coord of two bars
	vec4_t color_top; // color of bar
	vec4_t color2_top; // color of top bar of doublebar
	vec4_t color_back; // color of background
	qboolean two_bars; // one or two bars
} superhudBarContext_t;

#define OSPHUD_TEAMOVERLAY_STR_SIZE 128
typedef struct
{
	int powerupOffsetChar;
	int powerupLenChar;
	float powerupOffsetPix;
	float powerupLenPix;

	int nameOffsetChar;
	int nameLenChar;
	float nameOffsetPix;
	float nameLenPix;

	int healthAndArmorOffsetChar;
	int healthAndArmorLenChar;
	float healthAndArmorOffsetPix;
	float healthAndArmorLenPix;

	int weaponOffsetChar;
	int weaponLenChar;
	float weaponOffsetPix;
	float weaponLenPix;

	int locationOffsetChar;
	int locationLenChar;
	float locationOffsetPix;
	float locationLenPix;

	int overlayWidthChar;
	float overlayWidthPix;
} shudTeamOverlay_t;

void CG_SHUDBarMakeContext(const superhudConfig_t* in, superhudBarContext_t* out, float max);
void CG_SHUDTextMakeContext(const superhudConfig_t* in, superhudTextContext_t* out);
void CG_SHUDDrawStretchPic(superhudCoord_t coord, const superhudCoord_t coordPicture, const float* color, qhandle_t shader);
void CG_SHUDDrawMakeContext(const superhudConfig_t* cfg, superhudDrawContext_t* out);

void CG_SHUDTextPrint(const superhudConfig_t* cfg, superhudTextContext_t* pos);
void CG_SHUDTextPrintNew(const superhudConfig_t* cfg, superhudTextContext_t* pos, qboolean colorOverride);
void CG_SHUDDrawStretchPicCtx(const superhudConfig_t* cfg, superhudDrawContext_t* out);
void CG_SHUDBarPrint(const superhudConfig_t* cfg, superhudBarContext_t* ctx, float value);
qboolean CG_SHUDFill(const superhudConfig_t* cfg);

team_t CG_SHUDGetOurActiveTeam(void);
qboolean CG_SHUDGetFadeColor(const vec4_t from_color, vec4_t out, const superhudConfig_t* cfg, int startTime);
void CG_SHUDFillWithColor(const superhudCoord_t* coord, const float* color);
void CG_SHUDElementCompileTeamOverlayConfig(int fontWidth, shudTeamOverlay_t* configOut);
void CG_SHUDConfigPickBgColor(const superhudConfig_t* config, float* color, qboolean alphaOverride);


typedef struct
{
	char message[MAX_SAY_TEXT];
	int time;
} superhudChatEntry_t;

typedef struct
{
	int time;
	int attacker;
	int target;
	int attackerTeam;
	int targetTeam;
	int mod;
	qboolean unfrozen;
	struct
	{
		qboolean isInitialized;
		qhandle_t iconShader;
		vec4_t attackerColor;
		vec4_t targetColor;
		char attackerName[MAX_QPATH];
		char targetName[MAX_QPATH];
		int maxVisibleChars;
		float baseX;
		float attackerWidth;
		float targetWidth;
		float spacing;
		int maxNameLenPix;
	} runtime;
} superhudObituariesEntry_t;

typedef struct
{
	float tempAccuracy;
} superhudTempAccEntry_t;

typedef struct
{
	int lastTrackedWeapon;
	float lastAccuracy;
	float kdratio;
	struct
	{
		float accuracy;
		int kills;
		int deaths;
		int hits;
		int shots;
		int pickUps;
		int drops;
	} stats[WIRED_WEAPON_BUFFER_SIZE];
} customStats_t;

#define SHUD_MAX_OBITUARIES_LINES 8
#define SHUD_MAX_CHAT_LINES 16
#define SHUD_MAX_POWERUPS 8
#define SHUD_MAX_AWARD_QUEUE 8

// award notification queue entry (for WIRED_EVENT_AWARD)
typedef struct {
	char        name[32];       // "Impressive", "Rampage", etc.
	char        shaderPath[64]; // "medal_impressive", "menu/medals/medal_rampage"
	int         count;          // cumulative count (×3)
	int         arriveTime;     // wiredHud->time when received
} superhudAwardEntry_t;

// circular buffer for award notifications
typedef struct {
	superhudAwardEntry_t entries[SHUD_MAX_AWARD_QUEUE];
	int                  writeIndex;
} superhudAwardQueue_t;

// ── unified message queue (frag messages + center prints) ─────────────
// Priority-ordered: HIGH (frags) preempt NORMAL (center prints) preempt LOW (warmup)

#define SHUD_MSG_QUEUE_SIZE  8
#define SHUD_MSG_MAX_LEN     256

typedef enum {
	SHUD_MSG_LOW,       // warmup countdown, non-urgent info
	SHUD_MSG_NORMAL,    // center prints (Rampage!, spree announcements)
	SHUD_MSG_HIGH       // frag messages (most important immediate feedback)
} superhudMsgPriority_t;

typedef struct {
	char                    line1[SHUD_MSG_MAX_LEN];   // primary text
	char                    line2[SHUD_MSG_MAX_LEN];   // secondary text (rank line, or empty)
	int                     arriveTime;                // when enqueued (ms)
	int                     displayTime;               // how long to show (ms)
	superhudMsgPriority_t   priority;
	qboolean                shown;                     // already displayed and expired
} superhudMsgEntry_t;

typedef struct {
	superhudMsgEntry_t entries[SHUD_MSG_QUEUE_SIZE];
	int     writeIndex;      // next write slot (circular)
	int     currentIndex;    // currently displaying entry (-1 = none)
	int     showStartTime;   // when current message started (0 = none showing)
} superhudMsgQueue_t;

typedef struct
{
	struct
	{
		int time;
		char message[256];
	} fragmessage;
	struct
	{
		int time;
		char message[256];
	} rankmessage;
	struct
	{
		superhudChatEntry_t line[SHUD_MAX_CHAT_LINES];
		unsigned int index;
	} chat;
	struct
	{
		superhudObituariesEntry_t line[SHUD_MAX_OBITUARIES_LINES];
		unsigned int index;
	} obituaries;
	struct superhudPowerupsCache_t
	{
		struct superhudPowerupElement_t
		{
			int time;
			int powerup;
			qboolean isHoldable;
		} element[SHUD_MAX_POWERUPS];
		int numberOfActive;
		int lastUpdateTime;
	} powerupsCache;
	struct
	{
		superhudTempAccEntry_t weapon[WIRED_WEAPON_BUFFER_SIZE];
	} tempAcc;
	customStats_t customStats;
	superhudAwardQueue_t awards;
	superhudMsgQueue_t msgQueue;
} superhudGlobalContext_t;

superhudGlobalContext_t* CG_SHUDGetContext(void);
void CG_SHUDAvailableElementsInit(void);
const superHUDConfigElement_t* CG_SHUDAvailableElementsGet(void);

int CG_SHUDGetAmmo(int wpi);

void CG_SHUDFillAndFrameForText(superhudConfig_t* cfg, superhudTextContext_t* ctx);
qboolean CG_SHUDDrawBorder(const superhudConfig_t* cfg);
void CG_SHUDConfigPickBorderColor(const superhudConfig_t* config, float* color, qboolean alphaOverride);
void CG_SHUDDrawBorderDirect(const superhudCoord_t* coord, const vec4_t border, const vec4_t borderColor);

#ifdef __cplusplus
}
#endif

#endif
