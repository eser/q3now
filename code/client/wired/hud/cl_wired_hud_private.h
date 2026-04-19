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

#define MODERNHUD_DEFAULT_FADEDELAY 1000.0

typedef enum
{
	MODERNHUD_ALIGNH_LEFT,
	MODERNHUD_ALIGNH_CENTER,
	MODERNHUD_ALIGNH_RIGHT,
} modernhudAlignH_t;

typedef enum
{
	MODERNHUD_ALIGNV_TOP,
	MODERNHUD_ALIGNV_CENTER,
	MODERNHUD_ALIGNV_BOTTOM,
} modernhudAlignV_t;

typedef enum
{
	MODERNHUD_COLOR_RGBA,
	MODERNHUD_COLOR_T,
	MODERNHUD_COLOR_E,
	MODERNHUD_COLOR_I,
} modernhudColorType_t;

typedef struct
{
	modernhudColorType_t type;
	vec4_t rgba;
} modernhudColor_t;

typedef enum
{
	MODERNHUD_DIR_LEFT_TO_RIGHT,
	MODERNHUD_DIR_RIGHT_TO_LEFT,
	MODERNHUD_DIR_TOP_TO_BOTTOM,
	MODERNHUD_DIR_BOTTOM_TO_TOP,
} modernhudDirection_t;

typedef enum
{
	MODERNHUD_ITSIDE_BLUE,
	MODERNHUD_ITSIDE_RED,
	MODERNHUD_ITSIDE_NEUTRAL,
	MODERNHUD_ITSIDE_OWN,
	MODERNHUD_ITSIDE_ENEMY,
} modernhudItTeam_t;

typedef struct
{
	struct
	{
		modernhudAlignH_t value;
		qboolean isSet;
	} alignH;
	struct
	{
		modernhudAlignV_t value;
		qboolean isSet;
	} alignV;
	struct
	{
		vec4_t value;
		qboolean isSet;
	} angles;
	struct
	{
		modernhudColor_t value;
		qboolean isSet;
	} bgcolor;
	struct
	{
		modernhudColor_t value;
		qboolean isSet;
	} bgcolor2;
	struct
	{
		vec4_t value;
		qboolean isSet;
	} border;
	struct
	{
		modernhudColor_t value;
		qboolean isSet;
	} borderColor;
	struct
	{
		modernhudColor_t value;
		qboolean isSet;
	} borderColor2;
	struct
	{
		vec4_t value;
		qboolean isSet;
	} hlcolor;
	struct
	{
		modernhudColor_t value;
		qboolean isSet;
	} color;
	struct
	{
		modernhudColor_t value;
		qboolean isSet;
	} color2;
	struct
	{
		modernhudDirection_t value;
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
		int value;
		qboolean isSet;
	} fontWeight;
	struct
	{
		vec2_t value;
		qboolean isSet;
	} fontsize;
	struct
	{
		float value;
		qboolean isSet;
	} letterspacing;
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
		modernhudItTeam_t value;
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
		modernhudAlignH_t value;
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
} modernhudConfig_t;

typedef modernhudConfig_t modernhudElementDefault_t;

typedef enum
{
	MODERNHUD_ELEMENT_TYPE_DEFAULTS,
}
modernhudElementType_t;

typedef struct
{
	union
	{
		modernhudElementDefault_t def; //todo remove it, default have no content
	} value;
	modernhudElementType_t type;
} modernhudElementContent_t;

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
	MODERNHUD_CONFIG_OK,
	MODERNHUD_CONFIG_UNEXPECTED_CHARACTER,
	MODERNHUD_CONFIG_END_OF_FILE,
	MODERNHUD_CONFIG_END_OF_ELEMENT,
	MODERNHUD_CONFIG_WRONG_ELEMENT_NAME,
	MODERNHUD_CONFIG_WRONG_COMMAND_NAME,
	MODERNHUD_CONFIG_LOST_ELEMENT_BODY,
} modernhudConfigParseStatus_t;

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

