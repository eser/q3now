// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_transfer.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if(!(x)){fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;} } while(0)
#define MUTATE(field,value) do { bad=prepared;bad.field=(value);CHECK(!Ral_TransferReceiptExact(&prepared,&bad)); } while(0)

static ralTransferRequest_t TextureRequest( void ) {
	ralTransferRequest_t request;memset(&request,0,sizeof(request));
	request.backendType=RAL_BACKEND_VULKAN;request.direction=RAL_TRANSFER_UPLOAD;
	request.resourceKind=RAL_TRANSFER_TEXTURE;request.resourceIdentity=(uintptr_t)0x100u;
	request.resourceGeneration=7u;request.byteSize=4096u;request.byteBudget=8192u;
	request.mipLevel=2u;request.arrayLayer=1u;request.offsetX=4u;request.offsetY=8u;
	request.width=32u;request.height=16u;request.depth=1u;request.queue=RAL_QUEUE_TRANSFER;
	return request;
}

int main( void ) {
	ralTransferRequest_t request=TextureRequest();
	ralTransferReceipt_t prepared,submitted,completed,canceled,before,bad;
	ralBufferUploadReceipt_t uploadReceipt, uploadBefore, uploadBad;
	memset(&before,0x5a,sizeof(before));prepared=before;
	CHECK(Ral_TransferPrepare(&request,11u,&prepared));CHECK(prepared.state==RAL_TRANSFER_PREPARED);
	CHECK(Ral_TransferReceiptExact(&prepared,&prepared));
	MUTATE(request.resourceIdentity,(uintptr_t)0x101u);
	MUTATE(request.resourceGeneration,8u);MUTATE(request.byteSize,2048u);
	MUTATE(request.mipLevel,3u);MUTATE(request.arrayLayer,2u);MUTATE(request.offsetZ,1u);
	MUTATE(request.textureAspects,1u);MUTATE(request.queue,RAL_QUEUE_GRAPHICS);
	MUTATE(transferGeneration,12u);
	bad=before;CHECK(!Ral_TransferPublish(&prepared,RAL_TRANSFER_OUTCOME_NONE,12u,&bad));
	CHECK(!memcmp(&bad,&before,sizeof(bad)));
	CHECK(Ral_TransferPublish(&prepared,RAL_TRANSFER_OUTCOME_NATIVE_ASYNC,12u,&submitted));
	CHECK(submitted.state==RAL_TRANSFER_SUBMITTED&&!submitted.completionGeneration);
	bad=before;CHECK(!Ral_TransferComplete(&submitted,13u,qfalse,&bad));CHECK(!memcmp(&bad,&before,sizeof(bad)));
	CHECK(Ral_TransferComplete(&submitted,13u,qtrue,&completed));
	CHECK(completed.state==RAL_TRANSFER_COMPLETED&&completed.completionGeneration==13u);
	CHECK(!Ral_TransferComplete(&completed,14u,qtrue,&bad));
	CHECK(Ral_TransferPublish(&prepared,RAL_TRANSFER_OUTCOME_SYNCHRONOUS,15u,&completed));
	CHECK(completed.state==RAL_TRANSFER_COMPLETED&&completed.completionGeneration==15u);
	CHECK(Ral_TransferCancel(&prepared,&canceled));CHECK(canceled.state==RAL_TRANSFER_CANCELED);
	CHECK(!Ral_TransferCancel(&submitted,&bad));
	request.byteBudget=4095u;bad=before;CHECK(!Ral_TransferPrepare(&request,20u,&bad));CHECK(!memcmp(&bad,&before,sizeof(bad)));
	request=TextureRequest();request.resourceKind=RAL_TRANSFER_BUFFER;bad=before;
	CHECK(!Ral_TransferPrepare(&request,20u,&bad));CHECK(!memcmp(&bad,&before,sizeof(bad)));
	memset(&request,0,sizeof(request));request.backendType=RAL_BACKEND_VULKAN;
	request.direction=RAL_TRANSFER_READBACK;request.resourceKind=RAL_TRANSFER_BUFFER;
	request.resourceIdentity=(uintptr_t)0x200u;request.resourceGeneration=9u;
	request.byteOffset=128u;request.byteSize=512u;request.byteBudget=1024u;request.queue=RAL_QUEUE_GRAPHICS;
	CHECK(Ral_TransferPrepare(&request,21u,&prepared));
	request.direction=RAL_TRANSFER_UPLOAD;request.byteOffset=768u;request.byteSize=512u;
	bad=before;CHECK(!Ral_TransferPrepare(&request,22u,&bad));CHECK(!memcmp(&bad,&before,sizeof(bad)));
	request.byteOffset=512u;CHECK(Ral_TransferPrepare(&request,22u,&prepared));
	CHECK(Ral_TransferPublish(&prepared,RAL_TRANSFER_OUTCOME_NATIVE_ASYNC,23u,&submitted));
	CHECK(Ral_TransferComplete(&submitted,24u,qtrue,&completed));
	memset(&uploadBefore,0x66,sizeof(uploadBefore));uploadReceipt=uploadBefore;
	CHECK(Ral_BufferUploadReceiptBuild(&completed,24u,&uploadReceipt));
	CHECK(Ral_BufferUploadReceiptExact(&uploadReceipt,&uploadReceipt));
	uploadBad=uploadReceipt;uploadBad.graphicsVisibilityGeneration++;
	CHECK(!Ral_BufferUploadReceiptExact(&uploadReceipt,&uploadBad));
	uploadBad=uploadReceipt;uploadBad.transfer.request.byteOffset++;
	CHECK(!Ral_BufferUploadReceiptExact(&uploadReceipt,&uploadBad));
	uploadBad=uploadBefore;
	CHECK(!Ral_BufferUploadReceiptBuild(&completed,23u,&uploadBad));
	CHECK(!memcmp(&uploadBad,&uploadBefore,sizeof(uploadBad)));
	puts("ral transfer lifecycle: PASS");return 0;
}
