// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

struct Params {
	uint32_t outputGrid[4];
	float depthRange[4], inverse[16], eyeSky[4];
};
static_assert(sizeof(Params) == 112, "canonical composite UBO");

// Execute the actual portable composite. Each froxel carries its own address
// as radiance, so the CPU oracle tests lookup, not a duplicate shader formula.
int main(int argc, const char **argv) {
 @autoreleasepool {
	CHECK(argc == 2 || (argc == 3 && !strcmp(argv[2], "--transparent-order-audit")));
	const bool auditTransparentOrder = argc == 3;
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	CHECK(device);
	NSError *error = nil;
	NSString *source = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:argv[1]]
		encoding:NSUTF8StringEncoding error:&error];
	CHECK(source);
	id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
	if (!library) fprintf(stderr, "%s\n", error.description.UTF8String);
	CHECK(library);
	id<MTLFunction> function = [library newFunctionWithName:@"main0"];
	id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function error:&error];
	id<MTLCommandQueue> queue = [device newCommandQueue];
	CHECK(pipeline && queue);
	MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
	sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterNearest;
	sd.sAddressMode = sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
	sd.supportArgumentBuffers = YES;
	id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:sd];
	CHECK(sampler);
	const unsigned w = 17, h = 9, gx = 3, gy = 2, slices = 64;
	std::vector<float> media(gx*gy*slices*4);
	for (unsigned z=0; z<slices; ++z) for (unsigned y=0; y<gy; ++y) for (unsigned x=0; x<gx; ++x) {
		unsigned i = ((z*gy+y)*gx+x)*4;
		media[i]=float(z); media[i+1]=float(x); media[i+2]=float(y); media[i+3]=1;
	}
	id<MTLBuffer> mediaBuffer = [device newBufferWithBytes:media.data() length:media.size()*sizeof(float)
		options:MTLResourceStorageModeShared];
	auto texture = [&](MTLPixelFormat format) {
		MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
			width:w height:h mipmapped:NO];
		d.storageMode=MTLStorageModeShared;
		d.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
		return [device newTextureWithDescriptor:d];
	};
	id<MTLTexture> scene=texture(MTLPixelFormatRGBA32Float), depthTexture=texture(MTLPixelFormatR32Float);
	id<MTLTexture> sky=texture(MTLPixelFormatRGBA32Float), output=texture(MTLPixelFormatRGBA16Float);
	CHECK(mediaBuffer && scene && depthTexture && sky && output);
	MTLRegion region=MTLRegionMake2D(0,0,w,h);
	std::vector<float> black(w*h*4,0), skyPixels(w*h*4,0);
	for (unsigned i=0;i<w*h;++i) skyPixels[i*4]=2;
	[scene replaceRegion:region mipmapLevel:0 withBytes:black.data() bytesPerRow:w*16];
	[sky replaceRegion:region mipmapLevel:0 withBytes:skyPixels.data() bytesPerRow:w*16];
	unsigned checked=0;
	const float ranges[][2]={{0.5f,64},{4,4096},{16,65536},{4,4.01f},{4,65536}};
	for (const auto &range:ranges) for (unsigned variant=0;variant<2;++variant) for (unsigned scenario=0;scenario<(auditTransparentOrder?4u:3u);++scenario) {
		const double nearZ=auditTransparentOrder?4:range[0], farZ=auditTransparentOrder?64:range[1];
		const double sceneNear=variant?nearZ*0.25:nearZ;
		// A short media range is not the scene far plane. Keep generated
		// surfaces inside the oblique frustum even for the 4..4.01 grid.
		const double sceneFar=farZ<64 ? 64 : farZ;
		Params p={};
		p.outputGrid[0]=w; p.outputGrid[1]=h; p.outputGrid[2]=gx; p.outputGrid[3]=gy;
		p.depthRange[0]=nearZ; p.depthRange[1]=farZ; p.depthRange[2]=scenario==2?0:1;
		p.depthRange[3]=slices; p.eyeSky[3]=scenario==1?1:0;
		// Inverse of finite reversed-Z perspective with unit x/y scale.
		// w_clip = -z_eye. Translation does not change inverse homogeneous w.
		p.inverse[0]=p.inverse[5]=1; p.inverse[14]=-1;
		p.inverse[11]=(sceneFar-sceneNear)/(sceneFar*sceneNear); p.inverse[15]=1/sceneFar;
		// Scene clipping is independent of the integration range. Exercise both
		// a shrunk near plane and an oblique clip plane (w depends on screen XY).
		p.inverse[3]=variant?0.005/sceneFar:0; p.inverse[7]=variant?-0.003/sceneFar:0;
		std::vector<float> depths(w*h);
		std::vector<unsigned> expected(w*h);
		for (unsigned i=0;i<w*h;++i) {
			unsigned z=i%slices;
			double ndcX=2.0*((i%w)+0.5)/w-1.0, ndcY=2.0*((i/w)+0.5)/h-1.0;
			double wOffset=p.inverse[15]+p.inverse[3]*ndcX+p.inverse[7]*ndcY;
			double distance=nearZ*std::pow(farZ/nearZ,(z+0.5)/slices);
			depths[i]=float((1.0/distance-wOffset)/p.inverse[11]);
			CHECK(depths[i]>0 && depths[i]<=1);
			expected[i]=z;
			if (scenario==1) {
				// Real far geometry must never be replaced with sky merely because
				// its reversed depth is small. Zero alone is the clear sentinel.
				depths[i]=(i%2)?0.0005f:0;
				distance=1.0/(wOffset+depths[i]*p.inverse[11]);
				expected[i]=slices-1;
				for (unsigned s=0;depths[i]>0 && s<slices;++s) if (distance<nearZ*std::pow(farZ/nearZ,double(s+1)/slices)) {
					expected[i]=s; break;
				}
			}
			if (scenario==2) expected[i]=slices-1;
		}
		if (scenario==3) {
			// Current Vulkan ordering composites an already-added foreground
			// emission using background opaque depth. Reproduce that exact input
			// to the actual composite shader, then require the physical result.
			// This opt-in RED diagnostic is not a substitute for end-to-end beam
			// raster/descriptor validation when the production ordering is fixed.
			for(unsigned z=0;z<slices;++z)for(unsigned y=0;y<gy;++y)for(unsigned x=0;x<gx;++x) {
				const unsigned i=((z*gy+y)*gx+x)*4;
				const double distance=nearZ*std::pow(farZ/nearZ,(z+0.5)/slices);
				const float t=std::exp(-distance/32.0);
				media[i]=.1f*(1-t);media[i+1]=.2f*(1-t);media[i+2]=.3f*(1-t);media[i+3]=t;
			}
			memcpy(mediaBuffer.contents,media.data(),media.size()*sizeof(float));
			// Wall in slice48, beam in slice16, both centres avoid slice rounding.
			const double wall=nearZ*std::pow(farZ/nearZ,48.5/slices);
			for(unsigned i=0;i<w*h;++i) {
				const double x=2.0*((i%w)+.5)/w-1, y=2.0*((i/w)+.5)/h-1;
				depths[i]=float((1/wall-p.inverse[15]-p.inverse[3]*x-p.inverse[7]*y)/p.inverse[11]);
				for(unsigned c=0;c<3;++c)black[i*4+c]=.2f+.8f*.5f;
				black[i*4+3]=1;
			}
			[scene replaceRegion:region mipmapLevel:0 withBytes:black.data() bytesPerRow:w*16];
		}
		[depthTexture replaceRegion:region mipmapLevel:0 withBytes:depths.data() bytesPerRow:w*4];
		id<MTLBuffer> params=[device newBufferWithBytes:&p length:sizeof(p) options:MTLResourceStorageModeShared];
		id<MTLArgumentEncoder> args=[function newArgumentEncoderWithBufferIndex:0];
		id<MTLBuffer> bindings=[device newBufferWithLength:args.encodedLength options:MTLResourceStorageModeShared];
		CHECK(params && args && bindings);
		[args setArgumentBuffer:bindings offset:0];
		[args setBuffer:params offset:0 atIndex:0];
		[args setTexture:depthTexture atIndex:1]; [args setSamplerState:sampler atIndex:2];
		[args setBuffer:mediaBuffer offset:0 atIndex:3];
		[args setTexture:scene atIndex:4]; [args setSamplerState:sampler atIndex:5];
		[args setTexture:sky atIndex:6]; [args setSamplerState:sampler atIndex:7];
		[args setTexture:output atIndex:8];
		id<MTLCommandBuffer> command=[queue commandBuffer];
		id<MTLComputeCommandEncoder> encoder=[command computeCommandEncoder];
		[encoder setComputePipelineState:pipeline]; [encoder setBuffer:bindings offset:0 atIndex:0];
		for (id<MTLResource> r in @[params,mediaBuffer,scene,depthTexture,sky]) [encoder useResource:r usage:MTLResourceUsageRead];
		[encoder useResource:output usage:MTLResourceUsageWrite];
		[encoder dispatchThreadgroups:MTLSizeMake((w+7)/8,(h+7)/8,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];
		[encoder endEncoding]; [command commit]; [command waitUntilCompleted];
		CHECK(command.status==MTLCommandBufferStatusCompleted);
		std::vector<_Float16> pixels(w*h*4);
		[output getBytes:pixels.data() bytesPerRow:w*8 fromRegion:region mipmapLevel:0];
		if(scenario==3) {
			const double wall=nearZ*std::pow(farZ/nearZ,48.5/slices);
			const double beam=nearZ*std::pow(farZ/nearZ,16.5/slices);
			const double wallT=std::exp(-wall/32), beamT=std::exp(-beam/32);
			const double expected=.2*wallT+.1*(1-wallT)+.8*.5*beamT;
			const double lateComposite=(.2+.8*.5)*wallT+.1*(1-wallT);
			fprintf(stderr,"transparent-order GPU: beam=%g wall=%g actual=%g late-composite=%g required=%g\n",
				beam,wall,float(pixels[0]),lateComposite,expected);
			CHECK(std::fabs(float(pixels[0])-lateComposite)<.001);
			CHECK(std::fabs(float(pixels[0])-expected)<.001);
			return 0;
		}
		for (unsigned y=0;y<h;++y) for (unsigned x=0;x<w;++x) {
			unsigned i=y*w+x;
			float want=expected[i]+((scenario==1 && depths[i]==0)?2:0);
			if (float(pixels[i*4])!=want) fprintf(stderr,"near=%g far=%g case=%u pixel=%u depth=%g actual=%g expected=%g\n",
				nearZ,farZ,scenario,i,depths[i],float(pixels[i*4]),want);
			CHECK(float(pixels[i*4])==want);
			CHECK(float(pixels[i*4+1])==std::floor((x+0.5)*gx/w));
			CHECK(float(pixels[i*4+2])==std::floor((y+0.5)*gy/h));
			CHECK(float(pixels[i*4+3])==1);
			++checked;
		}
	}
	printf("canonical atmosphere depth GPU: %u cases, %u pixels PASS\n",checked/(w*h),checked);
 }
	return 0;
}
