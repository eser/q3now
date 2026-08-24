// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_proxy.h"

#include <limits.h>
#include <string.h>

#define FNV64_OFFSET UINT64_C(14695981039346656037)
#define FNV64_PRIME UINT64_C(1099511628211)

static const int32_t sineQ16[16] = {
	0, 25080, 46341, 60547, 65536, 60547, 46341, 25080,
	0, -25080, -46341, -60547, -65536, -60547, -46341, -25080
};

static uint64_t HashByte( uint64_t h, unsigned char v ) { return ( h ^ v ) * FNV64_PRIME; }
static uint64_t HashU32( uint64_t h, uint32_t v ) { uint32_t i; for(i=0u;i<4u;i++)h=HashByte(h,(unsigned char)(v>>(i*8u)));return h; }
static uint64_t HashU64( uint64_t h, uint64_t v ) { uint32_t i; for(i=0u;i<8u;i++)h=HashByte(h,(unsigned char)(v>>(i*8u)));return h; }

static qboolean ProgramValid( const ralMaterialProxyProgram_t *p ) {
	uint32_t i;
	qboolean hasWriter = qfalse;
	ralMaterialProxyTarget_t target = RAL_MATERIAL_PROXY_BASE_COLOR_R;
	if ( !p || p->schemaVersion != RAL_MATERIAL_PROXY_SCHEMA_VERSION
			|| !p->materialArtifactHash
			|| p->instructionCount > RAL_MATERIAL_PROXY_MAX_INSTRUCTIONS ) return qfalse;
	for ( i=0u; i<p->instructionCount; i++ ) {
		const ralMaterialProxyInstruction_t *v=&p->instructions[i];
		qboolean writer;
		if ( v->target < RAL_MATERIAL_PROXY_BASE_COLOR_R
				|| v->target > RAL_MATERIAL_PROXY_ALPHA_CUTOFF
				|| v->opcode < RAL_MATERIAL_PROXY_SET || v->opcode > RAL_MATERIAL_PROXY_SINE )
			return qfalse;
		if ( i && v->target < target ) return qfalse;
		if ( !i || v->target != target ) { target=v->target; hasWriter=qfalse; }
		writer = v->opcode==RAL_MATERIAL_PROXY_SET || v->opcode==RAL_MATERIAL_PROXY_LINEAR
			|| v->opcode==RAL_MATERIAL_PROXY_SINE;
		if ( writer && hasWriter ) return qfalse;
		if ( writer ) hasWriter=qtrue;
		if ( (v->opcode==RAL_MATERIAL_PROXY_SET || v->opcode==RAL_MATERIAL_PROXY_ADD
				|| v->opcode==RAL_MATERIAL_PROXY_MULTIPLY)
				? v->input!=RAL_MATERIAL_PROXY_INPUT_NONE
				: v->opcode==RAL_MATERIAL_PROXY_LINEAR
					? v->input!=RAL_MATERIAL_PROXY_INPUT_SCALAR
					: v->input!=RAL_MATERIAL_PROXY_INPUT_TIME ) return qfalse;
	}
	return qtrue;
}

static uint64_t ProgramHash( const ralMaterialProxyProgram_t *p ) {
	uint64_t h=FNV64_OFFSET;uint32_t i;
	h=HashU32(h,RAL_MATERIAL_PROXY_RECEIPT_SCHEMA_VERSION);
	h=HashU64(h,p->materialArtifactHash);h=HashU32(h,p->instructionCount);
	for(i=0u;i<p->instructionCount;i++){
		const ralMaterialProxyInstruction_t*v=&p->instructions[i];
		h=HashU32(h,(uint32_t)v->target);h=HashU32(h,(uint32_t)v->opcode);
		h=HashU32(h,(uint32_t)v->input);h=HashU32(h,(uint32_t)v->aQ16);
		h=HashU32(h,(uint32_t)v->bQ16);h=HashU32(h,(uint32_t)v->cQ16);
	}
	return h?h:1u;
}

qboolean Ral_MaterialProxyProgramBuild( const ralMaterialReceipt_t *material,
		const ralMaterialProxyProgram_t *program,
		ralMaterialProxyProgramReceipt_t *out ) {
	ralMaterialProxyProgramReceipt_t v;
	if(!out||!Ral_MaterialReceiptValid(material)||!ProgramValid(program)
			||program->materialArtifactHash!=material->artifactHash)return qfalse;
	memset(&v,0,sizeof(v));v.schemaVersion=RAL_MATERIAL_PROXY_RECEIPT_SCHEMA_VERSION;
	v.program=*program;v.programHash=ProgramHash(program);v.ready=qtrue;*out=v;return qtrue;
}

qboolean Ral_MaterialProxyProgramReceiptValid(
		const ralMaterialProxyProgramReceipt_t *r ) {
	return r&&r->schemaVersion==RAL_MATERIAL_PROXY_RECEIPT_SCHEMA_VERSION
		&&r->ready==qtrue&&ProgramValid(&r->program)&&r->programHash==ProgramHash(&r->program);
}

