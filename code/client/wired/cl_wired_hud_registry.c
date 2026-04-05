/*
===========================================================================
cl_wired_hud_registry.c — Wired UI HUD: element registry + lifecycle

Complete registry of all SuperHUD elements with create/routine/destroy
function pointers. Auto-generated from cg_superhud_private.c.
===========================================================================
*/

#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI

// ── forward declarations for all element functions ───────────────────

extern void* CG_SHUDElementAmmoMessageCreate(const superhudConfig_t*);
extern void CG_SHUDElementAmmoMessageDestroy(void*);
extern void CG_SHUDElementAmmoMessageRoutine(void*);
extern void* CG_SHUDElementChat10Create(const superhudConfig_t*);
extern void* CG_SHUDElementChat11Create(const superhudConfig_t*);
extern void* CG_SHUDElementChat12Create(const superhudConfig_t*);
extern void* CG_SHUDElementChat13Create(const superhudConfig_t*);
extern void* CG_SHUDElementChat14Create(const superhudConfig_t*);
extern void* CG_SHUDElementChat15Create(const superhudConfig_t*);
extern void* CG_SHUDElementChat16Create(const superhudConfig_t*);
extern void* CG_SHUDElementChat1Create(const superhudConfig_t*);
extern void* CG_SHUDElementChat2Create(const superhudConfig_t*);
extern void* CG_SHUDElementChat3Create(const superhudConfig_t*);
extern void* CG_SHUDElementChat4Create(const superhudConfig_t*);
extern void* CG_SHUDElementChat5Create(const superhudConfig_t*);
extern void* CG_SHUDElementChat6Create(const superhudConfig_t*);
extern void* CG_SHUDElementChat7Create(const superhudConfig_t*);
extern void* CG_SHUDElementChat8Create(const superhudConfig_t*);
extern void* CG_SHUDElementChat9Create(const superhudConfig_t*);
extern void CG_SHUDElementChatDestroy(void*);
extern void CG_SHUDElementChatRoutine(void*);
extern void* CG_SHUDElementCreateCurrentWeapon(const superhudConfig_t*);
extern void* CG_SHUDElementCreatePlayerStatsDG(const superhudConfig_t*);
extern void* CG_SHUDElementCreatePlayerStatsDGIcon(const superhudConfig_t*);
extern void* CG_SHUDElementCreatePlayerStatsDR(const superhudConfig_t*);
extern void* CG_SHUDElementCreatePlayerStatsDRIcon(const superhudConfig_t*);
extern void* CG_SHUDElementCreatePlayerStatsDamageRatio(const superhudConfig_t*);
extern void* CG_SHUDElementDecorCreate(const superhudConfig_t*);
extern void CG_SHUDElementDecorDestroy(void*);
extern void CG_SHUDElementDecorRoutine(void*);
extern void* CG_SHUDElementFPSCreate(const superhudConfig_t*);
extern void CG_SHUDElementFPSDestroy(void*);
extern void CG_SHUDElementFPSRoutine(void*);
extern void CG_SHUDElementFlagStatusDestroy(void*);
extern void* CG_SHUDElementFlagStatusNMECreate(const superhudConfig_t*);
extern void* CG_SHUDElementFlagStatusOWNCreate(const superhudConfig_t*);
extern void CG_SHUDElementFlagStatusRoutine(void*);
extern void* CG_SHUDElementFollowMessageCreate(const superhudConfig_t*);
extern void CG_SHUDElementFollowMessageDestroy(void*);
extern void CG_SHUDElementFollowMessageRoutine(void*);
extern void* CG_SHUDElementFragMessageCreate(const superhudConfig_t*);
extern void CG_SHUDElementFragMessageDestroy(void*);
extern void CG_SHUDElementFragMessageRoutine(void*);

