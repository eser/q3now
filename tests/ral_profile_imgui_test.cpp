// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include <cstdio>
#include <cstring>

#include "imgui.h"
#include <SDL3/SDL_events.h>
#include "wired_profile_imgui.h"
#include "wired_profile_imgui_sdl3.h"

#define CHECK(expr) do { \
	if ( !(expr) ) { \
		std::fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); \
		return 1; \
	} \
} while ( 0 )

int main() {
	ralProfileSnapshot_t snapshot = {};
	wiredProfileImGuiReceipt_t receipt = {}, before;
	double history[ 3 ][ RAL_PROFILE_MAX_LANES ] = {};
	ImDrawData *drawData;
	ImGuiIO *io;
	wiredProfileImGuiSdl3Receipt_t inputReceipt = {}, inputBefore;
	SDL_Event event = {};

	snapshot.labels[ 0 ] = "wired.main";
	snapshot.labels[ 1 ] = "wired.present";
	snapshot.latestMs[ 0 ] = 1.25; snapshot.latestMs[ 1 ] = 0.50;
	snapshot.averageMs[ 0 ] = 1.00; snapshot.averageMs[ 1 ] = 0.40;
	snapshot.minimumMs[ 0 ] = 0.75; snapshot.minimumMs[ 1 ] = 0.25;
	snapshot.maximumMs[ 0 ] = 1.50; snapshot.maximumMs[ 1 ] = 0.60;
	snapshot.laneCount = 2;
	snapshot.sampleCount = 3;
	snapshot.topologyEpoch = 7;
	history[ 0 ][ 0 ] = 0.75; history[ 0 ][ 1 ] = 0.25;
	history[ 1 ][ 0 ] = 1.00; history[ 1 ][ 1 ] = 0.45;
	history[ 2 ][ 0 ] = 1.25; history[ 2 ][ 1 ] = 0.50;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	io = &ImGui::GetIO();
	io->IniFilename = NULL;
	io->DisplaySize = ImVec2( 1280.0f, 720.0f );
	io->DisplayFramebufferScale = ImVec2( 1.0f, 1.0f );
	io->DeltaTime = 1.0f / 60.0f;
	io->ConfigInputTrickleEventQueue = false;
	io->Fonts->AddFontDefault();
	io->Fonts->Build();
	// The contract intentionally has no renderer backend. Give the legacy
	// backend-neutral atlas path a stable non-zero receipt so ImGui retains the
	// text/plot draw lists for structural inspection.
	io->Fonts->SetTexID( (ImTextureID)1 );
	event.type = SDL_EVENT_KEY_DOWN;
	event.key.scancode = SDL_SCANCODE_A;
	event.key.mod = SDL_KMOD_LSHIFT;
	CHECK( WiredProfileImGuiSdl3_ProcessEvent( &event, &inputReceipt ) == 1 );
	event = {};
	event.type = SDL_EVENT_TEXT_INPUT;
	event.text.text = "é";
	CHECK( WiredProfileImGuiSdl3_ProcessEvent( &event, &inputReceipt ) == 1 );
	event = {};
	event.type = SDL_EVENT_MOUSE_MOTION;
	event.motion.x = 125.5f; event.motion.y = 63.25f;
	CHECK( WiredProfileImGuiSdl3_ProcessEvent( &event, &inputReceipt ) == 1 );
	event = {};
	event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
	event.button.button = SDL_BUTTON_LEFT;
	CHECK( WiredProfileImGuiSdl3_ProcessEvent( &event, &inputReceipt ) == 1 );
	event = {};
	event.type = SDL_EVENT_MOUSE_WHEEL;
	event.wheel.x = 1.0f; event.wheel.y = -2.0f;
	event.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
	CHECK( WiredProfileImGuiSdl3_ProcessEvent( &event, &inputReceipt ) == 1 );
	event = {};
	event.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
	CHECK( WiredProfileImGuiSdl3_ProcessEvent( &event, &inputReceipt ) == 1 );
	CHECK( inputReceipt.handledEvents == 6 );
	CHECK( inputReceipt.keyboardEvents == 1 );
	CHECK( inputReceipt.textEvents == 1 );
	CHECK( inputReceipt.mouseEvents == 3 );
	CHECK( inputReceipt.focusEvents == 1 );
	ImGui::NewFrame();
	CHECK( ImGui::IsKeyDown( ImGuiKey_A ) );
	CHECK( io->KeyShift );
	// Dear ImGui consumes SDL's float coordinates onto its pixel grid.
	CHECK( io->MousePos.x == 125.0f && io->MousePos.y == 63.0f );
	CHECK( io->MouseDown[ 0 ] );
	CHECK( io->MouseWheelH == -1.0f && io->MouseWheel == 2.0f );
	CHECK( io->InputQueueCharacters.Size == 1 );
	CHECK( io->InputQueueCharacters[ 0 ] == 0x00e9 );
	CHECK( WiredProfileImGui_Draw( &snapshot, &history[ 0 ][ 0 ], 3, &receipt ) == 1 );
	ImGui::Render();
	event = {};
	event.type = SDL_EVENT_KEY_UP;
	event.key.scancode = SDL_SCANCODE_A;
	CHECK( WiredProfileImGuiSdl3_ProcessEvent( &event, &inputReceipt ) == 1 );
	event = {};
	event.type = SDL_EVENT_MOUSE_BUTTON_UP;
	event.button.button = SDL_BUTTON_LEFT;
	CHECK( WiredProfileImGuiSdl3_ProcessEvent( &event, &inputReceipt ) == 1 );
	event = {};
	event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
	CHECK( WiredProfileImGuiSdl3_ProcessEvent( &event, &inputReceipt ) == 1 );
	// Dear ImGui resolves an auto-fit window's final clip geometry after its
	// first frame. Exercise the stable second frame that a persistent tool host
	// actually presents.
	ImGui::NewFrame();
	CHECK( WiredProfileImGui_Draw( &snapshot, &history[ 0 ][ 0 ], 3, &receipt ) == 1 );
	CHECK( !ImGui::IsKeyDown( ImGuiKey_A ) );
	CHECK( !io->MouseDown[ 0 ] );
	CHECK( io->AppFocusLost );
	CHECK( inputReceipt.handledEvents == 9 );
	CHECK( inputReceipt.keyboardEvents == 2 );
	CHECK( inputReceipt.mouseEvents == 4 );
	CHECK( inputReceipt.focusEvents == 2 );
	CHECK( receipt.topologyEpoch == 7 );
	CHECK( receipt.laneRows == 2 );
	CHECK( receipt.plottedLanes == 2 );
	CHECK( receipt.plottedSamples == 3 );
	ImGui::Render();
	drawData = ImGui::GetDrawData();
	CHECK( drawData != NULL );
	CHECK( drawData->CmdListsCount > 0 );
	CHECK( drawData->TotalVtxCount > 0 );
	CHECK( drawData->TotalIdxCount > 0 );

	before = receipt;
	snapshot.laneCount = RAL_PROFILE_MAX_LANES + 1;
	CHECK( WiredProfileImGui_Draw( &snapshot, &history[ 0 ][ 0 ], 3, &receipt ) == 0 );
	CHECK( std::memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );
	snapshot.laneCount = 2;
	snapshot.labels[ 1 ] = NULL;
	CHECK( WiredProfileImGui_Draw( &snapshot, &history[ 0 ][ 0 ], 3, &receipt ) == 0 );
	CHECK( std::memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );
	snapshot.labels[ 1 ] = "wired.present";
	CHECK( WiredProfileImGui_Draw( &snapshot, NULL, 3, &receipt ) == 0 );
	CHECK( std::memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );
	CHECK( WiredProfileImGui_Draw( &snapshot, &history[ 0 ][ 0 ], 0, &receipt ) == 0 );
	CHECK( std::memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );

	inputBefore = inputReceipt;
	event = {};
	event.type = SDL_EVENT_USER;
	CHECK( WiredProfileImGuiSdl3_ProcessEvent( &event, &inputReceipt ) == 0 );
	CHECK( std::memcmp( &inputReceipt, &inputBefore, sizeof( inputReceipt ) ) == 0 );
	event = {};
	event.type = SDL_EVENT_KEY_DOWN;
	event.key.scancode = SDL_SCANCODE_UNKNOWN;
	CHECK( WiredProfileImGuiSdl3_ProcessEvent( &event, &inputReceipt ) == 0 );
	CHECK( std::memcmp( &inputReceipt, &inputBefore, sizeof( inputReceipt ) ) == 0 );
	CHECK( WiredProfileImGuiSdl3_ProcessEvent( NULL, &inputReceipt ) == 0 );
	CHECK( std::memcmp( &inputReceipt, &inputBefore, sizeof( inputReceipt ) ) == 0 );

	ImGui::DestroyContext();
	event = {};
	event.type = SDL_EVENT_KEY_DOWN;
	event.key.scancode = SDL_SCANCODE_F24;
	CHECK( WiredProfileImGuiSdl3_ProcessEvent( &event, &inputReceipt ) == 0 );
	CHECK( std::memcmp( &inputReceipt, &inputBefore, sizeof( inputReceipt ) ) == 0 );
	std::puts( "ral profile ImGui adapter contract: PASS" );
	return 0;
}
