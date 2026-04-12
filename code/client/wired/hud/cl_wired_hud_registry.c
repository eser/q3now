/*
===========================================================================
cl_wired_hud_registry.c — Wired UI HUD: element registry + lifecycle

Complete registry of all ModernHUD elements with create/routine/destroy
function pointers. Auto-generated from cg_modernhud_private.c.
===========================================================================
*/

#include "../../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI

// ── forward declarations for all element functions ───────────────────

extern void* CG_ModernHUDElementAmmoMessageCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementAmmoMessageDestroy(void*);
extern void CG_ModernHUDElementAmmoMessageRoutine(void*);
extern void* CG_ModernHUDElementAudioWaveformCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementAudioWaveformRoutine(void*);
extern void CG_ModernHUDElementAudioWaveformDestroy(void*);
extern void* CG_ModernHUDElementChat10Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementChat11Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementChat12Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementChat13Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementChat14Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementChat15Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementChat16Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementChat1Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementChat2Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementChat3Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementChat4Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementChat5Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementChat6Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementChat7Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementChat8Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementChat9Create(const modernhudConfig_t*);
extern void CG_ModernHUDElementChatDestroy(void*);
extern void CG_ModernHUDElementChatRoutine(void*);
extern void* CG_ModernHUDElementCreateCurrentWeapon(const modernhudConfig_t*);
extern void* CG_ModernHUDElementCreatePlayerStatsDG(const modernhudConfig_t*);
extern void* CG_ModernHUDElementCreatePlayerStatsDGIcon(const modernhudConfig_t*);
extern void* CG_ModernHUDElementCreatePlayerStatsDR(const modernhudConfig_t*);
extern void* CG_ModernHUDElementCreatePlayerStatsDRIcon(const modernhudConfig_t*);
extern void* CG_ModernHUDElementCreatePlayerStatsDamageRatio(const modernhudConfig_t*);
extern void* CG_ModernHUDElementDecorCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementDecorDestroy(void*);
extern void CG_ModernHUDElementDecorRoutine(void*);
extern void* CG_ModernHUDElementFPSCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementFPSDestroy(void*);
extern void CG_ModernHUDElementFPSRoutine(void*);
extern void CG_ModernHUDElementFlagStatusDestroy(void*);
extern void* CG_ModernHUDElementFlagStatusNMECreate(const modernhudConfig_t*);
extern void* CG_ModernHUDElementFlagStatusOWNCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementFlagStatusRoutine(void*);
extern void* CG_ModernHUDElementFollowMessageCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementFollowMessageDestroy(void*);
extern void CG_ModernHUDElementFollowMessageRoutine(void*);
extern void* CG_ModernHUDElementFragMessageCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementFragMessageDestroy(void*);
extern void CG_ModernHUDElementFragMessageRoutine(void*);

extern void* CG_ModernHUDElementMsgQueueCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementMsgQueueDestroy(void*);
extern void CG_ModernHUDElementMsgQueueRoutine(void*);
extern void* CG_ModernHUDElementGameTimeCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementGameTimeDestroy(void*);
extern void CG_ModernHUDElementGameTimeRoutine(void*);
extern void* CG_ModernHUDElementGameTypeCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementGameTypeDestroy(void*);
extern void CG_ModernHUDElementGameTypeRoutine(void*);
extern void* CG_ModernHUDElementGridCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementGridDestroy(void*);
extern void CG_ModernHUDElementGridRoutine(void*);
extern void* CG_ModernHUDElementIconCreateCurrentWeapon(const modernhudConfig_t*);
extern void* CG_ModernHUDElementIconCreateGL(const modernhudConfig_t*);
extern void* CG_ModernHUDElementIconCreateLG(const modernhudConfig_t*);
extern void* CG_ModernHUDElementIconCreateMG(const modernhudConfig_t*);
extern void* CG_ModernHUDElementIconCreatePG(const modernhudConfig_t*);
extern void* CG_ModernHUDElementIconCreateRG(const modernhudConfig_t*);
extern void* CG_ModernHUDElementIconCreateRL(const modernhudConfig_t*);
extern void* CG_ModernHUDElementIconCreateSG(const modernhudConfig_t*);
extern void* CG_ModernHUDElementItemPickupCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementItemPickupDestroy(void*);
extern void* CG_ModernHUDElementItemPickupIconCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementItemPickupIconDestroy(void*);
extern void CG_ModernHUDElementItemPickupIconRoutine(void*);
extern void CG_ModernHUDElementItemPickupRoutine(void*);
extern void* CG_ModernHUDElementLocalDateCreate(const modernhudConfig_t*);
extern void* CG_ModernHUDElementLocalTimeCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementLocalTimeDestroy(void*);
extern void CG_ModernHUDElementLocalTimeRoutine(void*);
extern void* CG_ModernHUDElementLocationCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementLocationDestroy(void*);
extern void CG_ModernHUDElementLocationRoutine(void*);
extern void* CG_ModernHUDElementNGCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementNGDestroy(void*);
extern void* CG_ModernHUDElementNGPCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementNGPDestroy(void*);
extern void CG_ModernHUDElementNGPRoutine(void*);
extern void CG_ModernHUDElementNGRoutine(void*);
extern void CG_ModernHUDElementNameDestroy(void*);
extern void* CG_ModernHUDElementNameNMECreate(const modernhudConfig_t*);
extern void* CG_ModernHUDElementNameOWNCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementNameRoutine(void*);
extern void* CG_ModernHUDElementNetStatsCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementNetStatsDestroy(void*);
extern void CG_ModernHUDElementNetStatsRoutine(void*);
extern void* CG_ModernHUDElementObituaries1Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementObituaries2Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementObituaries3Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementObituaries4Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementObituaries5Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementObituaries6Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementObituaries7Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementObituaries8Create(const modernhudConfig_t*);
extern void CG_ModernHUDElementObituariesDestroy(void*);
extern void CG_ModernHUDElementObituariesRoutine(void*);
extern void* CG_ModernHUDElementPlayerNameCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementPlayerNameDestroy(void*);
extern void CG_ModernHUDElementPlayerNameRoutine(void*);
extern void* CG_ModernHUDElementPlayerSpeedCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementPlayerSpeedDestroy(void*);
extern void CG_ModernHUDElementPlayerSpeedRoutine(void*);
extern void CG_ModernHUDElementPlayerStatsDestroy(void*);
extern void CG_ModernHUDElementPlayerStatsRoutine(void*);
extern void CG_ModernHUDElementPwDestroy(void*);
extern void* CG_ModernHUDElementPwIcon1Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementPwIcon2Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementPwIcon3Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementPwIcon4Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementPwIcon5Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementPwIcon6Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementPwIcon7Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementPwIcon8Create(const modernhudConfig_t*);
extern void CG_ModernHUDElementPwRoutine(void*);
extern void* CG_ModernHUDElementPwTime1Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementPwTime2Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementPwTime3Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementPwTime4Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementPwTime5Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementPwTime6Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementPwTime7Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementPwTime8Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementRankMessageCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementRankMessageDestroy(void*);
extern void CG_ModernHUDElementRankMessageRoutine(void*);
extern void* CG_ModernHUDElementRewardCountCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementRewardDestroy(void*);
extern void* CG_ModernHUDElementRewardIconCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementRewardRoutine(void*);
extern void* CG_ModernHUDElementAwardsCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementAwardsRoutine(void*);
extern void CG_ModernHUDElementAwardsDestroy(void*);
extern void* CG_ModernHUDElementCrosshairCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementCrosshairRoutine(void*);
extern void CG_ModernHUDElementCrosshairDestroy(void*);
extern void* CG_ModernHUDElementStatusbarValueCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementStatusbarValueRoutine(void*);
extern void CG_ModernHUDElementStatusbarValueDestroy(void*);
extern void* CG_ModernHUDElementStatusbarIconCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementStatusbarIconRoutine(void*);
extern void CG_ModernHUDElementStatusbarIconDestroy(void*);
extern void* CG_ModernHUDElementStatusbarBarCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementStatusbarBarRoutine(void*);
extern void CG_ModernHUDElementStatusbarBarDestroy(void*);
// old statusbar_health/armor/ammo count/icon/bar elements removed — replaced by generic bound elements
extern void CG_ModernHUDElementScoreDestroy(void*);
extern void* CG_ModernHUDElementScoreMAXCreate(const modernhudConfig_t*);
extern void* CG_ModernHUDElementScoreNMECreate(const modernhudConfig_t*);
extern void* CG_ModernHUDElementScoreOWNCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementScoreRoutine(void*);
extern void* CG_ModernHUDElementSpecMessageCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementSpecMessageDestroy(void*);
extern void CG_ModernHUDElementSpecMessageRoutine(void*);
extern void* CG_ModernHUDElementBotDirectivesCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementBotDirectivesDestroy(void*);
extern void CG_ModernHUDElementBotDirectivesRoutine(void*);
extern void* CG_ModernHUDElementSpectatorsCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementSpectatorsDestroy(void*);
extern void CG_ModernHUDElementSpectatorsRoutine(void*);
extern void* CG_ModernHUDElementTargetNameCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementTargetNameDestroy(void*);
extern void CG_ModernHUDElementTargetNameRoutine(void*);
extern void* CG_ModernHUDElementTargetStatusCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementTargetStatusDestroy(void*);
extern void CG_ModernHUDElementTargetStatusRoutine(void*);
extern void* CG_ModernHUDElementTeam10Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementTeam11Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementTeam12Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementTeam13Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementTeam14Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementTeam15Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementTeam16Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementTeam1Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementTeam2Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementTeam3Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementTeam4Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementTeam5Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementTeam6Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementTeam7Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementTeam8Create(const modernhudConfig_t*);
extern void* CG_ModernHUDElementTeam9Create(const modernhudConfig_t*);
extern void CG_ModernHUDElementTeamCountDestroy(void*);
extern void* CG_ModernHUDElementTeamCountNMECreate(const modernhudConfig_t*);
extern void* CG_ModernHUDElementTeamCountOWNCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementTeamCountRoutine(void*);
extern void CG_ModernHUDElementTeamDestroy(void*);
extern void CG_ModernHUDElementTeamRoutine(void*);
extern void CG_ModernHUDElementTempAccDestroy(void*);
extern void* CG_ModernHUDElementTempAccIconCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementTempAccRoutine(void*);
extern void* CG_ModernHUDElementTempAccTextCreate(const modernhudConfig_t*);
extern void* CG_ModernHUDElementVMWCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementVMWDestroy(void*);
extern void CG_ModernHUDElementVMWRoutine(void*);
extern void* CG_ModernHUDElementWarmupInfoCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementWarmupInfoDestroy(void*);
extern void CG_ModernHUDElementWarmupInfoRoutine(void*);
extern void* CG_ModernHUDElementWeaponListCreate(const modernhudConfig_t*);
extern void CG_ModernHUDElementWeaponListDestroy(void*);
extern void CG_ModernHUDElementWeaponListRoutine(void*);
extern void* CG_ModernHUDElementWeaponStatsCreateGL(const modernhudConfig_t*);
extern void* CG_ModernHUDElementWeaponStatsCreateLG(const modernhudConfig_t*);
extern void* CG_ModernHUDElementWeaponStatsCreateMG(const modernhudConfig_t*);
extern void* CG_ModernHUDElementWeaponStatsCreatePG(const modernhudConfig_t*);
extern void* CG_ModernHUDElementWeaponStatsCreateRG(const modernhudConfig_t*);
extern void* CG_ModernHUDElementWeaponStatsCreateRL(const modernhudConfig_t*);
extern void* CG_ModernHUDElementWeaponStatsCreateSG(const modernhudConfig_t*);
extern void CG_ModernHUDElementWeaponStatsDestroy(void*);
extern void CG_ModernHUDElementWeaponStatsRoutine(void*);