extern void* CG_SHUDElementMsgQueueCreate(const superhudConfig_t*);
extern void CG_SHUDElementMsgQueueDestroy(void*);
extern void CG_SHUDElementMsgQueueRoutine(void*);
extern void* CG_SHUDElementGameTimeCreate(const superhudConfig_t*);
extern void CG_SHUDElementGameTimeDestroy(void*);
extern void CG_SHUDElementGameTimeRoutine(void*);
extern void* CG_SHUDElementGameTypeCreate(const superhudConfig_t*);
extern void CG_SHUDElementGameTypeDestroy(void*);
extern void CG_SHUDElementGameTypeRoutine(void*);
extern void* CG_SHUDElementGridCreate(const superhudConfig_t*);
extern void CG_SHUDElementGridDestroy(void*);
extern void CG_SHUDElementGridRoutine(void*);
extern void* CG_SHUDElementIconCreateCurrentWeapon(const superhudConfig_t*);
extern void* CG_SHUDElementIconCreateGL(const superhudConfig_t*);
extern void* CG_SHUDElementIconCreateLG(const superhudConfig_t*);
extern void* CG_SHUDElementIconCreateMG(const superhudConfig_t*);
extern void* CG_SHUDElementIconCreatePG(const superhudConfig_t*);
extern void* CG_SHUDElementIconCreateRG(const superhudConfig_t*);
extern void* CG_SHUDElementIconCreateRL(const superhudConfig_t*);
extern void* CG_SHUDElementIconCreateSG(const superhudConfig_t*);
extern void* CG_SHUDElementItemPickupCreate(const superhudConfig_t*);
extern void CG_SHUDElementItemPickupDestroy(void*);
extern void* CG_SHUDElementItemPickupIconCreate(const superhudConfig_t*);
extern void CG_SHUDElementItemPickupIconDestroy(void*);
extern void CG_SHUDElementItemPickupIconRoutine(void*);
extern void CG_SHUDElementItemPickupRoutine(void*);
extern void* CG_SHUDElementLocalDateCreate(const superhudConfig_t*);
extern void* CG_SHUDElementLocalTimeCreate(const superhudConfig_t*);
extern void CG_SHUDElementLocalTimeDestroy(void*);
extern void CG_SHUDElementLocalTimeRoutine(void*);
extern void* CG_SHUDElementLocationCreate(const superhudConfig_t*);
extern void CG_SHUDElementLocationDestroy(void*);
extern void CG_SHUDElementLocationRoutine(void*);
extern void* CG_SHUDElementNGCreate(const superhudConfig_t*);
extern void CG_SHUDElementNGDestroy(void*);
extern void* CG_SHUDElementNGPCreate(const superhudConfig_t*);
extern void CG_SHUDElementNGPDestroy(void*);
extern void CG_SHUDElementNGPRoutine(void*);
extern void CG_SHUDElementNGRoutine(void*);
extern void CG_SHUDElementNameDestroy(void*);
extern void* CG_SHUDElementNameNMECreate(const superhudConfig_t*);
extern void* CG_SHUDElementNameOWNCreate(const superhudConfig_t*);
extern void CG_SHUDElementNameRoutine(void*);
extern void* CG_SHUDElementNetStatsCreate(const superhudConfig_t*);
extern void CG_SHUDElementNetStatsDestroy(void*);
extern void CG_SHUDElementNetStatsRoutine(void*);
extern void* CG_SHUDElementObituaries1Create(const superhudConfig_t*);
extern void* CG_SHUDElementObituaries2Create(const superhudConfig_t*);
extern void* CG_SHUDElementObituaries3Create(const superhudConfig_t*);
extern void* CG_SHUDElementObituaries4Create(const superhudConfig_t*);
extern void* CG_SHUDElementObituaries5Create(const superhudConfig_t*);
extern void* CG_SHUDElementObituaries6Create(const superhudConfig_t*);
extern void* CG_SHUDElementObituaries7Create(const superhudConfig_t*);
extern void* CG_SHUDElementObituaries8Create(const superhudConfig_t*);
extern void CG_SHUDElementObituariesDestroy(void*);
extern void CG_SHUDElementObituariesRoutine(void*);
extern void* CG_SHUDElementPlayerNameCreate(const superhudConfig_t*);
extern void CG_SHUDElementPlayerNameDestroy(void*);
extern void CG_SHUDElementPlayerNameRoutine(void*);
extern void* CG_SHUDElementPlayerSpeedCreate(const superhudConfig_t*);
extern void CG_SHUDElementPlayerSpeedDestroy(void*);
extern void CG_SHUDElementPlayerSpeedRoutine(void*);
extern void CG_SHUDElementPlayerStatsDestroy(void*);
extern void CG_SHUDElementPlayerStatsRoutine(void*);
extern void CG_SHUDElementPwDestroy(void*);
extern void* CG_SHUDElementPwIcon1Create(const superhudConfig_t*);
extern void* CG_SHUDElementPwIcon2Create(const superhudConfig_t*);
extern void* CG_SHUDElementPwIcon3Create(const superhudConfig_t*);
extern void* CG_SHUDElementPwIcon4Create(const superhudConfig_t*);
extern void* CG_SHUDElementPwIcon5Create(const superhudConfig_t*);
extern void* CG_SHUDElementPwIcon6Create(const superhudConfig_t*);
extern void* CG_SHUDElementPwIcon7Create(const superhudConfig_t*);
extern void* CG_SHUDElementPwIcon8Create(const superhudConfig_t*);
extern void CG_SHUDElementPwRoutine(void*);
extern void* CG_SHUDElementPwTime1Create(const superhudConfig_t*);
extern void* CG_SHUDElementPwTime2Create(const superhudConfig_t*);
extern void* CG_SHUDElementPwTime3Create(const superhudConfig_t*);
extern void* CG_SHUDElementPwTime4Create(const superhudConfig_t*);
extern void* CG_SHUDElementPwTime5Create(const superhudConfig_t*);
extern void* CG_SHUDElementPwTime6Create(const superhudConfig_t*);
extern void* CG_SHUDElementPwTime7Create(const superhudConfig_t*);
extern void* CG_SHUDElementPwTime8Create(const superhudConfig_t*);
extern void* CG_SHUDElementRankMessageCreate(const superhudConfig_t*);
extern void CG_SHUDElementRankMessageDestroy(void*);
extern void CG_SHUDElementRankMessageRoutine(void*);
extern void* CG_SHUDElementRewardCountCreate(const superhudConfig_t*);
extern void CG_SHUDElementRewardDestroy(void*);
extern void* CG_SHUDElementRewardIconCreate(const superhudConfig_t*);
extern void CG_SHUDElementRewardRoutine(void*);
extern void* CG_SHUDElementAwardsCreate(const superhudConfig_t*);
extern void CG_SHUDElementAwardsRoutine(void*);
extern void CG_SHUDElementAwardsDestroy(void*);
extern void* CG_SHUDElementCrosshairCreate(const superhudConfig_t*);
extern void CG_SHUDElementCrosshairRoutine(void*);
extern void CG_SHUDElementCrosshairDestroy(void*);
extern void* CG_SHUDElementStatusbarValueCreate(const superhudConfig_t*);
extern void CG_SHUDElementStatusbarValueRoutine(void*);
extern void CG_SHUDElementStatusbarValueDestroy(void*);
extern void* CG_SHUDElementStatusbarIconCreate(const superhudConfig_t*);
extern void CG_SHUDElementStatusbarIconRoutine(void*);
extern void CG_SHUDElementStatusbarIconDestroy(void*);
extern void* CG_SHUDElementStatusbarBarCreate(const superhudConfig_t*);
extern void CG_SHUDElementStatusbarBarRoutine(void*);
extern void CG_SHUDElementStatusbarBarDestroy(void*);
// old statusbar_health/armor/ammo count/icon/bar elements removed — replaced by generic bound elements
extern void CG_SHUDElementScoreDestroy(void*);
extern void* CG_SHUDElementScoreMAXCreate(const superhudConfig_t*);
extern void* CG_SHUDElementScoreNMECreate(const superhudConfig_t*);
extern void* CG_SHUDElementScoreOWNCreate(const superhudConfig_t*);
extern void CG_SHUDElementScoreRoutine(void*);
extern void* CG_SHUDElementSpecMessageCreate(const superhudConfig_t*);
extern void CG_SHUDElementSpecMessageDestroy(void*);
extern void CG_SHUDElementSpecMessageRoutine(void*);
extern void* CG_SHUDElementSpectatorsCreate(const superhudConfig_t*);
extern void CG_SHUDElementSpectatorsDestroy(void*);
extern void CG_SHUDElementSpectatorsRoutine(void*);
extern void* CG_SHUDElementTargetNameCreate(const superhudConfig_t*);
extern void CG_SHUDElementTargetNameDestroy(void*);
extern void CG_SHUDElementTargetNameRoutine(void*);
extern void* CG_SHUDElementTargetStatusCreate(const superhudConfig_t*);
extern void CG_SHUDElementTargetStatusDestroy(void*);
extern void CG_SHUDElementTargetStatusRoutine(void*);
extern void* CG_SHUDElementTeam10Create(const superhudConfig_t*);
extern void* CG_SHUDElementTeam11Create(const superhudConfig_t*);
extern void* CG_SHUDElementTeam12Create(const superhudConfig_t*);
extern void* CG_SHUDElementTeam13Create(const superhudConfig_t*);
extern void* CG_SHUDElementTeam14Create(const superhudConfig_t*);
extern void* CG_SHUDElementTeam15Create(const superhudConfig_t*);
extern void* CG_SHUDElementTeam16Create(const superhudConfig_t*);
extern void* CG_SHUDElementTeam1Create(const superhudConfig_t*);
extern void* CG_SHUDElementTeam2Create(const superhudConfig_t*);
extern void* CG_SHUDElementTeam3Create(const superhudConfig_t*);
extern void* CG_SHUDElementTeam4Create(const superhudConfig_t*);
extern void* CG_SHUDElementTeam5Create(const superhudConfig_t*);
extern void* CG_SHUDElementTeam6Create(const superhudConfig_t*);
extern void* CG_SHUDElementTeam7Create(const superhudConfig_t*);
extern void* CG_SHUDElementTeam8Create(const superhudConfig_t*);
extern void* CG_SHUDElementTeam9Create(const superhudConfig_t*);
extern void CG_SHUDElementTeamCountDestroy(void*);
extern void* CG_SHUDElementTeamCountNMECreate(const superhudConfig_t*);
extern void* CG_SHUDElementTeamCountOWNCreate(const superhudConfig_t*);
extern void CG_SHUDElementTeamCountRoutine(void*);
extern void CG_SHUDElementTeamDestroy(void*);
extern void CG_SHUDElementTeamRoutine(void*);
extern void CG_SHUDElementTempAccDestroy(void*);
extern void* CG_SHUDElementTempAccIconCreate(const superhudConfig_t*);
extern void CG_SHUDElementTempAccRoutine(void*);
extern void* CG_SHUDElementTempAccTextCreate(const superhudConfig_t*);
extern void* CG_SHUDElementVMWCreate(const superhudConfig_t*);
extern void CG_SHUDElementVMWDestroy(void*);
extern void CG_SHUDElementVMWRoutine(void*);
extern void* CG_SHUDElementWarmupInfoCreate(const superhudConfig_t*);
extern void CG_SHUDElementWarmupInfoDestroy(void*);
extern void CG_SHUDElementWarmupInfoRoutine(void*);
extern void* CG_SHUDElementWeaponListCreate(const superhudConfig_t*);
extern void CG_SHUDElementWeaponListDestroy(void*);
extern void CG_SHUDElementWeaponListRoutine(void*);
extern void* CG_SHUDElementWeaponStatsCreateGL(const superhudConfig_t*);
extern void* CG_SHUDElementWeaponStatsCreateLG(const superhudConfig_t*);
extern void* CG_SHUDElementWeaponStatsCreateMG(const superhudConfig_t*);
extern void* CG_SHUDElementWeaponStatsCreatePG(const superhudConfig_t*);
extern void* CG_SHUDElementWeaponStatsCreateRG(const superhudConfig_t*);
extern void* CG_SHUDElementWeaponStatsCreateRL(const superhudConfig_t*);
extern void* CG_SHUDElementWeaponStatsCreateSG(const superhudConfig_t*);
extern void CG_SHUDElementWeaponStatsDestroy(void*);
extern void CG_SHUDElementWeaponStatsRoutine(void*);

