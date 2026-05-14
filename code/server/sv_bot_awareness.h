// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#pragma once

void SV_BotAwareness_Init    ( void );
void SV_BotAwareness_Shutdown( void );
int  SV_BotAwareness_GetEvents( int clientNum, bot_sound_event_t *out, int maxOut );
