#ifndef WIRED_RAL_OPENGL_PIPELINE_H
#define WIRED_RAL_OPENGL_PIPELINE_H
#include "ral_shader_abi.h"
#define RAL_OPENGL_PIPELINE_SCHEMA_VERSION 1u
typedef enum { RAL_OPENGL_BIND_UNIFORM=1,RAL_OPENGL_BIND_STORAGE,RAL_OPENGL_BIND_TEXTURE,RAL_OPENGL_BIND_IMAGE,RAL_OPENGL_BIND_SAMPLER,RAL_OPENGL_BIND_COUNT } ralOpenGlBindingNamespace_t;
typedef struct { uint32_t set,binding; ralShaderBindingClass_t bindingClass; ralOpenGlBindingNamespace_t bindingNamespace; uint32_t glBinding,portableArrayCount,loweredArrayCount; } ralOpenGlBindingReceipt_t;
typedef struct { const ralShaderAbiManifest_t *manifest; uint64_t generation; ralShaderDigest_t catalogDigest; const char *vertex,*fragment; uint32_t vertexBytes,fragmentBytes; } ralOpenGlPipelineInfo_t;
typedef struct { uint32_t schemaVersion; ralBackendType_t backendType; uint64_t manifestGeneration,generation; uintptr_t identity; ralShaderDigest_t catalogDigest,glslDigests[2]; ralOpenGlBindingReceipt_t bindings[RAL_SHADER_ABI_MAX_BINDINGS]; uint32_t bindingCount; qboolean ready; } ralOpenGlPipelineReceipt_t;
qboolean RalOpenGl_PipelineBuild(const ralOpenGlPipelineInfo_t*,ralOpenGlPipelineReceipt_t*);
qboolean RalOpenGl_PipelineExact(const ralOpenGlPipelineReceipt_t*,const ralOpenGlPipelineReceipt_t*);
#endif
