// cg_superhud_element_player_stats.c — stubbed (needs OSP2-BE stats system)
#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI


void* CG_SHUDElementCreatePlayerStatsDG(const superhudConfig_t* config) { return NULL; }
void* CG_SHUDElementCreatePlayerStatsDR(const superhudConfig_t* config) { return NULL; }
void* CG_SHUDElementCreatePlayerStatsDGIcon(const superhudConfig_t* config) { return NULL; }
void* CG_SHUDElementCreatePlayerStatsDRIcon(const superhudConfig_t* config) { return NULL; }
void* CG_SHUDElementCreatePlayerStatsDamageRatio(const superhudConfig_t* config) { return NULL; }
void CG_SHUDElementPlayerStatsRoutine(void* context) { }
void CG_SHUDElementPlayerStatsDestroy(void* context) { }
#endif // FEAT_WIRED_UI
