# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/qcommon/qcommon.h" PUBLIC)
file(READ "${ROOT}/code/qcommon/wired/core/vfs/files.c" VFS)
file(READ "${ROOT}/code/qcommon/wired/core/vfs/files_pack.h" PACK)
file(READ "${ROOT}/tests/smoke-fs-alias.sh" PROCESS_SMOKE)
file(READ "${ROOT}/tests/smoke-fs-dedup.sh" GLOBAL_PROCESS_SMOKE)
foreach(NEEDLE IN ITEMS "typedef uint32_t fsMountScopeId_t"
	"FS_MOUNT_SCOPE_GLOBAL" "FS_MOUNT_ROLE_SERVER" "FS_MOUNT_ROLE_CLIENT"
	"FS_MOUNT_ROLE_SHARED" "FS_MOUNT_ROLE_COSMETIC" "fsMountResult_t"
	"fsMountPurePolicy_t" "approvedChecksumCount" "denyLoose"
	"FS_MountLayer(" "FS_UnmountScope(" "FS_SetScopePurePolicy(")
	string(FIND "${PUBLIC}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "public mount scope vocabulary lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "mountScopeId" "mountRole" "FS_CountScopeHandles"
	"FS_MOUNT_SCOPE_BUSY" "FS_DetachScopeBlock" "FS_RestoreScopeBlock"
	"FS_MOUNT_LIFECYCLE_UNAVAILABLE" "FS_MOUNT_ALIAS_RELOAD_FAILED"
	"WiredScript_ReloadFileAliases"
	"FS_ArchivePrecedesOrEquals" "FS_OrderScopePrefix" "FS_CaptureMountPrefix"
	"FS_DetachCapturedMountNodes"
	"fs_numServerPaks ? DIR_DENY : DIR_ALLOW" "FS_SearchPathsUsePack"
	"FS_RemoveFromCache( block->pack )" "FS_FreePak( block->pack )"
	"FS_ScopeVerify_f" "FS_MOUNT_SCOPE_BUSY || liveHandles != 1u"
	"layers=2 alias_generation="
	"scopeState" "purePolicyConfigured" "FS_GlobalPakIsPure"
	"FS_SetScopePurePolicy" "FS_ApplyScopeDirPolicy"
	"packageReceiptConfigured" "FS_MountReceiptDigest"
	"FS_CollectScopeReceiptOutputs" "changed package receipt scope busy"
	"Identical immutable policy is already published"
	"Exact receipt/policy replay is read-only"
	"result != FS_MOUNT_SCOPE_BUSY || liveHandles != 1u"
	"(*p_insert_index)->scopeId != FS_MOUNT_SCOPE_GLOBAL"
	"s->scopeId == FS_MOUNT_SCOPE_GLOBAL"
	"allow_role=client deny_role=cosmetic containers=pk3+sw3z")
	string(FIND "${VFS}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "scoped mount lifecycle invariant lost: ${NEEDLE}")
	endif()
endforeach()
string(REGEX MATCHALL "FS_ArchivePrecedesOrEquals" ARCHIVE_COMPARATOR_USES "${VFS}")
list(LENGTH ARCHIVE_COMPARATOR_USES ARCHIVE_COMPARATOR_USE_COUNT)
if(NOT ARCHIVE_COMPARATOR_USE_COUNT EQUAL 3)
	message(FATAL_ERROR
		"global and scoped archive ordering must share one comparator; got ${ARCHIVE_COMPARATOR_USE_COUNT} uses")
endif()
string(FIND "${VFS}" "if ( !fh->name[0] ) continue" OPEN_HANDLE_DIAGNOSTIC)
if(OPEN_HANDLE_DIAGNOSTIC EQUAL -1)
	message(FATAL_ERROR
		"lsof must use the same name-based predicate as scope BUSY accounting")
endif()
foreach(NEEDLE IN ITEMS "scopeShadowed" "FS_RoleAllowedForScope"
	"FS_RefreshGlobalScopeShadows" "fs_scope_role_verify"
	"server_denied=1" "global_restored=1")
	string(FIND "${VFS}${PROCESS_SMOKE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "scope role/fallthrough invariant lost: ${NEEDLE}")
	endif()
endforeach()
string(FIND "${PACK}" "char\t\t\tpakGamename[MAX_QPATH]" PACK_GAMENAME_POS)
if(PACK_GAMENAME_POS EQUAL -1)
	message(FATAL_ERROR
		"scoped-unmount cache safety lost: pakGamename must be pack-owned storage")
endif()
foreach(NEEDLE IN ITEMS "scope_policy.pk3" "scope_policy.sw3z"
	"fs_scope_pure_verify home scope-pk3" "VFS scope pure verify: PASS")
	string(FIND "${PROCESS_SMOKE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "mixed-scope pure process coverage lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "scope-precedence/pax21.sw3z"
	"scope-precedence/pax01.sw3z"
	"FS_FOpenFileRead: scope/precedence.bin.*pax21.sw3z"
	"FS_FOpenFileRead: scope/q3-pak-order.bin.*pak8.pk3"
	"FS_FOpenFileRead: scope/pax-over-pak.bin.*pax21.sw3z"
	"FS_FOpenFileRead: scope/container-order.bin.*same.sw3z"
	"FS_FOpenFileRead: scope/root-order.bin.*homepath.*zzroot.pk3"
	"FS_FOpenFileRead: scope/archive-over-loose.bin.*pax21.sw3z")
	string(FIND "${PROCESS_SMOKE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "scoped archive precedence coverage lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "fsMountScopeId_t scopeId" "fsMountRole_t role"
	"FS_AddGameDirectoryScoped" "sp->scopeId == scopeId" "sp->role == role"
	"search->scopeId = scopeId" "search->role = role"
	"FS_MOUNT_SCOPE_GLOBAL" "FS_MOUNT_ROLE_GLOBAL")
	string(FIND "${VFS}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "searchpath scope invariant lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
	"FS_FOpenFileRead: order/q3-pak.cfg.*pak8.pk3"
	"FS_FOpenFileRead: order/pax.cfg.*pax21.sw3z"
	"FS_FOpenFileRead: order/pax-over-pak.cfg.*pax21.sw3z"
	"FS_FOpenFileRead: order/container.cfg.*same.sw3z"
	"FS_FOpenFileRead: order/root.cfg.*homepath.*zzroot.sw3z"
	"FS_FOpenFileRead: order/archive-over-loose.cfg.*pax21.sw3z")
	string(FIND "${GLOBAL_PROCESS_SMOKE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "global archive precedence coverage lost: ${NEEDLE}")
	endif()
endforeach()
string(REGEX MATCHALL "search->scopeId = scopeId" SCOPE_WRITES "${VFS}")
list(LENGTH SCOPE_WRITES SCOPE_WRITE_COUNT)
if(NOT SCOPE_WRITE_COUNT EQUAL 4)
	message(FATAL_ERROR "every directory/PK3/pk3dir/SW3Z node must inherit scope; got ${SCOPE_WRITE_COUNT}")
endif()
message(STATUS "VFS mount scope metadata policy: PASS")
