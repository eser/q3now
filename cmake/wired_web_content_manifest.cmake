# SPDX-License-Identifier: GPL-3.0-or-later

foreach(REQUIRED IN ITEMS OUT_DIR PAX01 PAX21 GAMECL GAMESV CONFIG VISOR_ANIMATION AUTHORED_CONTENT)
	if(NOT DEFINED ${REQUIRED} OR NOT EXISTS "${${REQUIRED}}")
		message(FATAL_ERROR "wired Web content manifest requires ${REQUIRED}")
	endif()
endforeach()

file(MAKE_DIRECTORY "${OUT_DIR}")
set(ASSETS
	"pax01|${PAX01}|pax01.sw3z|/base/pax01.sw3z"
	"pax21|${PAX21}|pax21.sw3z|/base/pax21.sw3z"
	"gamecl|${GAMECL}|gameclwasm32.wasm|/base/gameclwasm32.wasm"
	"gamesv|${GAMESV}|gamesvwasm32.wasm|/base/gamesvwasm32.wasm"
	"config|${CONFIG}|default.cfg|/base/default.cfg"
	"visor-animation|${VISOR_ANIMATION}|visor-animation.cfg|/base/characters/visor/models/animation.cfg"
	"authored-content|${AUTHORED_CONTENT}|authored-content.wac|/base/web/authored-content.wac")

set(JSON_ASSETS "")
set(VERSION_SEED "")
foreach(ASSET IN LISTS ASSETS)
	string(REPLACE "|" ";" FIELDS "${ASSET}")
	list(GET FIELDS 0 ID)
	list(GET FIELDS 1 SOURCE)
	list(GET FIELDS 2 NAME)
	list(GET FIELDS 3 MOUNT)
	file(SHA256 "${SOURCE}" SHA)
	file(SIZE "${SOURCE}" SIZE)
	string(APPEND VERSION_SEED "${ID}:${SHA}:${SIZE};")
	set(DESTINATION "${OUT_DIR}/${NAME}")
	if(NOT SOURCE STREQUAL DESTINATION)
		file(REMOVE "${DESTINATION}")
		file(CREATE_LINK "${SOURCE}" "${DESTINATION}" SYMBOLIC COPY_ON_ERROR)
	endif()
	if(JSON_ASSETS)
		string(APPEND JSON_ASSETS ",\n")
	endif()
	string(APPEND JSON_ASSETS
		"    {\"id\":\"${ID}\",\"url\":\"./${NAME}\",\"mountPath\":\"${MOUNT}\",\"bytes\":${SIZE},\"sha256\":\"${SHA}\"}")
endforeach()
string(SHA256 VERSION_HASH "${VERSION_SEED}")
string(SUBSTRING "${VERSION_HASH}" 0 20 VERSION_SHORT)
set(MANIFEST "{\n  \"schemaVersion\":1,\n  \"version\":\"content-${VERSION_SHORT}\",\n  \"assets\":[\n${JSON_ASSETS}\n  ]\n}\n")
file(WRITE "${OUT_DIR}/manifest.json" "${MANIFEST}")
message(STATUS "Wired browser content manifest: ${OUT_DIR}/manifest.json")
