if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/tools/sw3z-archiver/lifecycle/manifest.go" GO_SCHEMA)
file(READ "${ROOT}/code/qcommon/mod_manifest.h" C_SCHEMA)
file(READ "${ROOT}/code/qcommon/mod_manifest.c" C_RUNTIME)

foreach(TOKEN IN ITEMS
	"wired.sw3z.package/v1"
	"runtime"
	"toolchain"
	"server"
	"client"
	"shared"
	"cosmetic")
	string(FIND "${GO_SCHEMA}" "${TOKEN}" GO_POS)
	string(FIND "${C_SCHEMA}${C_RUNTIME}" "${TOKEN}" C_POS)
	if(GO_POS EQUAL -1 OR C_POS EQUAL -1)
		message(FATAL_ERROR "canonical package vocabulary drift: ${TOKEN}")
	endif()
endforeach()

foreach(FIELD IN ITEMS id version owner depends provides role compatibility files)
	string(FIND "${GO_SCHEMA}" "json:\"${FIELD}" GO_POS)
	string(FIND "${C_RUNTIME}" "\"${FIELD}\"" C_POS)
	if(GO_POS EQUAL -1 OR C_POS EQUAL -1)
		message(FATAL_ERROR "canonical package field drift: ${FIELD}")
	endif()
endforeach()

foreach(INVARIANT IN ITEMS
	"WIRED_PACKAGE_DEPENDENCY_MAX"
	"WIRED_PACKAGE_PROVIDE_MAX"
	"WIRED_PACKAGE_FILE_MAX"
	"package depends on itself"
	"duplicate dependency"
	"duplicate file path"
	"WiredPackageManifest_ValidateSet"
	"unsafe multi-owner file collision"
	"ambiguous dependency provider"
	"unsupported schema")
	string(FIND "${C_SCHEMA}${C_RUNTIME}" "${INVARIANT}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "runtime manifest fail-closed invariant missing: ${INVARIANT}")
	endif()
endforeach()
