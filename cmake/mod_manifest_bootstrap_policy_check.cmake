# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/code/qcommon/net_transport.h" TRANSPORT_H)
file(READ "${ROOT}/code/qcommon/mod_manifest.h" MANIFEST_H)
file(READ "${ROOT}/code/server/sv_client.c" SERVER_CLIENT)
file(READ "${ROOT}/code/server/sv_init.c" SERVER_INIT)
file(READ "${ROOT}/code/client/client.h" CLIENT_H)
file(READ "${ROOT}/code/client/cl_parse.c" CLIENT_PARSE)
file(READ "${ROOT}/code/client/cl_main.c" CLIENT_MAIN)
file(READ "${ROOT}/code/qcommon/qcommon.h" QCOMMON_H)
file(READ "${ROOT}/code/qcommon/wired/core/vfs/files.c" VFS)
file(READ "${ROOT}/code/qcommon/util/crypto.c" CRYPTO)

foreach(NEEDLE IN ITEMS
	"WN_BOOTSTRAP_SEC_CONTENT_MANIFESTS = 6"
	"WIRED_PACKAGE_RECEIPT_BYTES_MAX (64 * 1024)")
	string(FIND "${TRANSPORT_H}${MANIFEST_H}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "typed content receipt wire bound drift: ${NEEDLE}")
	endif()
endforeach()

foreach(NEEDLE IN ITEMS
	"FS_MOUNT_ROLE_RUNTIME"
	"FS_MOUNT_ROLE_TOOLCHAIN"
	"FS_MOUNT_ROLE_SERVER"
	"FS_ApplyScopePackageReceipts("
	"FS_ApplyGlobalPackageReceipts("
	"Com_SHA256FileHex("
	"package integrity mismatch"
	"FS_CountScopeHandles( scopeId )"
	"search->role = receipt->role")
	string(FIND "${QCOMMON_H}${VFS}${CRYPTO}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "verified package-role receipt invariant lost: ${NEEDLE}")
	endif()
endforeach()

foreach(NEEDLE IN ITEMS
	"FS_ListFiles( \"\", \".sw3z.manifest.json\""
	"WiredPackageManifest_Parse("
	"WiredPackageManifest_ValidateSet("
	"SV_WiredNetWriteContentManifests("
	"WN_BOOTSTRAP_SEC_CONTENT_MANIFESTS"
	"SV_ContentManifestCatalogReset( void )"
	"SV_ContentManifestCatalogReset();")
	string(FIND "${SERVER_CLIENT}${SERVER_INIT}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "server content catalog invariant lost: ${NEEDLE}")
	endif()
endforeach()

foreach(NEEDLE IN ITEMS
	"SV_ApplyContentManifestRoles("
	"FS_ApplyGlobalPackageReceipts( receipts, receiptCount"
	"SV_BuildContentManifestCatalog()"
	"FS_LoadedPakChecksumsForRole( FS_MOUNT_ROLE_CLIENT")
	string(FIND "${SERVER_CLIENT}${SERVER_INIT}${VFS}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "server role-filtered pure advertise invariant lost: ${NEEDLE}")
	endif()
endforeach()

foreach(NEEDLE IN ITEMS
	"CL_ApplyContentManifestReceipt("
	"case WIRED_PACKAGE_ROLE_SERVER: return FS_MOUNT_ROLE_SERVER"
	"receipts[receiptCount].clientApproved = role == FS_MOUNT_ROLE_RUNTIME"
	"archiveCount != receiptCount"
	"legacy archives keep compatibility pure policy"
	"FS_SetScopePurePolicy("
	"policy.denyLoose = qtrue")
	string(FIND "${CLIENT_MAIN}${CLIENT_PARSE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "client package enforcement invariant lost: ${NEEDLE}")
	endif()
endforeach()

foreach(NEEDLE IN ITEMS
	"FS_SetPureChecksumFeed( int checksumFeed )"
	"FS_SetPureChecksumFeed( app->clc.checksumFeed )"
	"int emitted[MAX_REF_PAKS]"
	"identity once across the dedicated and general sections")
	string(FIND "${QCOMMON_H}${VFS}${CLIENT_PARSE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "pure checksum lifecycle/identity invariant lost: ${NEEDLE}")
	endif()
endforeach()

foreach(NEEDLE IN ITEMS
	"wiredPackageManifest_t *contentManifests"
	"contentManifestCount"
	"contentManifestReceived"
	"CL_WiredNetReadContentManifests("
	"sectionLen > WIRED_PACKAGE_RECEIPT_BYTES_MAX"
	"!sawAck || !sawCmds || !sawContent || !sawConfig || !sawBaselines || !sawClientInfo"
	"app->contentManifests = contentManifests"
	"fail:"
	"if ( contentManifests ) Z_Free( contentManifests )"
	"CL_ClearContentManifestReceipt( app )")
	string(FIND "${CLIENT_H}${CLIENT_PARSE}${CLIENT_MAIN}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "transactional client receipt invariant lost: ${NEEDLE}")
	endif()
endforeach()

message(STATUS "Typed content-manifest bootstrap policy: PASS")