#define ModernHUD_CHECK_SHOW_EMPTY(element) ( \
    ((element) != NULL) && \
    ((element)->config.visflags.isSet) && \
    (((element)->config.visflags.value & SE_SHOW_EMPTY) != 0) \
)

typedef struct modernHUDConfigElement_s
{
	const char* name;
	int visibility;
	void* (*create)(const modernhudConfig_t* config);
	void (*routine)(void* context);
	void (*destroy)(void* context);
	void* context;
	int order;
} modernHUDConfigElement_t;

typedef struct modernhudElementDictMember_s
{
	const modernHUDConfigElement_t* element;
	struct modernhudElementDictMember_s* next;
}
modernhudElementDictMember_t;

typedef struct modernhudElement_s
{
	modernHUDConfigElement_t element;
	modernhudConfig_t config;
	modernhudElementContent_t content;
	struct modernhudElement_s* next;
}
modernhudElement_t;

typedef struct modernHUDConfigCommand_s
{
	const char* name;
	modernhudConfigParseStatus_t (*parse)(configFileInfo_t* finfo, modernhudConfig_t* config);
	struct modernHUDConfigCommand_s* next;
} modernHUDConfigCommand_t;

typedef struct
{
	const modernHUDConfigCommand_t* item;
	modernhudConfigParseStatus_t status;
} modernhudConfigParseCommand_t;

typedef struct
{
	const modernHUDConfigElement_t* item;
	modernhudConfigParseStatus_t status;
} modernhudConfigParseElement_t;

#define ModernHUD_ELEMENT_INIT(E, CFG)                     \
    do{                                                 \
        E = Z_Malloc(sizeof(*E));                         \
        OSP_MEMORY_CHECK(E);                              \
        memset(E, 0, sizeof(*E));                         \
        memcpy(&E->config, CFG, sizeof(element->config)); \
    }while(0)

// Alloc + init + wire text context; requires element to have a 'ctx' field of type modernhudTextContext_t.
#define WHUD_ELEMENT_INIT_TEXT( E, CFG ) do { \
    ModernHUD_ELEMENT_INIT( E, CFG ); \
    CG_ModernHUDTextMakeContext( &(E)->config, &(E)->ctx ); \
    CG_ModernHUDFillAndFrameForText( &(E)->config, &(E)->ctx ); \
} while(0)

// Evaluate fade color, clear time-ref and return when fully faded, then print.
#define WHUD_FADE_AND_PRINT( config, ctx, timeRef ) do { \
    if ( !CG_ModernHUDGetFadeColor( (ctx)->color_origin, (ctx)->color, (config), *(timeRef) ) ) { \
        *(timeRef) = 0; return; \
    } \
    CG_ModernHUDTextPrint( (config), (ctx) ); \
} while(0)

void CG_ModernHUDParserInit(void);
const modernHUDConfigElement_t* CG_ModernHUDFindConfigElementItem(const char* name);
const modernHUDConfigCommand_t* CG_ModernHUDFindConfigCommandItem(const char* name);

qboolean CG_ModernHUDFileInfoInit(configFileInfo_t* info, const char* fileContent);
void CG_ModernHUDFileInfoTeardown(configFileInfo_t* cfi);

qboolean CG_ModernHUDFileInfoGoToChar(configFileInfo_t* cfi, char to, qboolean next);
void CG_ModernHUDFileInfoSkipSpaces(configFileInfo_t* cfi);
qboolean CG_ModernHUDFileInfoSkipCommandEnd(configFileInfo_t* cfi);
modernhudConfigParseElement_t CG_ModernHUDFileInfoGetElementItem(configFileInfo_t* cfi);
modernhudConfigParseCommand_t CG_ModernHUDFileInfoGetCommandItem(configFileInfo_t* cfi);

void* CG_ModernHUDElementFPSCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementFPSRoutine(void* context);

