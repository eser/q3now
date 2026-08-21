// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_transient.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

static void Fixture( ralTransientPolicy_t *policy, ralTransientRequest_t *requests ) {
	memset(policy,0,sizeof(*policy));memset(requests,0,sizeof(*requests)*4u);
	policy->backendType=RAL_BACKEND_VULKAN;policy->generation=3u;
	policy->budgetBytes=4096u;policy->explicitAliasing=qtrue;
#define REQ(i,id,key,psize,align,first,last) do { requests[i].resourceIdentity=(uintptr_t)(id); \
	requests[i].resourceGeneration=(i)+1u;requests[i].compatibilityKey=(key); \
	requests[i].size=(psize);requests[i].alignment=(align);requests[i].firstPass=(first); \
	requests[i].lastPass=(last);requests[i].allowAlias=qtrue; } while(0)
	REQ(0,100u,7u,1024u,256u,0u,1u);
	REQ(1,200u,8u,512u,256u,0u,2u);
	REQ(2,300u,7u,2048u,512u,2u,3u);
	REQ(3,400u,8u,256u,256u,3u,4u);
#undef REQ
}

int main( void ) {
	ralTransientPolicy_t policy;
	ralTransientRequest_t requests[RAL_TRANSIENT_MAX_REQUESTS];
	ralTransientPlan_t plan,exact,beforePlan;
	ralTransientBatchLifecycle_t lifecycle,beforeLifecycle;
	ralTransientBatchReceipt_t planned,submitted,retired,canceled,beforeReceipt,bad;
	Fixture(&policy,requests);
	CHECK(Ral_TransientPlanBuild(&policy,requests,4u,&plan));
	CHECK(plan.outcome==RAL_TRANSIENT_PLAN_EXPLICIT_ALIAS&&plan.slotCount==2u&&plan.aliasCount==2u);
	CHECK(plan.peakBytes==2560u&&plan.requestedBytes==3840u);
	CHECK(plan.assignments[0].slotIndex==0u&&plan.assignments[2].slotIndex==0u);
	CHECK(plan.assignments[1].slotIndex==1u&&plan.assignments[3].slotIndex==1u);
	CHECK(plan.assignments[0].committedSize==2048u);
	exact=plan;CHECK(Ral_TransientPlanExact(&plan,&exact));
	exact.assignments[2].slotIndex=1u;CHECK(!Ral_TransientPlanExact(&exact,&exact));
	Fixture(&policy,requests);requests[2].firstPass=1u;
	CHECK(Ral_TransientPlanBuild(&policy,requests,4u,&exact));
	CHECK(exact.assignments[2].slotIndex!=exact.assignments[0].slotIndex);
	Fixture(&policy,requests);requests[2].compatibilityKey=9u;
	CHECK(Ral_TransientPlanBuild(&policy,requests,4u,&exact));
	CHECK(exact.assignments[2].slotIndex!=exact.assignments[0].slotIndex);
	Fixture(&policy,requests);requests[2].allowAlias=qfalse;
	CHECK(Ral_TransientPlanBuild(&policy,requests,4u,&exact));
	CHECK(exact.assignments[2].slotIndex!=exact.assignments[0].slotIndex);

#define REJECT_PLAN(count) do { memset(&plan,0x5a,sizeof(plan));beforePlan=plan; \
	CHECK(!Ral_TransientPlanBuild(&policy,requests,(count),&plan)); \
	CHECK(memcmp(&plan,&beforePlan,sizeof(plan))==0);Fixture(&policy,requests); } while(0)
	requests[3].firstPass=1u;REJECT_PLAN(4u);
	requests[1].resourceIdentity=(uintptr_t)50u;REJECT_PLAN(4u);
	requests[2].resourceIdentity=requests[0].resourceIdentity;REJECT_PLAN(4u);
	requests[0].firstPass=2u;requests[0].lastPass=1u;REJECT_PLAN(4u);
	requests[0].alignment=3u;REJECT_PLAN(4u);
	requests[0].size=UINT64_MAX;REJECT_PLAN(4u);
	requests[0].compatibilityKey=0u;REJECT_PLAN(4u);
	requests[0].allowAlias=(qboolean)2;REJECT_PLAN(4u);
	policy.budgetBytes=2048u;REJECT_PLAN(4u);
	REJECT_PLAN(RAL_TRANSIENT_MAX_REQUESTS+1u);
#undef REJECT_PLAN
	CHECK(Ral_TransientPlanBuild(&policy,requests,4u,&plan));

	Ral_TransientBatchLifecycleInit(&lifecycle);
	CHECK(Ral_TransientBatchBegin(&lifecycle,&plan,1u,&planned));
	CHECK(planned.state==RAL_TRANSIENT_BATCH_PLANNED&&planned.planIdentity==&plan);
#define REJECT_TRANSITION(call) do { beforeLifecycle=lifecycle;memset(&bad,0x5a,sizeof(bad));beforeReceipt=bad; \
	CHECK(!(call));CHECK(memcmp(&lifecycle,&beforeLifecycle,sizeof(lifecycle))==0); \
	CHECK(memcmp(&bad,&beforeReceipt,sizeof(bad))==0); } while(0)
	REJECT_TRANSITION(Ral_TransientBatchBegin(&lifecycle,&plan,1u,&bad));
	exact=plan;REJECT_TRANSITION(Ral_TransientBatchSubmit(&lifecycle,&planned,&exact,5u,&bad));
	exact=plan;requests[0].resourceGeneration++;
	CHECK(Ral_TransientPlanBuild(&policy,requests,4u,&plan));
	REJECT_TRANSITION(Ral_TransientBatchSubmit(&lifecycle,&planned,&plan,5u,&bad));
	plan=exact;
	CHECK(Ral_TransientBatchSubmit(&lifecycle,&planned,&plan,5u,&submitted));
	CHECK(submitted.state==RAL_TRANSIENT_BATCH_SUBMITTED&&submitted.submissionGeneration==5u);
	REJECT_TRANSITION(Ral_TransientBatchCancel(&lifecycle,&submitted,&plan,&bad));
	REJECT_TRANSITION(Ral_TransientBatchRetire(&lifecycle,&submitted,&plan,qfalse,&bad));
	bad=submitted;bad.commandSlot=2u;
	beforeLifecycle=lifecycle;CHECK(!Ral_TransientBatchRetire(&lifecycle,&bad,&plan,qtrue,&retired));
	CHECK(memcmp(&lifecycle,&beforeLifecycle,sizeof(lifecycle))==0);
	CHECK(Ral_TransientBatchRetire(&lifecycle,&submitted,&plan,qtrue,&retired));
	CHECK(retired.state==RAL_TRANSIENT_BATCH_RETIRED);
	CHECK(Ral_TransientBatchBegin(&lifecycle,&plan,0u,&planned));
	CHECK(Ral_TransientBatchCancel(&lifecycle,&planned,&plan,&canceled));
	CHECK(canceled.state==RAL_TRANSIENT_BATCH_CANCELED&&canceled.submissionGeneration==0u);
	lifecycle.nextBatchGeneration=UINT64_MAX-1u;
	REJECT_TRANSITION(Ral_TransientBatchBegin(&lifecycle,&plan,0u,&bad));
#undef REJECT_TRANSITION
	puts("ral transient plan: PASS");
	return 0;
}
