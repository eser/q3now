// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "../code/renderervk/vk_bindless_cohort.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x); \
	return 1; } } while (0)

int main( void ) {
	struct ralBackend_s *backend=(struct ralBackend_s *)0x1000u;
	struct ralBindGroupLayout_s *layout=(struct ralBindGroupLayout_s *)0x2000u;
	struct ralBindGroup_s *set=(struct ralBindGroup_s *)0x3000u;
	void *rawLayout=(void *)0x4000u, *rawSet=(void *)0x5000u;
	vkBindlessPublicationLedger_t ledger, mutatedLedger;
	vkRalBindlessCohortReceipt_t receipt, snapshot, mutation;

	VK_BindlessPublicationInit( &ledger );
	CHECK( VK_BindlessPublicationActivateSet( &ledger, set ) );
	memset( &receipt, 0x5a, sizeof( receipt ) ); snapshot=receipt;
	CHECK( VK_BindlessCohortBuild( backend, layout, set, rawLayout, rawSet,
		qtrue, &ledger, &receipt ) );
	CHECK( receipt.ready && receipt.backend==backend && receipt.layout==layout
		&& receipt.set==set && receipt.rawLayout==rawLayout
		&& receipt.rawSet==rawSet && receipt.setGeneration==1u );
	CHECK( VK_BindlessCohortReceiptExact( &receipt, &receipt ) );

	#define MUTATE_RECEIPT(field) do { mutation=receipt; mutation.field=(void *) \
		((uintptr_t)mutation.field+1u); CHECK(!VK_BindlessCohortReceiptExact( \
		&receipt,&mutation)); } while(0)
	MUTATE_RECEIPT(backend);
	MUTATE_RECEIPT(layout);
	MUTATE_RECEIPT(set);
	MUTATE_RECEIPT(rawLayout);
	MUTATE_RECEIPT(rawSet);
	#undef MUTATE_RECEIPT
	mutation=receipt; mutation.setGeneration++;
	CHECK( !VK_BindlessCohortReceiptExact( &receipt, &mutation ) );
	mutation=receipt; mutation.ready=qfalse;
	CHECK( !VK_BindlessCohortReceiptExact( &mutation, &mutation ) );
	mutation=receipt; mutation.backend=NULL;
	CHECK( !VK_BindlessCohortReceiptExact( &mutation, &mutation ) );
	mutation=receipt; mutation.layout=NULL;
	CHECK( !VK_BindlessCohortReceiptExact( &mutation, &mutation ) );
	mutation=receipt; mutation.set=NULL;
	CHECK( !VK_BindlessCohortReceiptExact( &mutation, &mutation ) );
	mutation=receipt; mutation.rawLayout=NULL;
	CHECK( !VK_BindlessCohortReceiptExact( &mutation, &mutation ) );
	mutation=receipt; mutation.rawSet=NULL;
	CHECK( !VK_BindlessCohortReceiptExact( &mutation, &mutation ) );
	mutation=receipt; mutation.setGeneration=0;
	CHECK( !VK_BindlessCohortReceiptExact( &mutation, &mutation ) );
	mutation=receipt; mutation.setGeneration=UINT64_MAX;
	CHECK( !VK_BindlessCohortReceiptExact( &mutation, &mutation ) );
	mutation=receipt; mutation.layout=(struct ralBindGroupLayout_s *)backend;
	CHECK( !VK_BindlessCohortReceiptExact( &mutation, &mutation ) );
	mutation=receipt; mutation.set=(struct ralBindGroup_s *)backend;
	CHECK( !VK_BindlessCohortReceiptExact( &mutation, &mutation ) );
	mutation=receipt; mutation.set=(struct ralBindGroup_s *)layout;
	CHECK( !VK_BindlessCohortReceiptExact( &mutation, &mutation ) );

	#define BUILD_FAIL(...) do { snapshot=receipt; CHECK(!VK_BindlessCohortBuild( \
		__VA_ARGS__,&snapshot)); CHECK(memcmp(&snapshot,&receipt,sizeof(snapshot))==0); \
	} while(0)
	BUILD_FAIL(NULL,layout,set,rawLayout,rawSet,qtrue,&ledger);
	BUILD_FAIL(backend,NULL,set,rawLayout,rawSet,qtrue,&ledger);
	BUILD_FAIL(backend,layout,NULL,rawLayout,rawSet,qtrue,&ledger);
	BUILD_FAIL(backend,layout,set,NULL,rawSet,qtrue,&ledger);
	BUILD_FAIL(backend,layout,set,rawLayout,NULL,qtrue,&ledger);
	BUILD_FAIL(backend,layout,set,rawLayout,rawSet,qfalse,&ledger);
	mutatedLedger=ledger; mutatedLedger.setIdentity=(uintptr_t)layout;
	BUILD_FAIL(backend,layout,set,rawLayout,rawSet,qtrue,&mutatedLedger);
	mutatedLedger=ledger; mutatedLedger.setGeneration=0;
	BUILD_FAIL(backend,layout,set,rawLayout,rawSet,qtrue,&mutatedLedger);
	mutatedLedger=ledger; mutatedLedger.setGeneration=UINT64_MAX;
	BUILD_FAIL(backend,layout,set,rawLayout,rawSet,qtrue,&mutatedLedger);
	#undef BUILD_FAIL

	CHECK( VK_BindlessPublicationInvalidateSet( &ledger, set ) );
	CHECK( VK_BindlessPublicationActivateSet( &ledger, set ) );
	CHECK( ledger.setGeneration==2u );
	CHECK( VK_BindlessCohortBuild( backend,layout,set,rawLayout,rawSet,
		qtrue,&ledger,&mutation ) );
	CHECK( mutation.setGeneration==2u
		&& !VK_BindlessCohortReceiptExact( &receipt,&mutation ) );

	puts( "vk_bindless_cohort_test: PASS" );
	return 0;
}
