// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_transfer.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if(!(x)){fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;} } while(0)
#define CHECK_UPLOAD_MUTATION(statement) do { badUpload=upload; statement; \
	CHECK(!Ral_BufferUploadReceiptExact(&upload,&badUpload)); \
} while(0)

int main( void ) {
	ralTransferRequest_t request;ralTransferReceipt_t prepared,submitted,completed,before;
	ralBufferUploadReceipt_t upload, uploadBefore, badUpload;
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
	memset(&request,0,sizeof(request));request.backendType=RAL_BACKEND_WEBGPU;
	request.direction=RAL_TRANSFER_UPLOAD;request.resourceKind=RAL_TRANSFER_BUFFER;
	request.resourceIdentity=(uintptr_t)0x400u;request.resourceGeneration=5u;
	request.byteOffset=64u;request.byteSize=192u;request.byteBudget=256u;
	request.queue=RAL_QUEUE_GRAPHICS;
	CHECK(Ral_TransferPrepare(&request,10u,&prepared));
	CHECK(Ral_TransferPublish(&prepared,RAL_TRANSFER_OUTCOME_MANAGED_ASYNC,11u,&submitted));
	CHECK(Ral_TransferComplete(&submitted,12u,qtrue,&completed));
	memset(&uploadBefore,0x35,sizeof(uploadBefore));upload=uploadBefore;
	CHECK(Ral_BufferUploadReceiptBuild(&completed,12u,&upload));
	// WebGPU lowers the public immediate-write semantic to queue.writeBuffer:
	// a graphics-queue managed write whose completed receipt stays bound to the
	// exact buffer allocation, range, byte budget and visibility generation.
	CHECK(Ral_BufferUploadReceiptExact(&upload,&upload)
		&& upload.transfer.request.backendType==RAL_BACKEND_WEBGPU
		&& upload.transfer.request.resourceKind==RAL_TRANSFER_BUFFER
		&& upload.transfer.request.resourceIdentity==(uintptr_t)0x400u
		&& upload.transfer.request.resourceGeneration==5u
		&& upload.transfer.request.byteOffset==64u
		&& upload.transfer.request.byteSize==192u
		&& upload.transfer.request.byteBudget==256u
		&& upload.transfer.request.queue==RAL_QUEUE_GRAPHICS
		&& upload.transfer.outcome==RAL_TRANSFER_OUTCOME_MANAGED_ASYNC
		&& upload.transfer.transferGeneration==10u
		&& upload.transfer.submissionGeneration==11u
		&& upload.transfer.completionGeneration==12u
		&& upload.graphicsVisibilityGeneration==12u);
	CHECK_UPLOAD_MUTATION(badUpload.ready=qfalse);
	CHECK_UPLOAD_MUTATION(badUpload.transfer.request.backendType=RAL_BACKEND_VULKAN);
	CHECK_UPLOAD_MUTATION(badUpload.transfer.request.resourceIdentity=(uintptr_t)0x401u);
	CHECK_UPLOAD_MUTATION(badUpload.transfer.request.resourceGeneration=6u);
	CHECK_UPLOAD_MUTATION(badUpload.transfer.request.byteOffset=63u);
	CHECK_UPLOAD_MUTATION(badUpload.transfer.request.byteSize=191u);
	CHECK_UPLOAD_MUTATION(badUpload.transfer.request.byteBudget=257u);
	CHECK_UPLOAD_MUTATION(badUpload.transfer.outcome=RAL_TRANSFER_OUTCOME_NATIVE_ASYNC);
	CHECK_UPLOAD_MUTATION(badUpload.transfer.transferGeneration=13u);
	CHECK_UPLOAD_MUTATION(badUpload.transfer.completionGeneration=13u);
	CHECK_UPLOAD_MUTATION(badUpload.graphicsVisibilityGeneration=13u);
	badUpload=uploadBefore;
	CHECK(!Ral_BufferUploadReceiptBuild(&completed,11u,&badUpload));
	CHECK(!memcmp(&badUpload,&uploadBefore,sizeof(badUpload)));
	puts("ral webgpu transfer lifecycle: PASS");return 0;
}