// ── element registry ─────────────────────────────────────────────────

typedef struct {
	const char *name;
	int         defaultVisibility;
	void*       (*create)(const superhudConfig_t*);
	void        (*routine)(void*);
	void        (*destroy)(void*);
} wiredHudElementDef_t;

static const wiredHudElementDef_t wiredHudElementDefs[] = {
	{ "!default", 0, NULL, NULL, NULL },
	{ "grid", 0, CG_SHUDElementGridCreate, CG_SHUDElementGridRoutine, CG_SHUDElementGridDestroy },
	{ "predecorate", 0, CG_SHUDElementDecorCreate, CG_SHUDElementDecorRoutine, CG_SHUDElementDecorDestroy },
	{ "ammomessage", 0, CG_SHUDElementAmmoMessageCreate, CG_SHUDElementAmmoMessageRoutine, CG_SHUDElementAmmoMessageDestroy },
	{ "attackericon", 0, NULL, NULL, NULL },
	{ "attackername", 0, NULL, NULL, NULL },
	{ "chat1", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementChat1Create, CG_SHUDElementChatRoutine, CG_SHUDElementChatDestroy },
	{ "chat2", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementChat2Create, CG_SHUDElementChatRoutine, CG_SHUDElementChatDestroy },
	{ "chat3", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementChat3Create, CG_SHUDElementChatRoutine, CG_SHUDElementChatDestroy },
	{ "chat4", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementChat4Create, CG_SHUDElementChatRoutine, CG_SHUDElementChatDestroy },
	{ "chat5", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementChat5Create, CG_SHUDElementChatRoutine, CG_SHUDElementChatDestroy },
	{ "chat6", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementChat6Create, CG_SHUDElementChatRoutine, CG_SHUDElementChatDestroy },
	{ "chat7", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementChat7Create, CG_SHUDElementChatRoutine, CG_SHUDElementChatDestroy },
	{ "chat8", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementChat8Create, CG_SHUDElementChatRoutine, CG_SHUDElementChatDestroy },
	{ "chat9", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementChat9Create, CG_SHUDElementChatRoutine, CG_SHUDElementChatDestroy },
	{ "chat10", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementChat10Create, CG_SHUDElementChatRoutine, CG_SHUDElementChatDestroy },
	{ "chat11", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementChat11Create, CG_SHUDElementChatRoutine, CG_SHUDElementChatDestroy },
	{ "chat12", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementChat12Create, CG_SHUDElementChatRoutine, CG_SHUDElementChatDestroy },
	{ "chat13", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementChat13Create, CG_SHUDElementChatRoutine, CG_SHUDElementChatDestroy },
	{ "chat14", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementChat14Create, CG_SHUDElementChatRoutine, CG_SHUDElementChatDestroy },
	{ "chat15", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementChat15Create, CG_SHUDElementChatRoutine, CG_SHUDElementChatDestroy },
	{ "chat16", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementChat16Create, CG_SHUDElementChatRoutine, CG_SHUDElementChatDestroy },
	{ "console", 0, NULL, NULL, NULL },
	{ "flagstatus_nme", SE_TEAM_ONLY, CG_SHUDElementFlagStatusNMECreate, CG_SHUDElementFlagStatusRoutine, CG_SHUDElementFlagStatusDestroy },
	{ "flagstatus_own", SE_TEAM_ONLY, CG_SHUDElementFlagStatusOWNCreate, CG_SHUDElementFlagStatusRoutine, CG_SHUDElementFlagStatusDestroy },
	{ "followmessage", 0, CG_SHUDElementFollowMessageCreate, CG_SHUDElementFollowMessageRoutine, CG_SHUDElementFollowMessageDestroy },
	{ "fps", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementFPSCreate, CG_SHUDElementFPSRoutine, CG_SHUDElementFPSDestroy },
	{ "fragmessage", 0, CG_SHUDElementFragMessageCreate, CG_SHUDElementFragMessageRoutine, CG_SHUDElementFragMessageDestroy },
	{ "gametime", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementGameTimeCreate, CG_SHUDElementGameTimeRoutine, CG_SHUDElementGameTimeDestroy },
	{ "gametype", 0, CG_SHUDElementGameTypeCreate, CG_SHUDElementGameTypeRoutine, CG_SHUDElementGameTypeDestroy },
	{ "itempickup", 0, CG_SHUDElementItemPickupCreate, CG_SHUDElementItemPickupRoutine, CG_SHUDElementItemPickupDestroy },
	{ "itempickupicon", 0, CG_SHUDElementItemPickupIconCreate, CG_SHUDElementItemPickupIconRoutine, CG_SHUDElementItemPickupIconDestroy },
	{ "itemtimers1_icons", 0, NULL, NULL, NULL },
	{ "itemtimers2_icons", 0, NULL, NULL, NULL },
	{ "itemtimers3_icons", 0, NULL, NULL, NULL },
	{ "itemtimers4_icons", 0, NULL, NULL, NULL },
	{ "itemtimers1_times", 0, NULL, NULL, NULL },
	{ "itemtimers2_times", 0, NULL, NULL, NULL },
	{ "itemtimers3_times", 0, NULL, NULL, NULL },
	{ "itemtimers4_times", 0, NULL, NULL, NULL },
#if FEAT_MOVEMENT_KEYS
	{ "keydown_attack", SE_SPECT, CG_SHUDElementKeyDownAttackCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keydown_back", SE_SPECT, CG_SHUDElementKeyDownBackCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keydown_crouch", SE_SPECT, CG_SHUDElementKeyDownCrouchCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keydown_forward", SE_SPECT, CG_SHUDElementKeyDownForwardCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keydown_gesture", SE_SPECT, CG_SHUDElementKeyDownGestureCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keydown_jump", SE_SPECT, CG_SHUDElementKeyDownJumpCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keydown_left", SE_SPECT, CG_SHUDElementKeyDownLeftCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keydown_right", SE_SPECT, CG_SHUDElementKeyDownRightCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keydown_use", SE_SPECT, CG_SHUDElementKeyDownUseCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keydown_walk", SE_SPECT, CG_SHUDElementKeyDownWalkCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keyup_attack", SE_SPECT, CG_SHUDElementKeyUpAttackCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keyup_back", SE_SPECT, CG_SHUDElementKeyUpBackCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keyup_crouch", SE_SPECT, CG_SHUDElementKeyUpCrouchCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keyup_forward", SE_SPECT, CG_SHUDElementKeyUpForwardCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keyup_gesture", SE_SPECT, CG_SHUDElementKeyUpGestureCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keyup_jump", SE_SPECT, CG_SHUDElementKeyUpJumpCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keyup_left", SE_SPECT, CG_SHUDElementKeyUpLeftCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keyup_right", SE_SPECT, CG_SHUDElementKeyUpRightCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keyup_use", SE_SPECT, CG_SHUDElementKeyUpUseCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
	{ "keyup_walk", SE_SPECT, CG_SHUDElementKeyUpWalkCreate, CG_SHUDElementKeyRoutine, CG_SHUDElementKeyDestroy },
#else
	{ "keydown_attack", 0, NULL, NULL, NULL },
	{ "keydown_back", 0, NULL, NULL, NULL },
	{ "keydown_crouch", 0, NULL, NULL, NULL },
	{ "keydown_forward", 0, NULL, NULL, NULL },
	{ "keydown_gesture", 0, NULL, NULL, NULL },
	{ "keydown_jump", 0, NULL, NULL, NULL },
	{ "keydown_left", 0, NULL, NULL, NULL },
	{ "keydown_right", 0, NULL, NULL, NULL },
	{ "keydown_use", 0, NULL, NULL, NULL },
	{ "keydown_walk", 0, NULL, NULL, NULL },
	{ "keyup_attack", 0, NULL, NULL, NULL },
	{ "keyup_back", 0, NULL, NULL, NULL },
	{ "keyup_crouch", 0, NULL, NULL, NULL },
	{ "keyup_forward", 0, NULL, NULL, NULL },
	{ "keyup_gesture", 0, NULL, NULL, NULL },
	{ "keyup_jump", 0, NULL, NULL, NULL },
	{ "keyup_left", 0, NULL, NULL, NULL },
	{ "keyup_right", 0, NULL, NULL, NULL },
	{ "keyup_use", 0, NULL, NULL, NULL },
	{ "keyup_walk", 0, NULL, NULL, NULL },
#endif
	{ "localtime", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementLocalTimeCreate, CG_SHUDElementLocalTimeRoutine, CG_SHUDElementLocalTimeDestroy },
	{ "localdate", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementLocalDateCreate, CG_SHUDElementLocalTimeRoutine, CG_SHUDElementLocalTimeDestroy },
	{ "msgqueue", 0, CG_SHUDElementMsgQueueCreate, CG_SHUDElementMsgQueueRoutine, CG_SHUDElementMsgQueueDestroy },
	{ "multiview", 0, NULL, NULL, NULL },
	{ "name_nme", 0, CG_SHUDElementNameNMECreate, CG_SHUDElementNameRoutine, CG_SHUDElementNameDestroy },
	{ "name_own", 0, CG_SHUDElementNameOWNCreate, CG_SHUDElementNameRoutine, CG_SHUDElementNameDestroy },
	{ "netgraph", SE_IM | SE_SPECT | SE_DEAD | SE_DEMO_HIDE, CG_SHUDElementNGCreate, CG_SHUDElementNGRoutine, CG_SHUDElementNGDestroy },
	{ "netgraphping", SE_IM | SE_SPECT | SE_DEAD | SE_DEMO_HIDE, CG_SHUDElementNGPCreate, CG_SHUDElementNGPRoutine, CG_SHUDElementNGPDestroy },
	{ "playerspeed", 0, CG_SHUDElementPlayerSpeedCreate, CG_SHUDElementPlayerSpeedRoutine, CG_SHUDElementPlayerSpeedDestroy },
	{ "powerup1_icon", 0, CG_SHUDElementPwIcon1Create, CG_SHUDElementPwRoutine, CG_SHUDElementPwDestroy },
	{ "powerup2_icon", 0, CG_SHUDElementPwIcon2Create, CG_SHUDElementPwRoutine, CG_SHUDElementPwDestroy },
	{ "powerup3_icon", 0, CG_SHUDElementPwIcon3Create, CG_SHUDElementPwRoutine, CG_SHUDElementPwDestroy },
	{ "powerup4_icon", 0, CG_SHUDElementPwIcon4Create, CG_SHUDElementPwRoutine, CG_SHUDElementPwDestroy },
	{ "powerup5_icon", 0, CG_SHUDElementPwIcon5Create, CG_SHUDElementPwRoutine, CG_SHUDElementPwDestroy },
	{ "powerup6_icon", 0, CG_SHUDElementPwIcon6Create, CG_SHUDElementPwRoutine, CG_SHUDElementPwDestroy },
	{ "powerup7_icon", 0, CG_SHUDElementPwIcon7Create, CG_SHUDElementPwRoutine, CG_SHUDElementPwDestroy },
	{ "powerup8_icon", 0, CG_SHUDElementPwIcon8Create, CG_SHUDElementPwRoutine, CG_SHUDElementPwDestroy },
	{ "powerup1_time", 0, CG_SHUDElementPwTime1Create, CG_SHUDElementPwRoutine, CG_SHUDElementPwDestroy },
	{ "powerup2_time", 0, CG_SHUDElementPwTime2Create, CG_SHUDElementPwRoutine, CG_SHUDElementPwDestroy },
	{ "powerup3_time", 0, CG_SHUDElementPwTime3Create, CG_SHUDElementPwRoutine, CG_SHUDElementPwDestroy },
	{ "powerup4_time", 0, CG_SHUDElementPwTime4Create, CG_SHUDElementPwRoutine, CG_SHUDElementPwDestroy },
	{ "powerup5_time", 0, CG_SHUDElementPwTime5Create, CG_SHUDElementPwRoutine, CG_SHUDElementPwDestroy },
	{ "powerup6_time", 0, CG_SHUDElementPwTime6Create, CG_SHUDElementPwRoutine, CG_SHUDElementPwDestroy },
	{ "powerup7_time", 0, CG_SHUDElementPwTime7Create, CG_SHUDElementPwRoutine, CG_SHUDElementPwDestroy },
	{ "powerup8_time", 0, CG_SHUDElementPwTime8Create, CG_SHUDElementPwRoutine, CG_SHUDElementPwDestroy },
	{ "rankmessage", 0, CG_SHUDElementRankMessageCreate, CG_SHUDElementRankMessageRoutine, CG_SHUDElementRankMessageDestroy },
	{ "recordingdemo", 0, NULL, NULL, NULL },
	{ "score_limit", 0, CG_SHUDElementScoreMAXCreate, CG_SHUDElementScoreRoutine, CG_SHUDElementScoreDestroy },
	{ "score_nme", 0, CG_SHUDElementScoreNMECreate, CG_SHUDElementScoreRoutine, CG_SHUDElementScoreDestroy },
	{ "score_own", 0, CG_SHUDElementScoreOWNCreate, CG_SHUDElementScoreRoutine, CG_SHUDElementScoreDestroy },
	{ "specmessage", SE_SPECT, CG_SHUDElementSpecMessageCreate, CG_SHUDElementSpecMessageRoutine, CG_SHUDElementSpecMessageDestroy },
	{ "spectators", SE_IM, CG_SHUDElementSpectatorsCreate, CG_SHUDElementSpectatorsRoutine, CG_SHUDElementSpectatorsDestroy },
	// old statusbar_health/armor/ammo count/icon/bar entries removed — use statusbar_value/icon/bar + bind
	{ "targetname", 0, CG_SHUDElementTargetNameCreate, CG_SHUDElementTargetNameRoutine, CG_SHUDElementTargetNameDestroy },
	{ "targetstatus", SE_TEAM_ONLY, CG_SHUDElementTargetStatusCreate, CG_SHUDElementTargetStatusRoutine, CG_SHUDElementTargetStatusDestroy },
	{ "teamcount_nme", SE_TEAM_ONLY, CG_SHUDElementTeamCountNMECreate, CG_SHUDElementTeamCountRoutine, CG_SHUDElementTeamCountDestroy },
	{ "teamcount_own", SE_TEAM_ONLY, CG_SHUDElementTeamCountOWNCreate, CG_SHUDElementTeamCountRoutine, CG_SHUDElementTeamCountDestroy },
	{ "teamicon_nme", SE_TEAM_ONLY, NULL, NULL, NULL },
	{ "teamicon_own", SE_TEAM_ONLY, NULL, NULL, NULL },
	{ "team1", SE_TEAM_ONLY, CG_SHUDElementTeam1Create, CG_SHUDElementTeamRoutine, CG_SHUDElementTeamDestroy },
	{ "team2", SE_TEAM_ONLY, CG_SHUDElementTeam2Create, CG_SHUDElementTeamRoutine, CG_SHUDElementTeamDestroy },
	{ "team3", SE_TEAM_ONLY, CG_SHUDElementTeam3Create, CG_SHUDElementTeamRoutine, CG_SHUDElementTeamDestroy },
	{ "team4", SE_TEAM_ONLY, CG_SHUDElementTeam4Create, CG_SHUDElementTeamRoutine, CG_SHUDElementTeamDestroy },
	{ "team5", SE_TEAM_ONLY, CG_SHUDElementTeam5Create, CG_SHUDElementTeamRoutine, CG_SHUDElementTeamDestroy },
	{ "team6", SE_TEAM_ONLY, CG_SHUDElementTeam6Create, CG_SHUDElementTeamRoutine, CG_SHUDElementTeamDestroy },
	{ "team7", SE_TEAM_ONLY, CG_SHUDElementTeam7Create, CG_SHUDElementTeamRoutine, CG_SHUDElementTeamDestroy },
	{ "team8", SE_TEAM_ONLY, CG_SHUDElementTeam8Create, CG_SHUDElementTeamRoutine, CG_SHUDElementTeamDestroy },
	{ "team9", SE_TEAM_ONLY, CG_SHUDElementTeam9Create, CG_SHUDElementTeamRoutine, CG_SHUDElementTeamDestroy },
	{ "team10", SE_TEAM_ONLY, CG_SHUDElementTeam10Create, CG_SHUDElementTeamRoutine, CG_SHUDElementTeamDestroy },
	{ "team11", SE_TEAM_ONLY, CG_SHUDElementTeam11Create, CG_SHUDElementTeamRoutine, CG_SHUDElementTeamDestroy },
	{ "team12", SE_TEAM_ONLY, CG_SHUDElementTeam12Create, CG_SHUDElementTeamRoutine, CG_SHUDElementTeamDestroy },
	{ "team13", SE_TEAM_ONLY, CG_SHUDElementTeam13Create, CG_SHUDElementTeamRoutine, CG_SHUDElementTeamDestroy },
	{ "team14", SE_TEAM_ONLY, CG_SHUDElementTeam14Create, CG_SHUDElementTeamRoutine, CG_SHUDElementTeamDestroy },
	{ "team15", SE_TEAM_ONLY, CG_SHUDElementTeam15Create, CG_SHUDElementTeamRoutine, CG_SHUDElementTeamDestroy },
	{ "team16", SE_TEAM_ONLY, CG_SHUDElementTeam16Create, CG_SHUDElementTeamRoutine, CG_SHUDElementTeamDestroy },
	{ "votemessagearena", 0, NULL, NULL, NULL },
	{ "votemessageworld", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementVMWCreate, CG_SHUDElementVMWRoutine, CG_SHUDElementVMWDestroy },
	{ "warmupinfo", 0, CG_SHUDElementWarmupInfoCreate, CG_SHUDElementWarmupInfoRoutine, CG_SHUDElementWarmupInfoDestroy },
	{ "weaponlist", 0, CG_SHUDElementWeaponListCreate, CG_SHUDElementWeaponListRoutine, CG_SHUDElementWeaponListDestroy },
	{ "weaponselection", 0, NULL, NULL, NULL },
	{ "weaponselectionname", 0, NULL, NULL, NULL },
	{ "chat", 0, NULL, NULL, NULL },
	{ "gameevents", 0, NULL, NULL, NULL },
	{ "team1_NME", 0, NULL, NULL, NULL },
	{ "team2_NME", 0, NULL, NULL, NULL },
	{ "team3_NME", 0, NULL, NULL, NULL },
	{ "team4_NME", 0, NULL, NULL, NULL },
	{ "team5_NME", 0, NULL, NULL, NULL },
	{ "team6_NME", 0, NULL, NULL, NULL },
	{ "team7_NME", 0, NULL, NULL, NULL },
	{ "team8_NME", 0, NULL, NULL, NULL },
	{ "rewardicons", 0, CG_SHUDElementRewardIconCreate, CG_SHUDElementRewardRoutine, CG_SHUDElementRewardDestroy },
	{ "rewardnumbers", 0, CG_SHUDElementRewardCountCreate, CG_SHUDElementRewardRoutine, CG_SHUDElementRewardDestroy },
	{ "awards", 0, CG_SHUDElementAwardsCreate, CG_SHUDElementAwardsRoutine, CG_SHUDElementAwardsDestroy },
	{ "crosshair", 0, CG_SHUDElementCrosshairCreate, CG_SHUDElementCrosshairRoutine, CG_SHUDElementCrosshairDestroy },
	{ "statusbar_value", 0, CG_SHUDElementStatusbarValueCreate, CG_SHUDElementStatusbarValueRoutine, CG_SHUDElementStatusbarValueDestroy },
	{ "statusbar_icon", 0, CG_SHUDElementStatusbarIconCreate, CG_SHUDElementStatusbarIconRoutine, CG_SHUDElementStatusbarIconDestroy },
	{ "statusbar_bar", 0, CG_SHUDElementStatusbarBarCreate, CG_SHUDElementStatusbarBarRoutine, CG_SHUDElementStatusbarBarDestroy },
	{ "obituary1", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementObituaries1Create, CG_SHUDElementObituariesRoutine, CG_SHUDElementObituariesDestroy },
	{ "obituary2", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementObituaries2Create, CG_SHUDElementObituariesRoutine, CG_SHUDElementObituariesDestroy },
	{ "obituary3", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementObituaries3Create, CG_SHUDElementObituariesRoutine, CG_SHUDElementObituariesDestroy },
	{ "obituary4", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementObituaries4Create, CG_SHUDElementObituariesRoutine, CG_SHUDElementObituariesDestroy },
	{ "obituary5", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementObituaries5Create, CG_SHUDElementObituariesRoutine, CG_SHUDElementObituariesDestroy },
	{ "obituary6", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementObituaries6Create, CG_SHUDElementObituariesRoutine, CG_SHUDElementObituariesDestroy },
	{ "obituary7", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementObituaries7Create, CG_SHUDElementObituariesRoutine, CG_SHUDElementObituariesDestroy },
	{ "obituary8", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementObituaries8Create, CG_SHUDElementObituariesRoutine, CG_SHUDElementObituariesDestroy },
	{ "location", 0, CG_SHUDElementLocationCreate, CG_SHUDElementLocationRoutine, CG_SHUDElementLocationDestroy },
	{ "tempAcc_current", SE_IM | SE_DEAD, CG_SHUDElementTempAccTextCreate, CG_SHUDElementTempAccRoutine, CG_SHUDElementTempAccDestroy },
	{ "tempAcc_icon", SE_IM | SE_DEAD, CG_SHUDElementTempAccIconCreate, CG_SHUDElementTempAccRoutine, CG_SHUDElementTempAccDestroy },
	{ "currentWeaponStats", SE_IM, CG_SHUDElementCreateCurrentWeapon, CG_SHUDElementWeaponStatsRoutine, CG_SHUDElementWeaponStatsDestroy },
	{ "weaponStats_MG", SE_IM, CG_SHUDElementWeaponStatsCreateMG, CG_SHUDElementWeaponStatsRoutine, CG_SHUDElementWeaponStatsDestroy },
	{ "weaponStats_SG", SE_IM, CG_SHUDElementWeaponStatsCreateSG, CG_SHUDElementWeaponStatsRoutine, CG_SHUDElementWeaponStatsDestroy },
	{ "weaponStats_GL", SE_IM, CG_SHUDElementWeaponStatsCreateGL, CG_SHUDElementWeaponStatsRoutine, CG_SHUDElementWeaponStatsDestroy },
	{ "weaponStats_RL", SE_IM, CG_SHUDElementWeaponStatsCreateRL, CG_SHUDElementWeaponStatsRoutine, CG_SHUDElementWeaponStatsDestroy },
	{ "weaponStats_LG", SE_IM, CG_SHUDElementWeaponStatsCreateLG, CG_SHUDElementWeaponStatsRoutine, CG_SHUDElementWeaponStatsDestroy },
	{ "weaponStats_RG", SE_IM, CG_SHUDElementWeaponStatsCreateRG, CG_SHUDElementWeaponStatsRoutine, CG_SHUDElementWeaponStatsDestroy },
	{ "weaponStats_PG", SE_IM, CG_SHUDElementWeaponStatsCreatePG, CG_SHUDElementWeaponStatsRoutine, CG_SHUDElementWeaponStatsDestroy },
	{ "currentWeaponStats_icon", SE_IM, CG_SHUDElementIconCreateCurrentWeapon, CG_SHUDElementWeaponStatsRoutine, CG_SHUDElementWeaponStatsDestroy },
	{ "weaponStats_MG_icon", SE_IM, CG_SHUDElementIconCreateMG, CG_SHUDElementWeaponStatsRoutine, CG_SHUDElementWeaponStatsDestroy },
	{ "weaponStats_SG_icon", SE_IM, CG_SHUDElementIconCreateSG, CG_SHUDElementWeaponStatsRoutine, CG_SHUDElementWeaponStatsDestroy },
	{ "weaponStats_GL_icon", SE_IM, CG_SHUDElementIconCreateGL, CG_SHUDElementWeaponStatsRoutine, CG_SHUDElementWeaponStatsDestroy },
	{ "weaponStats_RL_icon", SE_IM, CG_SHUDElementIconCreateRL, CG_SHUDElementWeaponStatsRoutine, CG_SHUDElementWeaponStatsDestroy },
	{ "weaponStats_LG_icon", SE_IM, CG_SHUDElementIconCreateLG, CG_SHUDElementWeaponStatsRoutine, CG_SHUDElementWeaponStatsDestroy },
	{ "weaponStats_RG_icon", SE_IM, CG_SHUDElementIconCreateRG, CG_SHUDElementWeaponStatsRoutine, CG_SHUDElementWeaponStatsDestroy },
	{ "weaponStats_PG_icon", SE_IM, CG_SHUDElementIconCreatePG, CG_SHUDElementWeaponStatsRoutine, CG_SHUDElementWeaponStatsDestroy },
	{ "playerStats_DG", SE_IM, CG_SHUDElementCreatePlayerStatsDG, CG_SHUDElementPlayerStatsRoutine, CG_SHUDElementPlayerStatsDestroy },
	{ "playerStats_DR", SE_IM, CG_SHUDElementCreatePlayerStatsDR, CG_SHUDElementPlayerStatsRoutine, CG_SHUDElementPlayerStatsDestroy },
	{ "playerStats_DG_icon", SE_IM, CG_SHUDElementCreatePlayerStatsDGIcon, CG_SHUDElementPlayerStatsRoutine, CG_SHUDElementPlayerStatsDestroy },
	{ "playerStats_DR_icon", SE_IM, CG_SHUDElementCreatePlayerStatsDRIcon, CG_SHUDElementPlayerStatsRoutine, CG_SHUDElementPlayerStatsDestroy },
	{ "playerStats_damageRatio", SE_IM, CG_SHUDElementCreatePlayerStatsDamageRatio, CG_SHUDElementPlayerStatsRoutine, CG_SHUDElementPlayerStatsDestroy },
	{ "player_name", 0, CG_SHUDElementPlayerNameCreate, CG_SHUDElementPlayerNameRoutine, CG_SHUDElementPlayerNameDestroy },
	{ "postdecorate", 0, CG_SHUDElementDecorCreate, CG_SHUDElementDecorRoutine, CG_SHUDElementDecorDestroy },
	{ "netstats", SE_IM | SE_SPECT | SE_DEAD, CG_SHUDElementNetStatsCreate, CG_SHUDElementNetStatsRoutine, CG_SHUDElementNetStatsDestroy },
	{ NULL, 0, NULL, NULL, NULL }
};

// ── active element list ──────────────────────────────────────────────

#define WIRED_HUD_MAX_ACTIVE_ELEMENTS  256

typedef struct {
	const char *name;
	void       *context;
	void       (*routine)(void*);
	void       (*destroy)(void*);
	int         visibility;
	int         order;
	qboolean    active;
} wiredHudActiveElement_t;

static wiredHudActiveElement_t wired_hudElements[WIRED_HUD_MAX_ACTIVE_ELEMENTS];
static int wired_hudElementCount = 0;

// ── public API ───────────────────────────────────────────────────────

const wiredHudElementDef_t *WiredHud_FindElementDef( const char *name ) {
	int i;
	for ( i = 0; wiredHudElementDefs[i].name; i++ ) {
		if ( !Q_stricmp( wiredHudElementDefs[i].name, name ) ) {
			return &wiredHudElementDefs[i];
		}
	}
	return NULL;
}

qboolean WiredHud_CreateElement( const char *name, const superhudConfig_t *config ) {
	const wiredHudElementDef_t *def;
	wiredHudActiveElement_t *elem;
	void *ctx;

	if ( wired_hudElementCount >= WIRED_HUD_MAX_ACTIVE_ELEMENTS ) {
		Com_Printf( S_COLOR_YELLOW "WiredHud: too many active elements\n" );
		return qfalse;
	}

	def = WiredHud_FindElementDef( name );
	if ( !def || !def->create ) {
		Com_DPrintf( "WiredHud: unknown element '%s'\n", name );
		return qfalse;
	}

	ctx = def->create( config );
	if ( !ctx ) return qfalse;

	elem = &wired_hudElements[wired_hudElementCount++];
	elem->name       = def->name;
	elem->context     = ctx;
	elem->routine     = def->routine;
	elem->destroy     = def->destroy;
	elem->visibility  = config->visflags.isSet ? config->visflags.value : def->defaultVisibility;
	elem->order       = wired_hudElementCount;
	elem->active      = qtrue;

	return qtrue;
}

void WiredHud_DestroyAllElements( void ) {
	int i;
	for ( i = 0; i < wired_hudElementCount; i++ ) {
		if ( wired_hudElements[i].active && wired_hudElements[i].destroy && wired_hudElements[i].context ) {
			wired_hudElements[i].destroy( wired_hudElements[i].context );
		}
		wired_hudElements[i].active = qfalse;
		wired_hudElements[i].context = NULL;
	}
	wired_hudElementCount = 0;
}

void WiredHud_RenderElements( void ) {
	int i;
	qboolean is_dead, is_intermission, is_team_game, is_spectator, is_scores;
	int vflags;
	qboolean skip;

	is_dead        = wiredHud->predictedPlayerState.pm_type == PM_DEAD;
	is_intermission = wiredHud->predictedPlayerState.pm_type == PM_INTERMISSION;
	is_team_game   = wiredHud->gametype >= GT_TDM;
	is_spectator   = wired_IsSpectator();
	is_scores      = wiredHud->showScores;

	for ( i = 0; i < wired_hudElementCount; i++ ) {
		wiredHudActiveElement_t *elem = &wired_hudElements[i];
		if ( !elem->active || !elem->routine || !elem->context ) continue;

		vflags = elem->visibility;

		skip = (!(vflags & SE_IM) && is_intermission) ||
		       ((vflags & SE_TEAM_ONLY) && !is_team_game) ||
		       (!(vflags & SE_DEAD) && is_dead) ||
		       (!(vflags & SE_SPECT) && is_spectator) ||
		       ((vflags & SE_SCORES_HIDE) && is_scores) ||
		       ((vflags & SE_DEMO_HIDE) && wiredHud->demoPlayback);

		if ( !skip ) {
			elem->routine( elem->context );
		}
	}
}

int WiredHud_GetElementCount( void ) {
	return wired_hudElementCount;
}

#endif // FEAT_WIRED_UI
