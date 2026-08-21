// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_transient.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

int main( void ) {
	ralTransientPolicy_t policy;
	ralTransientRequest_t requests[3];
	ralTransientPlan_t plan,before;
	uint32_t i;
	memset(&policy,0,sizeof(policy));memset(requests,0,sizeof(requests));
	policy.backendType=RAL_BACKEND_WEBGPU;policy.generation=1u;
	policy.budgetBytes=4096u;policy.explicitAliasing=qfalse;
	for(i=0u;i<3u;++i) {
		requests[i].resourceIdentity=(uintptr_t)(100u+i);
		requests[i].resourceGeneration=1u;requests[i].compatibilityKey=9u;
		requests[i].size=1024u;requests[i].alignment=256u;
		requests[i].firstPass=i*2u;requests[i].lastPass=i*2u+1u;
		requests[i].allowAlias=qtrue;
	}
	CHECK(Ral_TransientPlanBuild(&policy,requests,3u,&plan));
	CHECK(plan.outcome==RAL_TRANSIENT_PLAN_MANAGED_DISJOINT);
	CHECK(plan.slotCount==3u&&plan.aliasCount==0u&&plan.peakBytes==3072u);
	CHECK(plan.assignments[0].slotIndex==0u&&plan.assignments[1].slotIndex==1u
		&& plan.assignments[2].slotIndex==2u);
	before=plan;
	policy.explicitAliasing=qtrue;
	CHECK(!Ral_TransientPlanBuild(&policy,requests,3u,&plan));
	CHECK(memcmp(&plan,&before,sizeof(plan))==0);
	puts("ral WebGPU transient plan: PASS");
	return 0;
}