#if FEAT_MOVEMENT_KEYS
// shared routine/destroy for all key elements
void CG_ModernHUDElementKeyRoutine(void* context);
// keydown factories
void* CG_ModernHUDElementKeyDownForwardCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyDownBackCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyDownLeftCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyDownRightCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyDownJumpCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyDownCrouchCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyDownAttackCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyDownUseCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyDownWalkCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyDownGestureCreate(const modernhudConfig_t* c);
// keyup factories
void* CG_ModernHUDElementKeyUpForwardCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyUpBackCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyUpLeftCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyUpRightCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyUpJumpCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyUpCrouchCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyUpAttackCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyUpUseCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyUpWalkCreate(const modernhudConfig_t* c);
void* CG_ModernHUDElementKeyUpGestureCreate(const modernhudConfig_t* c);
#endif

#if FEAT_UNLAGGED
void* CG_ModernHUDElementNetStatsCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementNetStatsRoutine(void* context);
#endif

void* CG_ModernHUDElementSBHCCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementSBHCRoutine(void* context);
void CG_ModernHUDElementSBHCDestroy(void* context);

void* CG_ModernHUDElementSBHBCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementSBHBRoutine(void* context);
void CG_ModernHUDElementSBHBDestroy(void* context);

void* CG_ModernHUDElementSBHICreate(const modernhudConfig_t* config);
void CG_ModernHUDElementSBHIRoutine(void* context);
void CG_ModernHUDElementSBHIDestroy(void* context);

void* CG_ModernHUDElementSBACCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementSBACRoutine(void* context);
void CG_ModernHUDElementSBACDestroy(void* context);

void* CG_ModernHUDElementSBABCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementSBABRoutine(void* context);
void CG_ModernHUDElementSBABDestroy(void* context);

void* CG_ModernHUDElementSBAICreate(const modernhudConfig_t* config);
void CG_ModernHUDElementSBAIRoutine(void* context);
void CG_ModernHUDElementSBAIDestroy(void* context);

void* CG_ModernHUDElementSBAmBCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementSBAmBRoutine(void* context);
void CG_ModernHUDElementSBAmBDestroy(void* context);

void* CG_ModernHUDElementSBAmCCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementSBAmCRoutine(void* context);
void CG_ModernHUDElementSBAmCDestroy(void* context);

void* CG_ModernHUDElementSBAmICreate(const modernhudConfig_t* config);
void CG_ModernHUDElementSBAmIRoutine(void* context);
void CG_ModernHUDElementSBAmIDestroy(void* context);

void* CG_ModernHUDElementTargetNameCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementTargetNameRoutine(void* context);

void* CG_ModernHUDElementTargetStatusCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementTargetStatusRoutine(void* context);

void* CG_ModernHUDElementVMWCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementVMWRoutine(void* context);

void* CG_ModernHUDElementFragMessageCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementFragMessageRoutine(void* context);

void* CG_ModernHUDElementRankMessageCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementRankMessageRoutine(void* context);

void* CG_ModernHUDElementNGPCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementNGPRoutine(void* context);

void* CG_ModernHUDElementNGCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementNGRoutine(void* context);

void* CG_ModernHUDElementDecorCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementDecorRoutine(void* context);

void* CG_ModernHUDElementPlayerSpeedCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementPlayerSpeedRoutine(void* context);

void* CG_ModernHUDElementLocalTimeCreate(const modernhudConfig_t* config);
void* CG_ModernHUDElementLocalDateCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementLocalTimeRoutine(void* context);

void* CG_ModernHUDElementAmmoMessageCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementAmmoMessageRoutine(void* context);

void* CG_ModernHUDElementChatCreate(const modernhudConfig_t* config, int index);
void CG_ModernHUDElementChatRoutine(void* context);

void* CG_ModernHUDElementSpecMessageCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementSpecMessageRoutine(void* context);

void* CG_ModernHUDElementSpectatorsCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementSpectatorsRoutine(void* context);

void* CG_ModernHUDElementFollowMessageCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementFollowMessageRoutine(void* context);

void* CG_ModernHUDElementGameTimeCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementGameTimeRoutine(void* context);

void* CG_ModernHUDElementItemPickupCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementItemPickupRoutine(void* context);

void* CG_ModernHUDElementItemPickupIconCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementItemPickupIconRoutine(void* context);

void* CG_ModernHUDElementFlagStatusNMECreate(const modernhudConfig_t* config);
void* CG_ModernHUDElementFlagStatusOWNCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementFlagStatusRoutine(void* context);

void* CG_ModernHUDElementPlayerNameCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementPlayerNameRoutine(void* context);

#define MODERNHUD_UPDATE_TIME 50

void* CG_ModernHUDElementPwTimeCreate(const modernhudConfig_t* config, int index);
void* CG_ModernHUDElementPwIconCreate(const modernhudConfig_t* config, int index);
void CG_ModernHUDElementPwRoutine(void* context);

void* CG_ModernHUDElementNameNMECreate(const modernhudConfig_t* config);
void* CG_ModernHUDElementNameOWNCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementNameRoutine(void* context);

void* CG_ModernHUDElementScoreNMECreate(const modernhudConfig_t* config);
void* CG_ModernHUDElementScoreOWNCreate(const modernhudConfig_t* config);
void* CG_ModernHUDElementScoreMAXCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementScoreRoutine(void* context);

void* CG_ModernHUDElementRewardIconCreate(const modernhudConfig_t* config);
void* CG_ModernHUDElementRewardCountCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementRewardRoutine(void* context);

void* CG_ModernHUDElementTeamCountOWNCreate(const modernhudConfig_t* config);
void* CG_ModernHUDElementTeamCountNMECreate(const modernhudConfig_t* config);
void CG_ModernHUDElementTeamCountRoutine(void* context);

void* CG_ModernHUDElementTeamCreate(const modernhudConfig_t* config, int index);
void CG_ModernHUDElementTeamRoutine(void* context);

void* CG_ModernHUDElementWeaponListCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementWeaponListRoutine(void* context);



void* CG_ModernHUDElementTempAccTextCreate(const modernhudConfig_t* config);
void* CG_ModernHUDElementTempAccIconCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementTempAccRoutine(void* context);

void* CG_ModernHUDElementWarmupInfoCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementWarmupInfoRoutine(void* context);
void* CG_ModernHUDElementGameTypeCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementGameTypeRoutine(void* context);

void* CG_ModernHUDElementLocationCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementLocationRoutine(void* context);

void* CG_ModernHUDElementCreateCurrentWeapon(const modernhudConfig_t* config);
void* CG_ModernHUDElementWeaponStatsCreateMG(const modernhudConfig_t* config);
void* CG_ModernHUDElementWeaponStatsCreateSG(const modernhudConfig_t* config);
void* CG_ModernHUDElementWeaponStatsCreateGL(const modernhudConfig_t* config);
void* CG_ModernHUDElementWeaponStatsCreateRL(const modernhudConfig_t* config);
void* CG_ModernHUDElementWeaponStatsCreateLG(const modernhudConfig_t* config);
void* CG_ModernHUDElementWeaponStatsCreateRG(const modernhudConfig_t* config);
void* CG_ModernHUDElementWeaponStatsCreatePG(const modernhudConfig_t* config);
void* CG_ModernHUDElementIconCreateCurrentWeapon(const modernhudConfig_t* config);
void* CG_ModernHUDElementIconCreateMG(const modernhudConfig_t* config);
void* CG_ModernHUDElementIconCreateSG(const modernhudConfig_t* config);
void* CG_ModernHUDElementIconCreateGL(const modernhudConfig_t* config);
void* CG_ModernHUDElementIconCreateRL(const modernhudConfig_t* config);
void* CG_ModernHUDElementIconCreateLG(const modernhudConfig_t* config);
void* CG_ModernHUDElementIconCreateRG(const modernhudConfig_t* config);
void* CG_ModernHUDElementIconCreatePG(const modernhudConfig_t* config);
void CG_ModernHUDElementWeaponStatsRoutine(void* context);


