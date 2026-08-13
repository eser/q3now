// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "wired_profile_imgui.h"

#include "imgui.h"

int WiredProfileImGui_Draw( const ralProfileSnapshot_t *snapshot,
		const double *historyMs, uint32_t historyFrames,
		wiredProfileImGuiReceipt_t *receipt ) {
	wiredProfileImGuiReceipt_t result = {};
	float laneHistory[ RAL_PROFILE_HISTORY_CAPACITY ];
	uint32_t lane, frame;

	if ( !snapshot || !receipt || !historyMs || historyFrames == 0 ||
	     historyFrames > RAL_PROFILE_HISTORY_CAPACITY ||
	     snapshot->laneCount == 0 || snapshot->laneCount > RAL_PROFILE_MAX_LANES ) {
		return 0;
	}
	for ( lane = 0; lane < snapshot->laneCount; ++lane ) {
		if ( !snapshot->labels[ lane ] || !snapshot->labels[ lane ][ 0 ] ) {
			return 0;
		}
	}

	result.topologyEpoch = snapshot->topologyEpoch;
	ImGui::Begin( "Wired GPU Profile" );
	ImGui::Text( "Topology epoch %u | samples %u", snapshot->topologyEpoch,
	             snapshot->sampleCount );
	if ( ImGui::BeginTable( "ral-profile-summary", 5,
	                       ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg ) ) {
		ImGui::TableSetupColumn( "Lane" );
		ImGui::TableSetupColumn( "Latest ms" );
		ImGui::TableSetupColumn( "Average ms" );
		ImGui::TableSetupColumn( "Minimum ms" );
		ImGui::TableSetupColumn( "Maximum ms" );
		ImGui::TableHeadersRow();
		for ( lane = 0; lane < snapshot->laneCount; ++lane ) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex( 0 ); ImGui::TextUnformatted( snapshot->labels[ lane ] );
			ImGui::TableSetColumnIndex( 1 ); ImGui::Text( "%.3f", snapshot->latestMs[ lane ] );
			ImGui::TableSetColumnIndex( 2 ); ImGui::Text( "%.3f", snapshot->averageMs[ lane ] );
			ImGui::TableSetColumnIndex( 3 ); ImGui::Text( "%.3f", snapshot->minimumMs[ lane ] );
			ImGui::TableSetColumnIndex( 4 ); ImGui::Text( "%.3f", snapshot->maximumMs[ lane ] );
			result.laneRows++;
		}
		ImGui::EndTable();
	}

	for ( lane = 0; lane < snapshot->laneCount; ++lane ) {
		for ( frame = 0; frame < historyFrames; ++frame ) {
			laneHistory[ frame ] = (float)historyMs[
				frame * RAL_PROFILE_MAX_LANES + lane ];
		}
		ImGui::PlotLines( snapshot->labels[ lane ], laneHistory,
		                  (int)historyFrames, 0, NULL, 0.0f, FLT_MAX,
		                  ImVec2( 0.0f, 64.0f ) );
		result.plottedLanes++;
	}
	result.plottedSamples = historyFrames;
	ImGui::End();

	*receipt = result;
	return 1;
}
