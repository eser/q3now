#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI && FEAT_MOVEMENT_KEYS

typedef struct {
	superhudConfig_t config;
	superhudTextContext_t ctx;
	int keyBit;        // KEYS_* bit to check
	qboolean invert;   // qfalse = keydown (show when pressed), qtrue = keyup (show when released)
	const char *label;
} shudElementKey_t;

static void* CG_SHUDElementKeyCreate(const superhudConfig_t* cfg, int keyBit, qboolean invert, const char *label) {
	shudElementKey_t* element;
	SHUD_ELEMENT_INIT(element, cfg);
	element->keyBit = keyBit;
	element->invert = invert;
	element->label = label;
	CG_SHUDTextMakeContext(&element->config, &element->ctx);
	CG_SHUDFillAndFrameForText(&element->config, &element->ctx);
	return element;
}

void CG_SHUDElementKeyRoutine(void* context) {
	shudElementKey_t* el = (shudElementKey_t*)context;
	int keys;
	qboolean pressed;

	if ( !cg.snap ) return;

	keys = cg.snap->ps.stats[STAT_KEYS];
	pressed = (keys & el->keyBit) != 0;

	if ( el->invert ) pressed = !pressed;
	if ( !pressed ) return;

	el->ctx.text = el->label;
	CG_SHUDTextPrint(&el->config, &el->ctx);
}

void CG_SHUDElementKeyDestroy(void* context) {
	if (context) Z_Free(context);
}

// ── keydown factories ────────────────────────────────────────────────────

void* CG_SHUDElementKeyDownForwardCreate(const superhudConfig_t* c) { return CG_SHUDElementKeyCreate(c, KEYS_FORWARD, qfalse, "W"); }
void* CG_SHUDElementKeyDownBackCreate(const superhudConfig_t* c)    { return CG_SHUDElementKeyCreate(c, KEYS_BACK,    qfalse, "S"); }
void* CG_SHUDElementKeyDownLeftCreate(const superhudConfig_t* c)    { return CG_SHUDElementKeyCreate(c, KEYS_LEFT,    qfalse, "A"); }
void* CG_SHUDElementKeyDownRightCreate(const superhudConfig_t* c)   { return CG_SHUDElementKeyCreate(c, KEYS_RIGHT,   qfalse, "D"); }
void* CG_SHUDElementKeyDownJumpCreate(const superhudConfig_t* c)    { return CG_SHUDElementKeyCreate(c, KEYS_JUMP,    qfalse, "JUMP"); }
void* CG_SHUDElementKeyDownCrouchCreate(const superhudConfig_t* c)  { return CG_SHUDElementKeyCreate(c, KEYS_CROUCH,  qfalse, "CROUCH"); }
void* CG_SHUDElementKeyDownAttackCreate(const superhudConfig_t* c)  { return CG_SHUDElementKeyCreate(c, KEYS_ATTACK,  qfalse, "FIRE"); }
void* CG_SHUDElementKeyDownUseCreate(const superhudConfig_t* c)     { return CG_SHUDElementKeyCreate(c, KEYS_USE,     qfalse, "USE"); }
void* CG_SHUDElementKeyDownWalkCreate(const superhudConfig_t* c)    { return CG_SHUDElementKeyCreate(c, KEYS_WALK,    qfalse, "WALK"); }
void* CG_SHUDElementKeyDownGestureCreate(const superhudConfig_t* c) { return CG_SHUDElementKeyCreate(c, KEYS_GESTURE, qfalse, "GESTURE"); }

// ── keyup factories ──────────────────────────────────────────────────────

void* CG_SHUDElementKeyUpForwardCreate(const superhudConfig_t* c) { return CG_SHUDElementKeyCreate(c, KEYS_FORWARD, qtrue, "W"); }
void* CG_SHUDElementKeyUpBackCreate(const superhudConfig_t* c)    { return CG_SHUDElementKeyCreate(c, KEYS_BACK,    qtrue, "S"); }
void* CG_SHUDElementKeyUpLeftCreate(const superhudConfig_t* c)    { return CG_SHUDElementKeyCreate(c, KEYS_LEFT,    qtrue, "A"); }
void* CG_SHUDElementKeyUpRightCreate(const superhudConfig_t* c)   { return CG_SHUDElementKeyCreate(c, KEYS_RIGHT,   qtrue, "D"); }
void* CG_SHUDElementKeyUpJumpCreate(const superhudConfig_t* c)    { return CG_SHUDElementKeyCreate(c, KEYS_JUMP,    qtrue, "JUMP"); }
void* CG_SHUDElementKeyUpCrouchCreate(const superhudConfig_t* c)  { return CG_SHUDElementKeyCreate(c, KEYS_CROUCH,  qtrue, "CROUCH"); }
void* CG_SHUDElementKeyUpAttackCreate(const superhudConfig_t* c)  { return CG_SHUDElementKeyCreate(c, KEYS_ATTACK,  qtrue, "FIRE"); }
void* CG_SHUDElementKeyUpUseCreate(const superhudConfig_t* c)     { return CG_SHUDElementKeyCreate(c, KEYS_USE,     qtrue, "USE"); }
void* CG_SHUDElementKeyUpWalkCreate(const superhudConfig_t* c)    { return CG_SHUDElementKeyCreate(c, KEYS_WALK,    qtrue, "WALK"); }
void* CG_SHUDElementKeyUpGestureCreate(const superhudConfig_t* c) { return CG_SHUDElementKeyCreate(c, KEYS_GESTURE, qtrue, "GESTURE"); }

#endif // FEAT_WIRED_UI && FEAT_MOVEMENT_KEYS