static qboolean Add( int32_t a,int32_t b,int32_t*out ) {
	int64_t v=(int64_t)a+b;if(v<INT32_MIN||v>INT32_MAX)return qfalse;*out=(int32_t)v;return qtrue;
}
static qboolean Mul( int32_t a,int32_t b,int32_t*out ) {
	int64_t v=((int64_t)a*b)/RAL_MATERIAL_PROXY_Q16_ONE;
	if(v<INT32_MIN||v>INT32_MAX)return qfalse;*out=(int32_t)v;return qtrue;
}
static qboolean Lerp( int32_t a,int32_t b,int32_t t,int32_t*out ) {
	int64_t v=(int64_t)a+((int64_t)b-a)*t/RAL_MATERIAL_PROXY_Q16_ONE;
	if(v<INT32_MIN||v>INT32_MAX)return qfalse;*out=(int32_t)v;return qtrue;
}
static qboolean FloatQ16( float f,int32_t*out ) {
	double v=(double)f*RAL_MATERIAL_PROXY_Q16_ONE;
	if(v<INT32_MIN||v>INT32_MAX)return qfalse;*out=(int32_t)v;return qtrue;
}

static qboolean BaseValues( const ralMaterialReceipt_t*m,int32_t*v ) {
	uint32_t i;
	for(i=0u;i<4u;i++)if(!FloatQ16(m->material.factors.baseColor[i],&v[i]))return qfalse;
	for(i=0u;i<3u;i++)if(!FloatQ16(m->material.factors.emissive[i],&v[4u+i]))return qfalse;
	return FloatQ16(m->material.factors.metallic,&v[7])
		&&FloatQ16(m->material.factors.roughness,&v[8])
		&&FloatQ16(m->material.factors.normalScale,&v[9])
		&&FloatQ16(m->material.factors.occlusionStrength,&v[10])
		&&FloatQ16(m->material.renderPolicy.alphaCutoff,&v[11]);
}

qboolean Ral_MaterialProxyEvaluate( const ralMaterialReceipt_t *material,
		const ralMaterialProxyProgramReceipt_t *program,
		const ralMaterialProxyInputs_t *inputs, ralMaterialProxyDomain_t domain,
		uint32_t capacity, ralMaterialProxyEvaluationReceipt_t *out ) {
	ralMaterialProxyEvaluationReceipt_t v;
	uint32_t i;
	if(!out||!inputs||capacity<RAL_MATERIAL_PROXY_VALUE_COUNT
			||(domain!=RAL_MATERIAL_PROXY_DOMAIN_RUNTIME&&domain!=RAL_MATERIAL_PROXY_DOMAIN_PREVIEW)
			||!inputs->evaluationGeneration||inputs->scalarQ16<0
			||inputs->scalarQ16>RAL_MATERIAL_PROXY_Q16_ONE
			||!Ral_MaterialReceiptValid(material)||!Ral_MaterialProxyProgramReceiptValid(program)
			||program->program.materialArtifactHash!=material->artifactHash)return qfalse;
	memset(&v,0,sizeof(v));if(!BaseValues(material,v.valuesQ16))return qfalse;
	for(i=0u;i<program->program.instructionCount;i++){
		const ralMaterialProxyInstruction_t*p=&program->program.instructions[i];
		int32_t *value=&v.valuesQ16[p->target],temp;
		switch(p->opcode){
		case RAL_MATERIAL_PROXY_SET:*value=p->aQ16;break;
		case RAL_MATERIAL_PROXY_ADD:if(!Add(*value,p->aQ16,value))return qfalse;break;
		case RAL_MATERIAL_PROXY_MULTIPLY:if(!Mul(*value,p->aQ16,value))return qfalse;break;
		case RAL_MATERIAL_PROXY_LINEAR:
			if(!Lerp(p->aQ16,p->bQ16,inputs->scalarQ16,value))return qfalse;break;
		case RAL_MATERIAL_PROXY_SINE:{
			uint64_t rate=p->cQ16<0?(uint64_t)(-(int64_t)p->cQ16):(uint64_t)p->cQ16;
			uint64_t cycles;uint32_t index;
			if(rate&&inputs->timeMilliseconds>UINT64_MAX/rate)return qfalse;
			cycles=(inputs->timeMilliseconds*rate)/1000u;
			index=(uint32_t)(cycles>>12u)&15u;
			if(p->cQ16<0)index=(16u-index)&15u;
			if(!Mul(p->bQ16,sineQ16[index],&temp)||!Add(p->aQ16,temp,value))return qfalse;
			break;}
		default:return qfalse;
		}
	}
	v.schemaVersion=RAL_MATERIAL_PROXY_EVALUATION_SCHEMA_VERSION;
	v.evaluationGeneration=inputs->evaluationGeneration;v.materialArtifactHash=material->artifactHash;
	v.programHash=program->programHash;v.timeMilliseconds=inputs->timeMilliseconds;
	v.scalarQ16=inputs->scalarQ16;v.valueCount=RAL_MATERIAL_PROXY_VALUE_COUNT;v.ready=qtrue;
	*out=v;return qtrue;
}

static qboolean EvaluationValid(const ralMaterialProxyEvaluationReceipt_t*r){
	return r&&r->schemaVersion==RAL_MATERIAL_PROXY_EVALUATION_SCHEMA_VERSION
		&&r->evaluationGeneration&&r->materialArtifactHash&&r->programHash
		&&r->scalarQ16>=0&&r->scalarQ16<=RAL_MATERIAL_PROXY_Q16_ONE
		&&r->valueCount==RAL_MATERIAL_PROXY_VALUE_COUNT&&r->ready==qtrue;
}
qboolean Ral_MaterialProxyEvaluationReceiptExact(
		const ralMaterialProxyEvaluationReceipt_t*a,
		const ralMaterialProxyEvaluationReceipt_t*b){
	return EvaluationValid(a)&&EvaluationValid(b)&&!memcmp(a,b,sizeof(*a));
}
