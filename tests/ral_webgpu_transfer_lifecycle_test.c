// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_transfer.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if(!(x)){fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;} } while(0)

int main( void ) {
	ralTransferRequest_t request;ralTransferReceipt_t prepared,submitted,completed,before;
	memset(&request,0,sizeof(request));request.backendType=RAL_BACKEND_WEBGPU;
	request.direction=RAL_TRANSFER_UPLOAD;request.resourceKind=RAL_TRANSFER_TEXTURE;
	request.resourceIdentity=(uintptr_t)0x300u;request.resourceGeneration=4u;
	request.byteSize=256u;request.byteBudget=256u;request.width=8u;request.height=8u;
	request.depth=1u;request.queue=RAL_QUEUE_GRAPHICS;
	CHECK(Ral_TransferPrepare(&request,5u,&prepared));
	CHECK(Ral_TransferPublish(&prepared,RAL_TRANSFER_OUTCOME_MANAGED_ASYNC,6u,&submitted));
	CHECK(submitted.state==RAL_TRANSFER_SUBMITTED&&submitted.request.queue==RAL_QUEUE_GRAPHICS);
	memset(&before,0x4c,sizeof(before));completed=before;
	CHECK(!Ral_TransferComplete(&submitted,7u,qfalse,&completed));CHECK(!memcmp(&completed,&before,sizeof(completed)));
	CHECK(Ral_TransferComplete(&submitted,7u,qtrue,&completed));
	CHECK(completed.state==RAL_TRANSFER_COMPLETED);
	request.queue=RAL_QUEUE_TRANSFER;CHECK(Ral_TransferPrepare(&request,8u,&prepared));
	CHECK(Ral_TransferPublish(&prepared,RAL_TRANSFER_OUTCOME_MANAGED_ASYNC,9u,&submitted));
	puts("ral webgpu transfer lifecycle: PASS");return 0;
}
