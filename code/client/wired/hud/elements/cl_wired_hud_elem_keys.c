#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI && FEAT_MOVEMENT_KEYS

typedef struct {
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
	int keyBit;        // KEYS_* bit to check
	qboolean invert;   // qfalse = keydown (show when pressed), qtrue = keyup (show when released)
	const char *label;
} modernHudElementKey_t;

static void* CG_ModernHUDElementKeyCreate(const modernhudConfig_t* cfg, int keyBit, qboolean invert, const char *label) {
	modernHudElementKey_t* element;
	ModernHUD_ELEMENT_INIT(element, cfg);
	element->keyBit = keyBit;
	element->invert = invert;
	element->label = label;
	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);
	return element;
}

void CG_ModernHUDElementKeyRoutine(void* context) {
	modernHudElementKey_t* el = (modernHudElementKey_t*)context;
	int keys;
	qboolean pressed;

	if ( !cg.snap ) return;

	keys = cg.snap->ps.stats[STAT_KEYS];
	pressed = (keys & el->keyBit) != 0;

	if ( el->invert ) pressed = !pressed;
	if ( !pressed ) return;

	el->ctx.text = el->label;
	CG_ModernHUDTextPrint(&el->config, &el->ctx);
}

void CG_ModernHUDElementKeyDestroy(void* context) {
	if (context) Z_Free(context);
}

// ── keydown factories ────────────────────────────────────────────────────

void* CG_ModernHUDElementKeyDownForwardCreate(const modernhudConfig_t* c) { return CG_ModernHUDElementKeyCreate(c, KEYS_FORWARD, qfalse, "W"); }
void* CG_ModernHUDElementKeyDownBackCreate(const modernhudConfig_t* c)    { return CG_ModernHUDElementKeyCreate(c, KEYS_BACK,    qfalse, "S"); }
void* CG_ModernHUDElementKeyDownLeftCreate(const modernhudConfig_t* c)    { return CG_ModernHUDElementKeyCreate(c, KEYS_LEFT,    qfalse, "A"); }
void* CG_ModernHUDElementKeyDownRightCreate(const modernhudConfig_t* c)   { return CG_ModernHUDElementKeyCreate(c, KEYS_RIGHT,   qfalse, "D"); }
void* CG_ModernHUDElementKeyDownJumpCreate(const modernhudConfig_t* c)    { return CG_ModernHUDElementKeyCreate(c, KEYS_JUMP,    qfalse, "JUMP"); }
void* CG_ModernHUDElementKeyDownCrouchCreate(const modernhudConfig_t* c)  { return CG_ModernHUDElementKeyCreate(c, KEYS_CROUCH,  qfalse, "CROUCH"); }
void* CG_ModernHUDElementKeyDownAttackCreate(const modernhudConfig_t* c)  { return CG_ModernHUDElementKeyCreate(c, KEYS_ATTACK,  qfalse, "FIRE"); }
void* CG_ModernHUDElementKeyDownUseCreate(const modernhudConfig_t* c)     { return CG_ModernHUDElementKeyCreate(c, KEYS_USE,     qfalse, "USE"); }
void* CG_ModernHUDElementKeyDownWalkCreate(const modernhudConfig_t* c)    { return CG_ModernHUDElementKeyCreate(c, KEYS_WALK,    qfalse, "WALK"); }
void* CG_ModernHUDElementKeyDownGestureCreate(const modernhudConfig_t* c) { return CG_ModernHUDElementKeyCreate(c, KEYS_GESTURE, qfalse, "GESTURE"); }

// ── keyup factories ──────────────────────────────────────────────────────

void* CG_ModernHUDElementKeyUpForwardCreate(const modernhudConfig_t* c) { return CG_ModernHUDElementKeyCreate(c, KEYS_FORWARD, qtrue, "W"); }
void* CG_ModernHUDElementKeyUpBackCreate(const modernhudConfig_t* c)    { return CG_ModernHUDElementKeyCreate(c, KEYS_BACK,    qtrue, "S"); }
void* CG_ModernHUDElementKeyUpLeftCreate(const modernhudConfig_t* c)    { return CG_ModernHUDElementKeyCreate(c, KEYS_LEFT,    qtrue, "A"); }
void* CG_ModernHUDElementKeyUpRightCreate(const modernhudConfig_t* c)   { return CG_ModernHUDElementKeyCreate(c, KEYS_RIGHT,   qtrue, "D"); }
void* CG_ModernHUDElementKeyUpJumpCreate(const modernhudConfig_t* c)    { return CG_ModernHUDElementKeyCreate(c, KEYS_JUMP,    qtrue, "JUMP"); }
void* CG_ModernHUDElementKeyUpCrouchCreate(const modernhudConfig_t* c)  { return CG_ModernHUDElementKeyCreate(c, KEYS_CROUCH,  qtrue, "CROUCH"); }
void* CG_ModernHUDElementKeyUpAttackCreate(const modernhudConfig_t* c)  { return CG_ModernHUDElementKeyCreate(c, KEYS_ATTACK,  qtrue, "FIRE"); }
void* CG_ModernHUDElementKeyUpUseCreate(const modernhudConfig_t* c)     { return CG_ModernHUDElementKeyCreate(c, KEYS_USE,     qtrue, "USE"); }
void* CG_ModernHUDElementKeyUpWalkCreate(const modernhudConfig_t* c)    { return CG_ModernHUDElementKeyCreate(c, KEYS_WALK,    qtrue, "WALK"); }
void* CG_ModernHUDElementKeyUpGestureCreate(const modernhudConfig_t* c) { return CG_ModernHUDElementKeyCreate(c, KEYS_GESTURE, qtrue, "GESTURE"); }

#endif // FEAT_WIRED_UI && FEAT_MOVEMENT_KEYS