// ── element registry ─────────────────────────────────────────────────

typedef struct {
	const char *name;
	int         defaultVisibility;
	void*       (*create)(const modernhudConfig_t*);
	void        (*routine)(void*);
	void        (*destroy)(void*);
} wiredHudElementDef_t;

static const wiredHudElementDef_t wiredHudElementDefs[] = {
	{ "!default", 0, NULL, NULL, NULL },
	{ "grid", 0, CG_ModernHUDElementGridCreate, CG_ModernHUDElementGridRoutine, CG_ModernHUDElementGridDestroy },
	{ "predecorate", 0, CG_ModernHUDElementDecorCreate, CG_ModernHUDElementDecorRoutine, CG_ModernHUDElementDecorDestroy },
	{ "ammomessage", 0, CG_ModernHUDElementAmmoMessageCreate, CG_ModernHUDElementAmmoMessageRoutine, CG_ModernHUDElementAmmoMessageDestroy },
	{ "audio_waveform", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementAudioWaveformCreate, CG_ModernHUDElementAudioWaveformRoutine, CG_ModernHUDElementAudioWaveformDestroy },
	{ "attackericon", 0, NULL, NULL, NULL },
	{ "attackername", 0, NULL, NULL, NULL },
	{ "chat1", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChat1Create, CG_ModernHUDElementChatRoutine, CG_ModernHUDElementChatDestroy },
	{ "chat2", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChat2Create, CG_ModernHUDElementChatRoutine, CG_ModernHUDElementChatDestroy },
	{ "chat3", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChat3Create, CG_ModernHUDElementChatRoutine, CG_ModernHUDElementChatDestroy },
	{ "chat4", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChat4Create, CG_ModernHUDElementChatRoutine, CG_ModernHUDElementChatDestroy },
	{ "chat5", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChat5Create, CG_ModernHUDElementChatRoutine, CG_ModernHUDElementChatDestroy },
	{ "chat6", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChat6Create, CG_ModernHUDElementChatRoutine, CG_ModernHUDElementChatDestroy },
	{ "chat7", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChat7Create, CG_ModernHUDElementChatRoutine, CG_ModernHUDElementChatDestroy },
	{ "chat8", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChat8Create, CG_ModernHUDElementChatRoutine, CG_ModernHUDElementChatDestroy },
	{ "chat9", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChat9Create, CG_ModernHUDElementChatRoutine, CG_ModernHUDElementChatDestroy },
	{ "chat10", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChat10Create, CG_ModernHUDElementChatRoutine, CG_ModernHUDElementChatDestroy },
	{ "chat11", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChat11Create, CG_ModernHUDElementChatRoutine, CG_ModernHUDElementChatDestroy },
	{ "chat12", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChat12Create, CG_ModernHUDElementChatRoutine, CG_ModernHUDElementChatDestroy },
	{ "chat13", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChat13Create, CG_ModernHUDElementChatRoutine, CG_ModernHUDElementChatDestroy },
	{ "chat14", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChat14Create, CG_ModernHUDElementChatRoutine, CG_ModernHUDElementChatDestroy },
	{ "chat15", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChat15Create, CG_ModernHUDElementChatRoutine, CG_ModernHUDElementChatDestroy },
	{ "chat16", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChat16Create, CG_ModernHUDElementChatRoutine, CG_ModernHUDElementChatDestroy },
	{ "console", 0, NULL, NULL, NULL },
	{ "flagstatus_nme", SE_SIDES_ONLY, CG_ModernHUDElementFlagStatusNMECreate, CG_ModernHUDElementFlagStatusRoutine, CG_ModernHUDElementFlagStatusDestroy },
	{ "flagstatus_own", SE_SIDES_ONLY, CG_ModernHUDElementFlagStatusOWNCreate, CG_ModernHUDElementFlagStatusRoutine, CG_ModernHUDElementFlagStatusDestroy },
	{ "followmessage", 0, CG_ModernHUDElementFollowMessageCreate, CG_ModernHUDElementFollowMessageRoutine, CG_ModernHUDElementFollowMessageDestroy },
	{ "fps", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementFPSCreate, CG_ModernHUDElementFPSRoutine, CG_ModernHUDElementFPSDestroy },
	{ "fragmessage", 0, CG_ModernHUDElementFragMessageCreate, CG_ModernHUDElementFragMessageRoutine, CG_ModernHUDElementFragMessageDestroy },
	{ "gametime", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementGameTimeCreate, CG_ModernHUDElementGameTimeRoutine, CG_ModernHUDElementGameTimeDestroy },
	{ "gametype", 0, CG_ModernHUDElementGameTypeCreate, CG_ModernHUDElementGameTypeRoutine, CG_ModernHUDElementGameTypeDestroy },
	{ "itempickup", 0, CG_ModernHUDElementItemPickupCreate, CG_ModernHUDElementItemPickupRoutine, CG_ModernHUDElementItemPickupDestroy },
	{ "itempickupicon", 0, CG_ModernHUDElementItemPickupIconCreate, CG_ModernHUDElementItemPickupIconRoutine, CG_ModernHUDElementItemPickupIconDestroy },
	{ "itemtimers1_icons", 0, NULL, NULL, NULL },
	{ "itemtimers2_icons", 0, NULL, NULL, NULL },
	{ "itemtimers3_icons", 0, NULL, NULL, NULL },
	{ "itemtimers4_icons", 0, NULL, NULL, NULL },
	{ "itemtimers1_times", 0, NULL, NULL, NULL },
	{ "itemtimers2_times", 0, NULL, NULL, NULL },
	{ "itemtimers3_times", 0, NULL, NULL, NULL },
	{ "itemtimers4_times", 0, NULL, NULL, NULL },
#if FEAT_MOVEMENT_KEYS
	{ "keydown_attack", SE_SPECT, CG_ModernHUDElementKeyDownAttackCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keydown_back", SE_SPECT, CG_ModernHUDElementKeyDownBackCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keydown_crouch", SE_SPECT, CG_ModernHUDElementKeyDownCrouchCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keydown_forward", SE_SPECT, CG_ModernHUDElementKeyDownForwardCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keydown_gesture", SE_SPECT, CG_ModernHUDElementKeyDownGestureCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keydown_jump", SE_SPECT, CG_ModernHUDElementKeyDownJumpCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keydown_left", SE_SPECT, CG_ModernHUDElementKeyDownLeftCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keydown_right", SE_SPECT, CG_ModernHUDElementKeyDownRightCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keydown_use", SE_SPECT, CG_ModernHUDElementKeyDownUseCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keydown_walk", SE_SPECT, CG_ModernHUDElementKeyDownWalkCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keyup_attack", SE_SPECT, CG_ModernHUDElementKeyUpAttackCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keyup_back", SE_SPECT, CG_ModernHUDElementKeyUpBackCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keyup_crouch", SE_SPECT, CG_ModernHUDElementKeyUpCrouchCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keyup_forward", SE_SPECT, CG_ModernHUDElementKeyUpForwardCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keyup_gesture", SE_SPECT, CG_ModernHUDElementKeyUpGestureCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keyup_jump", SE_SPECT, CG_ModernHUDElementKeyUpJumpCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keyup_left", SE_SPECT, CG_ModernHUDElementKeyUpLeftCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keyup_right", SE_SPECT, CG_ModernHUDElementKeyUpRightCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keyup_use", SE_SPECT, CG_ModernHUDElementKeyUpUseCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
	{ "keyup_walk", SE_SPECT, CG_ModernHUDElementKeyUpWalkCreate, CG_ModernHUDElementKeyRoutine, CG_ModernHUDElementKeyDestroy },
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
	{ "localtime", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementLocalTimeCreate, CG_ModernHUDElementLocalTimeRoutine, CG_ModernHUDElementLocalTimeDestroy },
	{ "localdate", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementLocalDateCreate, CG_ModernHUDElementLocalTimeRoutine, CG_ModernHUDElementLocalTimeDestroy },
	{ "msgqueue", 0, CG_ModernHUDElementMsgQueueCreate, CG_ModernHUDElementMsgQueueRoutine, CG_ModernHUDElementMsgQueueDestroy },
	{ "multiview", 0, NULL, NULL, NULL },
	{ "name_nme", 0, CG_ModernHUDElementNameNMECreate, CG_ModernHUDElementNameRoutine, CG_ModernHUDElementNameDestroy },
	{ "name_own", 0, CG_ModernHUDElementNameOWNCreate, CG_ModernHUDElementNameRoutine, CG_ModernHUDElementNameDestroy },
	{ "netgraph", SE_IM | SE_SPECT | SE_DEAD | SE_DEMO_HIDE, CG_ModernHUDElementNGCreate, CG_ModernHUDElementNGRoutine, CG_ModernHUDElementNGDestroy },
	{ "netgraphping", SE_IM | SE_SPECT | SE_DEAD | SE_DEMO_HIDE, CG_ModernHUDElementNGPCreate, CG_ModernHUDElementNGPRoutine, CG_ModernHUDElementNGPDestroy },
	{ "playerspeed", 0, CG_ModernHUDElementPlayerSpeedCreate, CG_ModernHUDElementPlayerSpeedRoutine, CG_ModernHUDElementPlayerSpeedDestroy },
	{ "powerup1_icon", 0, CG_ModernHUDElementPwIcon1Create, CG_ModernHUDElementPwRoutine, CG_ModernHUDElementPwDestroy },
	{ "powerup2_icon", 0, CG_ModernHUDElementPwIcon2Create, CG_ModernHUDElementPwRoutine, CG_ModernHUDElementPwDestroy },
	{ "powerup3_icon", 0, CG_ModernHUDElementPwIcon3Create, CG_ModernHUDElementPwRoutine, CG_ModernHUDElementPwDestroy },
	{ "powerup4_icon", 0, CG_ModernHUDElementPwIcon4Create, CG_ModernHUDElementPwRoutine, CG_ModernHUDElementPwDestroy },
	{ "powerup5_icon", 0, CG_ModernHUDElementPwIcon5Create, CG_ModernHUDElementPwRoutine, CG_ModernHUDElementPwDestroy },
	{ "powerup6_icon", 0, CG_ModernHUDElementPwIcon6Create, CG_ModernHUDElementPwRoutine, CG_ModernHUDElementPwDestroy },
	{ "powerup7_icon", 0, CG_ModernHUDElementPwIcon7Create, CG_ModernHUDElementPwRoutine, CG_ModernHUDElementPwDestroy },
	{ "powerup8_icon", 0, CG_ModernHUDElementPwIcon8Create, CG_ModernHUDElementPwRoutine, CG_ModernHUDElementPwDestroy },
	{ "powerup1_time", 0, CG_ModernHUDElementPwTime1Create, CG_ModernHUDElementPwRoutine, CG_ModernHUDElementPwDestroy },
	{ "powerup2_time", 0, CG_ModernHUDElementPwTime2Create, CG_ModernHUDElementPwRoutine, CG_ModernHUDElementPwDestroy },
	{ "powerup3_time", 0, CG_ModernHUDElementPwTime3Create, CG_ModernHUDElementPwRoutine, CG_ModernHUDElementPwDestroy },
	{ "powerup4_time", 0, CG_ModernHUDElementPwTime4Create, CG_ModernHUDElementPwRoutine, CG_ModernHUDElementPwDestroy },
	{ "powerup5_time", 0, CG_ModernHUDElementPwTime5Create, CG_ModernHUDElementPwRoutine, CG_ModernHUDElementPwDestroy },
	{ "powerup6_time", 0, CG_ModernHUDElementPwTime6Create, CG_ModernHUDElementPwRoutine, CG_ModernHUDElementPwDestroy },
	{ "powerup7_time", 0, CG_ModernHUDElementPwTime7Create, CG_ModernHUDElementPwRoutine, CG_ModernHUDElementPwDestroy },
	{ "powerup8_time", 0, CG_ModernHUDElementPwTime8Create, CG_ModernHUDElementPwRoutine, CG_ModernHUDElementPwDestroy },
	{ "rankmessage", 0, CG_ModernHUDElementRankMessageCreate, CG_ModernHUDElementRankMessageRoutine, CG_ModernHUDElementRankMessageDestroy },
	{ "recordingdemo", 0, NULL, NULL, NULL },
	{ "score_limit", 0, CG_ModernHUDElementScoreMAXCreate, CG_ModernHUDElementScoreRoutine, CG_ModernHUDElementScoreDestroy },
	{ "score_nme", 0, CG_ModernHUDElementScoreNMECreate, CG_ModernHUDElementScoreRoutine, CG_ModernHUDElementScoreDestroy },
	{ "score_own", 0, CG_ModernHUDElementScoreOWNCreate, CG_ModernHUDElementScoreRoutine, CG_ModernHUDElementScoreDestroy },
	{ "specmessage", SE_SPECT, CG_ModernHUDElementSpecMessageCreate, CG_ModernHUDElementSpecMessageRoutine, CG_ModernHUDElementSpecMessageDestroy },
	{ "botdirectives", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementBotDirectivesCreate, CG_ModernHUDElementBotDirectivesRoutine, CG_ModernHUDElementBotDirectivesDestroy },
	{ "spectators", SE_IM, CG_ModernHUDElementSpectatorsCreate, CG_ModernHUDElementSpectatorsRoutine, CG_ModernHUDElementSpectatorsDestroy },
	// old statusbar_health/armor/ammo count/icon/bar entries removed — use statusbar_value/icon/bar + bind
	{ "targetname", 0, CG_ModernHUDElementTargetNameCreate, CG_ModernHUDElementTargetNameRoutine, CG_ModernHUDElementTargetNameDestroy },
	{ "targetstatus", SE_SIDES_ONLY, CG_ModernHUDElementTargetStatusCreate, CG_ModernHUDElementTargetStatusRoutine, CG_ModernHUDElementTargetStatusDestroy },
	{ "teamcount_nme", SE_SIDES_ONLY, CG_ModernHUDElementTeamCountNMECreate, CG_ModernHUDElementTeamCountRoutine, CG_ModernHUDElementTeamCountDestroy },
	{ "teamcount_own", SE_SIDES_ONLY, CG_ModernHUDElementTeamCountOWNCreate, CG_ModernHUDElementTeamCountRoutine, CG_ModernHUDElementTeamCountDestroy },
	{ "teamicon_nme", SE_SIDES_ONLY, NULL, NULL, NULL },
	{ "teamicon_own", SE_SIDES_ONLY, NULL, NULL, NULL },
	{ "team1", SE_SIDES_ONLY, CG_ModernHUDElementTeam1Create, CG_ModernHUDElementTeamRoutine, CG_ModernHUDElementTeamDestroy },
	{ "team2", SE_SIDES_ONLY, CG_ModernHUDElementTeam2Create, CG_ModernHUDElementTeamRoutine, CG_ModernHUDElementTeamDestroy },
	{ "team3", SE_SIDES_ONLY, CG_ModernHUDElementTeam3Create, CG_ModernHUDElementTeamRoutine, CG_ModernHUDElementTeamDestroy },
	{ "team4", SE_SIDES_ONLY, CG_ModernHUDElementTeam4Create, CG_ModernHUDElementTeamRoutine, CG_ModernHUDElementTeamDestroy },
	{ "team5", SE_SIDES_ONLY, CG_ModernHUDElementTeam5Create, CG_ModernHUDElementTeamRoutine, CG_ModernHUDElementTeamDestroy },
	{ "team6", SE_SIDES_ONLY, CG_ModernHUDElementTeam6Create, CG_ModernHUDElementTeamRoutine, CG_ModernHUDElementTeamDestroy },
	{ "team7", SE_SIDES_ONLY, CG_ModernHUDElementTeam7Create, CG_ModernHUDElementTeamRoutine, CG_ModernHUDElementTeamDestroy },
	{ "team8", SE_SIDES_ONLY, CG_ModernHUDElementTeam8Create, CG_ModernHUDElementTeamRoutine, CG_ModernHUDElementTeamDestroy },
	{ "team9", SE_SIDES_ONLY, CG_ModernHUDElementTeam9Create, CG_ModernHUDElementTeamRoutine, CG_ModernHUDElementTeamDestroy },
	{ "team10", SE_SIDES_ONLY, CG_ModernHUDElementTeam10Create, CG_ModernHUDElementTeamRoutine, CG_ModernHUDElementTeamDestroy },
	{ "team11", SE_SIDES_ONLY, CG_ModernHUDElementTeam11Create, CG_ModernHUDElementTeamRoutine, CG_ModernHUDElementTeamDestroy },
	{ "team12", SE_SIDES_ONLY, CG_ModernHUDElementTeam12Create, CG_ModernHUDElementTeamRoutine, CG_ModernHUDElementTeamDestroy },
	{ "team13", SE_SIDES_ONLY, CG_ModernHUDElementTeam13Create, CG_ModernHUDElementTeamRoutine, CG_ModernHUDElementTeamDestroy },
	{ "team14", SE_SIDES_ONLY, CG_ModernHUDElementTeam14Create, CG_ModernHUDElementTeamRoutine, CG_ModernHUDElementTeamDestroy },
	{ "team15", SE_SIDES_ONLY, CG_ModernHUDElementTeam15Create, CG_ModernHUDElementTeamRoutine, CG_ModernHUDElementTeamDestroy },
	{ "team16", SE_SIDES_ONLY, CG_ModernHUDElementTeam16Create, CG_ModernHUDElementTeamRoutine, CG_ModernHUDElementTeamDestroy },
	{ "votemessagearena", 0, NULL, NULL, NULL },
	{ "votemessageworld", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementVMWCreate, CG_ModernHUDElementVMWRoutine, CG_ModernHUDElementVMWDestroy },
	{ "warmupinfo", 0, CG_ModernHUDElementWarmupInfoCreate, CG_ModernHUDElementWarmupInfoRoutine, CG_ModernHUDElementWarmupInfoDestroy },
	{ "weaponlist", 0, CG_ModernHUDElementWeaponListCreate, CG_ModernHUDElementWeaponListRoutine, CG_ModernHUDElementWeaponListDestroy },
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
	{ "rewardicons", 0, CG_ModernHUDElementRewardIconCreate, CG_ModernHUDElementRewardRoutine, CG_ModernHUDElementRewardDestroy },
	{ "rewardnumbers", 0, CG_ModernHUDElementRewardCountCreate, CG_ModernHUDElementRewardRoutine, CG_ModernHUDElementRewardDestroy },
	{ "awards", 0, CG_ModernHUDElementAwardsCreate, CG_ModernHUDElementAwardsRoutine, CG_ModernHUDElementAwardsDestroy },
	{ "crosshair", 0, CG_ModernHUDElementCrosshairCreate, CG_ModernHUDElementCrosshairRoutine, CG_ModernHUDElementCrosshairDestroy },
	{ "statusbar_value", 0, CG_ModernHUDElementStatusbarValueCreate, CG_ModernHUDElementStatusbarValueRoutine, CG_ModernHUDElementStatusbarValueDestroy },
	{ "statusbar_icon", 0, CG_ModernHUDElementStatusbarIconCreate, CG_ModernHUDElementStatusbarIconRoutine, CG_ModernHUDElementStatusbarIconDestroy },
	{ "statusbar_bar", 0, CG_ModernHUDElementStatusbarBarCreate, CG_ModernHUDElementStatusbarBarRoutine, CG_ModernHUDElementStatusbarBarDestroy },
	{ "obituary1", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementObituaries1Create, CG_ModernHUDElementObituariesRoutine, CG_ModernHUDElementObituariesDestroy },
	{ "obituary2", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementObituaries2Create, CG_ModernHUDElementObituariesRoutine, CG_ModernHUDElementObituariesDestroy },
	{ "obituary3", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementObituaries3Create, CG_ModernHUDElementObituariesRoutine, CG_ModernHUDElementObituariesDestroy },
	{ "obituary4", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementObituaries4Create, CG_ModernHUDElementObituariesRoutine, CG_ModernHUDElementObituariesDestroy },
	{ "obituary5", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementObituaries5Create, CG_ModernHUDElementObituariesRoutine, CG_ModernHUDElementObituariesDestroy },
	{ "obituary6", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementObituaries6Create, CG_ModernHUDElementObituariesRoutine, CG_ModernHUDElementObituariesDestroy },
	{ "obituary7", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementObituaries7Create, CG_ModernHUDElementObituariesRoutine, CG_ModernHUDElementObituariesDestroy },
	{ "obituary8", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementObituaries8Create, CG_ModernHUDElementObituariesRoutine, CG_ModernHUDElementObituariesDestroy },
	{ "location", 0, CG_ModernHUDElementLocationCreate, CG_ModernHUDElementLocationRoutine, CG_ModernHUDElementLocationDestroy },
	{ "tempAcc_current", SE_IM | SE_DEAD, CG_ModernHUDElementTempAccTextCreate, CG_ModernHUDElementTempAccRoutine, CG_ModernHUDElementTempAccDestroy },
	{ "tempAcc_icon", SE_IM | SE_DEAD, CG_ModernHUDElementTempAccIconCreate, CG_ModernHUDElementTempAccRoutine, CG_ModernHUDElementTempAccDestroy },
	{ "currentWeaponStats", SE_IM, CG_ModernHUDElementCreateCurrentWeapon, CG_ModernHUDElementWeaponStatsRoutine, CG_ModernHUDElementWeaponStatsDestroy },
	{ "weaponStats_MG", SE_IM, CG_ModernHUDElementWeaponStatsCreateMG, CG_ModernHUDElementWeaponStatsRoutine, CG_ModernHUDElementWeaponStatsDestroy },
	{ "weaponStats_SG", SE_IM, CG_ModernHUDElementWeaponStatsCreateSG, CG_ModernHUDElementWeaponStatsRoutine, CG_ModernHUDElementWeaponStatsDestroy },
	{ "weaponStats_GL", SE_IM, CG_ModernHUDElementWeaponStatsCreateGL, CG_ModernHUDElementWeaponStatsRoutine, CG_ModernHUDElementWeaponStatsDestroy },
	{ "weaponStats_RL", SE_IM, CG_ModernHUDElementWeaponStatsCreateRL, CG_ModernHUDElementWeaponStatsRoutine, CG_ModernHUDElementWeaponStatsDestroy },
	{ "weaponStats_LG", SE_IM, CG_ModernHUDElementWeaponStatsCreateLG, CG_ModernHUDElementWeaponStatsRoutine, CG_ModernHUDElementWeaponStatsDestroy },
	{ "weaponStats_RG", SE_IM, CG_ModernHUDElementWeaponStatsCreateRG, CG_ModernHUDElementWeaponStatsRoutine, CG_ModernHUDElementWeaponStatsDestroy },
	{ "weaponStats_PG", SE_IM, CG_ModernHUDElementWeaponStatsCreatePG, CG_ModernHUDElementWeaponStatsRoutine, CG_ModernHUDElementWeaponStatsDestroy },
	{ "currentWeaponStats_icon", SE_IM, CG_ModernHUDElementIconCreateCurrentWeapon, CG_ModernHUDElementWeaponStatsRoutine, CG_ModernHUDElementWeaponStatsDestroy },
	{ "weaponStats_MG_icon", SE_IM, CG_ModernHUDElementIconCreateMG, CG_ModernHUDElementWeaponStatsRoutine, CG_ModernHUDElementWeaponStatsDestroy },
	{ "weaponStats_SG_icon", SE_IM, CG_ModernHUDElementIconCreateSG, CG_ModernHUDElementWeaponStatsRoutine, CG_ModernHUDElementWeaponStatsDestroy },
	{ "weaponStats_GL_icon", SE_IM, CG_ModernHUDElementIconCreateGL, CG_ModernHUDElementWeaponStatsRoutine, CG_ModernHUDElementWeaponStatsDestroy },
	{ "weaponStats_RL_icon", SE_IM, CG_ModernHUDElementIconCreateRL, CG_ModernHUDElementWeaponStatsRoutine, CG_ModernHUDElementWeaponStatsDestroy },
	{ "weaponStats_LG_icon", SE_IM, CG_ModernHUDElementIconCreateLG, CG_ModernHUDElementWeaponStatsRoutine, CG_ModernHUDElementWeaponStatsDestroy },
	{ "weaponStats_RG_icon", SE_IM, CG_ModernHUDElementIconCreateRG, CG_ModernHUDElementWeaponStatsRoutine, CG_ModernHUDElementWeaponStatsDestroy },
	{ "weaponStats_PG_icon", SE_IM, CG_ModernHUDElementIconCreatePG, CG_ModernHUDElementWeaponStatsRoutine, CG_ModernHUDElementWeaponStatsDestroy },
	{ "playerStats_DG", SE_IM, CG_ModernHUDElementCreatePlayerStatsDG, CG_ModernHUDElementPlayerStatsRoutine, CG_ModernHUDElementPlayerStatsDestroy },
	{ "playerStats_DR", SE_IM, CG_ModernHUDElementCreatePlayerStatsDR, CG_ModernHUDElementPlayerStatsRoutine, CG_ModernHUDElementPlayerStatsDestroy },
	{ "playerStats_DG_icon", SE_IM, CG_ModernHUDElementCreatePlayerStatsDGIcon, CG_ModernHUDElementPlayerStatsRoutine, CG_ModernHUDElementPlayerStatsDestroy },
	{ "playerStats_DR_icon", SE_IM, CG_ModernHUDElementCreatePlayerStatsDRIcon, CG_ModernHUDElementPlayerStatsRoutine, CG_ModernHUDElementPlayerStatsDestroy },
	{ "playerStats_damageRatio", SE_IM, CG_ModernHUDElementCreatePlayerStatsDamageRatio, CG_ModernHUDElementPlayerStatsRoutine, CG_ModernHUDElementPlayerStatsDestroy },
	{ "player_name", 0, CG_ModernHUDElementPlayerNameCreate, CG_ModernHUDElementPlayerNameRoutine, CG_ModernHUDElementPlayerNameDestroy },
	{ "postdecorate", 0, CG_ModernHUDElementDecorCreate, CG_ModernHUDElementDecorRoutine, CG_ModernHUDElementDecorDestroy },
	{ "netstats", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementNetStatsCreate, CG_ModernHUDElementNetStatsRoutine, CG_ModernHUDElementNetStatsDestroy },
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

qboolean WiredHud_CreateElement( const char *name, const modernhudConfig_t *config ) {
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
	is_team_game   = wiredHud->isTeamGame;
	is_spectator   = wired_IsSpectator();
	is_scores      = wiredHud->showScores;

	for ( i = 0; i < wired_hudElementCount; i++ ) {
		wiredHudActiveElement_t *elem = &wired_hudElements[i];
		if ( !elem->active || !elem->routine || !elem->context ) continue;

		vflags = elem->visibility;

		skip = (!(vflags & SE_IM) && is_intermission) ||
		       ((vflags & SE_SIDES_ONLY) && !is_team_game) ||
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