void* CG_ModernHUDElementCreatePlayerStatsDG(const modernhudConfig_t* config);
void* CG_ModernHUDElementCreatePlayerStatsDR(const modernhudConfig_t* config);
void* CG_ModernHUDElementCreatePlayerStatsDamageRatio(const modernhudConfig_t* config);
void* CG_ModernHUDElementCreatePlayerStatsDRIcon(const modernhudConfig_t* config);
void* CG_ModernHUDElementCreatePlayerStatsDGIcon(const modernhudConfig_t* config);
void CG_ModernHUDElementPlayerStatsRoutine(void* context);

void* CG_ModernHUDElementGridCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementGridRoutine(void* context);

void* CG_ModernHUDElementAudioWaveformCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementAudioWaveformRoutine(void* context);

void* CG_ModernHUDElementMsgQueueCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementMsgQueueRoutine(void* context);

void* CG_ModernHUDElementBotDirectivesCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementBotDirectivesRoutine(void* context);

void* CG_ModernHUDElementAwardsCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementAwardsRoutine(void* context);

void* CG_ModernHUDElementCrosshairCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementCrosshairRoutine(void* context);

void* CG_ModernHUDElementStatusbarValueCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementStatusbarValueRoutine(void* context);

void* CG_ModernHUDElementStatusbarIconCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementStatusbarIconRoutine(void* context);

void* CG_ModernHUDElementStatusbarBarCreate(const modernhudConfig_t* config);
void CG_ModernHUDElementStatusbarBarRoutine(void* context);

