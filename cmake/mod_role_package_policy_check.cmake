if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/Makefile" makefile_text)

set(required_fragments
	"PAK_OUTPUTS := $(PAK_OUT) $(PAK_CLIENT_OUT) $(PAK_SERVER_OUT)"
	"$(PAK_OUT): Makefile $(PAK_CONTENT_SRC) $(SW3Z_BIN)"
	"$(PAK_CLIENT_OUT): Makefile $(PAK_CLIENT_VM) $(SW3Z_BIN)"
	"$(PAK_SERVER_OUT): Makefile $(PAK_SERVER_VM) $(SW3Z_BIN)"
	"--role shared"
	"--role client"
	"--role server"
	"--depends q3now.content.core"
	"--file \"pax21-client.sw3z=$(PAK_CLIENT_OUT)\""
	"--file \"pax21-server.sw3z=$(PAK_SERVER_OUT)\""
	"$(SW3Z_BIN) manifest validate $(PAK_MANIFESTS)"
	"cp $(PAK_OUTPUTS) \"$(1)/\""
	"cp $(PAK_MANIFESTS) \"$(1)/\"")

foreach(fragment IN LISTS required_fragments)
	string(FIND "${makefile_text}" "${fragment}" fragment_pos)
	if(fragment_pos EQUAL -1)
		message(FATAL_ERROR "role-package contract missing: ${fragment}")
	endif()
endforeach()

string(FIND "${makefile_text}" "cp $(PAK_VM_MODULES) $(PAK_SHARED_STAGING)" aggregate_vm_copy)
if(NOT aggregate_vm_copy EQUAL -1)
	message(FATAL_ERROR "shared package regained aggregate client/server VM ownership")
endif()

message(STATUS "Role-split SW3Z package policy contract passed")