/*
 * cg_modernhud_util.c
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
} modernhudCoord_t;

typedef struct
{
	modernhudCoord_t coord;
	int flags;
	vec4_t color_origin;
	vec4_t color;
	vec4_t shadowColor;
	vec4_t background;
	vec4_t border;
	vec4_t borderColor;
	int width;
	int fontId;
	float letterSpacing;
	const char* text;
} modernhudTextContext_t;

typedef struct
{
	modernhudCoord_t coord;
	modernhudCoord_t coordPicture;
	qhandle_t image;
	vec4_t color;
	vec4_t color_origin;
} modernhudDrawContext_t;

typedef struct
{
	modernhudDirection_t direction;
	float max; // maximum coord for bar
	float koeff; //multiplier
	vec4_t bar[2]; // coord of two bars
	vec4_t color_top; // color of bar
	vec4_t color2_top; // color of top bar of doublebar
	vec4_t color_back; // color of background
	qboolean two_bars; // one or two bars
} modernhudBarContext_t;

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
} modernHudTeamOverlay_t;

void CG_ModernHUDBarMakeContext(const modernhudConfig_t* in, modernhudBarContext_t* out, float max);
void CG_ModernHUDTextMakeContext(const modernhudConfig_t* in, modernhudTextContext_t* out);
void CG_ModernHUDDrawStretchPic(modernhudCoord_t coord, const modernhudCoord_t coordPicture, const float* color, qhandle_t shader);
void CG_ModernHUDDrawMakeContext(const modernhudConfig_t* cfg, modernhudDrawContext_t* out);

void CG_ModernHUDTextPrint(const modernhudConfig_t* cfg, modernhudTextContext_t* pos);
void CG_ModernHUDTextPrintNew(const modernhudConfig_t* cfg, modernhudTextContext_t* pos, qboolean colorOverride);
void CG_ModernHUDDrawStretchPicCtx(const modernhudConfig_t* cfg, modernhudDrawContext_t* out);
void CG_ModernHUDBarPrint(const modernhudConfig_t* cfg, modernhudBarContext_t* ctx, float value);
qboolean CG_ModernHUDFill(const modernhudConfig_t* cfg);

team_t CG_ModernHUDGetOurActiveTeam(void);
qboolean CG_ModernHUDGetFadeColor(const vec4_t from_color, vec4_t out, const modernhudConfig_t* cfg, int startTime);
void CG_ModernHUDFillWithColor(const modernhudCoord_t* coord, const float* color);
void CG_ModernHUDElementCompileTeamOverlayConfig(int fontWidth, modernHudTeamOverlay_t* configOut);
void CG_ModernHUDConfigPickBgColor(const modernhudConfig_t* config, float* color, qboolean alphaOverride);


typedef struct
{
	char message[MAX_SAY_TEXT];
	int time;
} modernhudChatEntry_t;


typedef struct
{
	float tempAccuracy;
} modernhudTempAccEntry_t;

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

#define ModernHUD_MAX_CHAT_LINES 16
#define ModernHUD_MAX_POWERUPS 8
#define ModernHUD_MAX_AWARD_QUEUE 8

// award notification queue entry (for WIRED_EVENT_AWARD)
typedef struct {
	char        name[32];       // "Impressive", "Rampage", etc.
	char        shaderPath[64]; // "medal_impressive", "menu/medals/medal_rampage"
	int         count;          // cumulative count (×3)
	int         arriveTime;     // wiredHud->time when received
} modernhudAwardEntry_t;

// circular buffer for award notifications
typedef struct {
	modernhudAwardEntry_t entries[ModernHUD_MAX_AWARD_QUEUE];
	int                  writeIndex;
} modernhudAwardQueue_t;

// ── unified message queue (frag messages + center prints) ─────────────
// Priority-ordered: HIGH (frags) preempt NORMAL (center prints) preempt LOW (warmup)

#define ModernHUD_MSG_QUEUE_SIZE  8
#define ModernHUD_MSG_MAX_LEN     256

typedef enum {
	ModernHUD_MSG_LOW,       // warmup countdown, non-urgent info
	ModernHUD_MSG_NORMAL,    // center prints (Rampage!, spree announcements)
	ModernHUD_MSG_HIGH       // frag messages (most important immediate feedback)
} modernhudMsgPriority_t;

typedef struct {
	char                    line1[ModernHUD_MSG_MAX_LEN];   // primary text
	char                    line2[ModernHUD_MSG_MAX_LEN];   // secondary text (rank line, or empty)
	int                     arriveTime;                // when enqueued (ms)
	int                     displayTime;               // how long to show (ms)
	modernhudMsgPriority_t   priority;
	qboolean                shown;                     // already displayed and expired
} modernhudMsgEntry_t;

typedef struct {
	modernhudMsgEntry_t entries[ModernHUD_MSG_QUEUE_SIZE];
	int     writeIndex;      // next write slot (circular)
	int     currentIndex;    // currently displaying entry (-1 = none)
	int     showStartTime;   // when current message started (0 = none showing)
} modernhudMsgQueue_t;

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
		modernhudChatEntry_t line[ModernHUD_MAX_CHAT_LINES];
		unsigned int index;
	} chat;
	struct modernhudPowerupsCache_t
	{
		struct modernhudPowerupElement_t
		{
			int time;
			int powerup;
			qboolean isHoldable;
		} element[ModernHUD_MAX_POWERUPS];
		int numberOfActive;
		int lastUpdateTime;
	} powerupsCache;
	struct
	{
		modernhudTempAccEntry_t weapon[WIRED_WEAPON_BUFFER_SIZE];
	} tempAcc;
	customStats_t customStats;
	modernhudAwardQueue_t awards;
	modernhudMsgQueue_t msgQueue;
} modernhudGlobalContext_t;

modernhudGlobalContext_t* CG_ModernHUDGetContext(void);
void CG_ModernHUDAvailableElementsInit(void);
const modernHUDConfigElement_t* CG_ModernHUDAvailableElementsGet(void);

int CG_ModernHUDGetAmmo(int wpi);

void CG_ModernHUDFillAndFrameForText(modernhudConfig_t* cfg, modernhudTextContext_t* ctx);
qboolean CG_ModernHUDDrawBorder(const modernhudConfig_t* cfg);
void CG_ModernHUDConfigPickBorderColor(const modernhudConfig_t* config, float* color, qboolean alphaOverride);
void CG_ModernHUDDrawBorderDirect(const modernhudCoord_t* coord, const vec4_t border, const vec4_t borderColor);

#ifdef __cplusplus
}
#endif

#endif
