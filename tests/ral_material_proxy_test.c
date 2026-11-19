// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_proxy.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)

static int Material(ralMaterialReceipt_t*out){
	ralMaterialDescription_t d;memset(&d,0,sizeof(d));d.schemaVersion=RAL_MATERIAL_SCHEMA_VERSION;
	d.materialGeneration=9u;d.provenanceHash=99u;d.source=RAL_MATERIAL_SOURCE_PBR;
	d.factors.baseColor[0]=d.factors.baseColor[1]=d.factors.baseColor[2]=d.factors.baseColor[3]=1.0f;
	d.factors.roughness=0.5f;d.factors.normalScale=d.factors.occlusionStrength=1.0f;
	d.renderPolicy.depthTest=d.renderPolicy.depthWrite=qtrue;return Ral_MaterialCompile(&d,out);
}

static ralMaterialProxyProgram_t Program(const ralMaterialReceipt_t*m){
	ralMaterialProxyProgram_t p;memset(&p,0,sizeof(p));p.schemaVersion=RAL_MATERIAL_PROXY_SCHEMA_VERSION;
	p.materialArtifactHash=m->artifactHash;p.instructionCount=3u;
	p.instructions[0].target=RAL_MATERIAL_PROXY_EMISSIVE_R;
	p.instructions[0].opcode=RAL_MATERIAL_PROXY_SINE;p.instructions[0].input=RAL_MATERIAL_PROXY_INPUT_TIME;
	p.instructions[0].aQ16=RAL_MATERIAL_PROXY_Q16_ONE;p.instructions[0].bQ16=RAL_MATERIAL_PROXY_Q16_ONE/2;
	p.instructions[0].cQ16=RAL_MATERIAL_PROXY_Q16_ONE;
	p.instructions[1].target=RAL_MATERIAL_PROXY_ROUGHNESS;
	p.instructions[1].opcode=RAL_MATERIAL_PROXY_LINEAR;p.instructions[1].input=RAL_MATERIAL_PROXY_INPUT_SCALAR;
	p.instructions[1].aQ16=RAL_MATERIAL_PROXY_Q16_ONE/4;p.instructions[1].bQ16=RAL_MATERIAL_PROXY_Q16_ONE;
	p.instructions[2].target=RAL_MATERIAL_PROXY_ROUGHNESS;
	p.instructions[2].opcode=RAL_MATERIAL_PROXY_MULTIPLY;p.instructions[2].input=RAL_MATERIAL_PROXY_INPUT_NONE;
	p.instructions[2].aQ16=RAL_MATERIAL_PROXY_Q16_ONE/2;return p;
}

static ralMaterialProxyProgram_t ExpressionProgram(const ralMaterialReceipt_t*m){
	ralMaterialProxyProgram_t p;uint32_t i;memset(&p,0,sizeof(p));
	p.schemaVersion=RAL_MATERIAL_PROXY_SCHEMA_VERSION;p.materialArtifactHash=m->artifactHash;
	p.expressionRegisterCount=40u;p.expressionConstantCount=2u;
	p.expressionInstructionCount=RAL_MATERIAL_EXPRESSION_OPCODE_COUNT;
	p.expressionFieldCount=3u;p.expressionTableCount=1u;
	for(i=0u;i<4u;i++){
		p.expressionConstants[0].lane[i]=(int32_t)(i+1u)*RAL_MATERIAL_PROXY_Q16_ONE;
		p.expressionConstants[1].lane[i]=RAL_MATERIAL_PROXY_Q16_ONE/2;
	}
#define OP(index,type,dst,srcA,srcB) do{p.expressionInstructions[index].opcode=(type); \
	p.expressionInstructions[index].destination=(dst);p.expressionInstructions[index].a=(srcA); \
	p.expressionInstructions[index].b=(srcB);}while(0)
	OP(0,RAL_MATERIAL_EXPRESSION_UNMASKED_MOVE_CONSTANT,14,0,0);
	OP(1,RAL_MATERIAL_EXPRESSION_UNMASKED_MOVE,15,14,0);
	OP(2,RAL_MATERIAL_EXPRESSION_MOVE_CONSTANT,15,1,3);
	OP(3,RAL_MATERIAL_EXPRESSION_MOVE,15,14,4);
	OP(4,RAL_MATERIAL_EXPRESSION_SWIZZLE,16,15,27);
	OP(5,RAL_MATERIAL_EXPRESSION_ADD,17,14,16);
	OP(6,RAL_MATERIAL_EXPRESSION_SUBTRACT,18,17,14);
	OP(7,RAL_MATERIAL_EXPRESSION_MULTIPLY,19,14,RAL_MATERIAL_PROXY_REGISTER_SCALAR);
	OP(8,RAL_MATERIAL_EXPRESSION_DOT3,20,14,14);
	OP(9,RAL_MATERIAL_EXPRESSION_DOT4,21,14,14);
	OP(10,RAL_MATERIAL_EXPRESSION_DIVIDE,22,14,RAL_MATERIAL_PROXY_REGISTER_SCALAR);
	OP(11,RAL_MATERIAL_EXPRESSION_MOD,23,14,RAL_MATERIAL_PROXY_REGISTER_SCALAR);
	OP(12,RAL_MATERIAL_EXPRESSION_GT,24,14,RAL_MATERIAL_PROXY_REGISTER_SCALAR);
	OP(13,RAL_MATERIAL_EXPRESSION_GE,25,14,RAL_MATERIAL_PROXY_REGISTER_SCALAR);
	OP(14,RAL_MATERIAL_EXPRESSION_LT,26,14,RAL_MATERIAL_PROXY_REGISTER_SCALAR);
	OP(15,RAL_MATERIAL_EXPRESSION_LE,27,14,RAL_MATERIAL_PROXY_REGISTER_SCALAR);
	OP(16,RAL_MATERIAL_EXPRESSION_EQ,28,14,15);
	OP(17,RAL_MATERIAL_EXPRESSION_NE,29,14,15);
	OP(18,RAL_MATERIAL_EXPRESSION_AND,30,24,25);
	OP(19,RAL_MATERIAL_EXPRESSION_OR,31,26,24);
	OP(20,RAL_MATERIAL_EXPRESSION_TABLE,32,RAL_MATERIAL_PROXY_REGISTER_SCALAR,0);
#undef OP
	p.expressionTables[0].sampleCount=2u;
	p.expressionTables[0].samplesQ16[0]=0;
	p.expressionTables[0].samplesQ16[1]=RAL_MATERIAL_PROXY_Q16_ONE;
	strcpy(p.expressionFields[0].name,"baseColor");p.expressionFields[0].registerIndex=16u;
	p.expressionFields[0].inlineByteOffset=0u;p.expressionFields[0].componentCount=4u;
	p.expressionFields[0].outputValueOffset=RAL_MATERIAL_PROXY_BASE_COLOR_R;
	strcpy(p.expressionFields[1].name,"emissive");p.expressionFields[1].registerIndex=20u;
	p.expressionFields[1].inlineByteOffset=16u;p.expressionFields[1].componentCount=3u;
	p.expressionFields[1].outputValueOffset=RAL_MATERIAL_PROXY_EMISSIVE_R;
	strcpy(p.expressionFields[2].name,"roughness");p.expressionFields[2].registerIndex=32u;
	p.expressionFields[2].inlineByteOffset=32u;p.expressionFields[2].componentCount=1u;
	p.expressionFields[2].outputValueOffset=RAL_MATERIAL_PROXY_ROUGHNESS;
	return p;
}

int main(void){
	ralMaterialReceipt_t material,stale;ralMaterialProxyProgram_t p,bad;
	ralMaterialProxyProgramReceipt_t program,beforeProgram,expressionProgram,legacyProgram;
	ralMaterialProxyInputs_t inputs;ralMaterialProxyEvaluationReceipt_t runtime,preview,before,expression,legacy;
	CHECK(Material(&material));p=Program(&material);CHECK(Ral_MaterialProxyProgramBuild(&material,&p,&program));
	CHECK(Ral_MaterialProxyProgramReceiptValid(&program));
	memset(&inputs,0,sizeof(inputs));inputs.evaluationGeneration=4u;inputs.timeMilliseconds=250u;
	inputs.scalarQ16=RAL_MATERIAL_PROXY_Q16_ONE/2;
	CHECK(Ral_MaterialProxyEvaluate(&material,&program,&inputs,RAL_MATERIAL_PROXY_DOMAIN_RUNTIME,
		RAL_MATERIAL_PROXY_VALUE_COUNT,&runtime));
	CHECK(Ral_MaterialProxyEvaluate(&material,&program,&inputs,RAL_MATERIAL_PROXY_DOMAIN_PREVIEW,
		RAL_MATERIAL_PROXY_VALUE_COUNT,&preview));
	CHECK(Ral_MaterialProxyEvaluationReceiptExact(&runtime,&preview));
	CHECK(runtime.valuesQ16[RAL_MATERIAL_PROXY_EMISSIVE_R]==RAL_MATERIAL_PROXY_Q16_ONE+RAL_MATERIAL_PROXY_Q16_ONE/2);
	CHECK(runtime.valuesQ16[RAL_MATERIAL_PROXY_ROUGHNESS]==(RAL_MATERIAL_PROXY_Q16_ONE*5)/16);

	before=runtime;CHECK(!Ral_MaterialProxyEvaluate(&material,&program,&inputs,
		RAL_MATERIAL_PROXY_DOMAIN_RUNTIME,RAL_MATERIAL_PROXY_VALUE_COUNT-1u,&runtime)
		&&!memcmp(&runtime,&before,sizeof(runtime)));
	stale=material;stale.artifactHash++;CHECK(!Ral_MaterialProxyEvaluate(&stale,&program,&inputs,
		RAL_MATERIAL_PROXY_DOMAIN_RUNTIME,RAL_MATERIAL_PROXY_VALUE_COUNT,&runtime));
	bad=p;bad.instructions[0].opcode=(ralMaterialProxyOpcode_t)99;
	beforeProgram=program;CHECK(!Ral_MaterialProxyProgramBuild(&material,&bad,&program)
		&&!memcmp(&program,&beforeProgram,sizeof(program)));
	bad=p;bad.instructions[2].opcode=RAL_MATERIAL_PROXY_LINEAR;
	bad.instructions[2].input=RAL_MATERIAL_PROXY_INPUT_SCALAR;
	CHECK(!Ral_MaterialProxyProgramBuild(&material,&bad,&program));
	bad=p;bad.instructions[0].target=RAL_MATERIAL_PROXY_ROUGHNESS;
	CHECK(!Ral_MaterialProxyProgramBuild(&material,&bad,&program));
	bad=p;bad.instructions[0].opcode=RAL_MATERIAL_PROXY_SET;bad.instructions[0].input=RAL_MATERIAL_PROXY_INPUT_NONE;
	bad.instructions[0].aQ16=INT32_MAX;bad.instructions[1].target=RAL_MATERIAL_PROXY_EMISSIVE_R;
	bad.instructions[1].opcode=RAL_MATERIAL_PROXY_ADD;bad.instructions[1].input=RAL_MATERIAL_PROXY_INPUT_NONE;
	bad.instructions[1].aQ16=1;bad.instructionCount=2u;
	CHECK(Ral_MaterialProxyProgramBuild(&material,&bad,&program));
	CHECK(!Ral_MaterialProxyEvaluate(&material,&program,&inputs,RAL_MATERIAL_PROXY_DOMAIN_RUNTIME,
		RAL_MATERIAL_PROXY_VALUE_COUNT,&runtime));

	/* Schema-v2 is one bounded four-lane VM with the complete 21-op set. */
	p=ExpressionProgram(&material);CHECK(Ral_MaterialProxyProgramBuild(&material,&p,&expressionProgram));
	CHECK(Ral_MaterialProxyProgramReceiptValid(&expressionProgram));
	CHECK(Ral_MaterialProxyEvaluate(&material,&expressionProgram,&inputs,
		RAL_MATERIAL_PROXY_DOMAIN_RUNTIME,RAL_MATERIAL_PROXY_VALUE_COUNT,&expression));
	CHECK(expression.valuesQ16[RAL_MATERIAL_PROXY_BASE_COLOR_R]==4*RAL_MATERIAL_PROXY_Q16_ONE);
	CHECK(expression.valuesQ16[RAL_MATERIAL_PROXY_BASE_COLOR_G]==3*RAL_MATERIAL_PROXY_Q16_ONE);
	CHECK(expression.valuesQ16[RAL_MATERIAL_PROXY_BASE_COLOR_B]==RAL_MATERIAL_PROXY_Q16_ONE/2);
	CHECK(expression.valuesQ16[RAL_MATERIAL_PROXY_EMISSIVE_R]==14*RAL_MATERIAL_PROXY_Q16_ONE);
	CHECK(expression.valuesQ16[RAL_MATERIAL_PROXY_ROUGHNESS]==RAL_MATERIAL_PROXY_Q16_ONE/2);
	beforeProgram=expressionProgram;bad=p;strcpy(bad.expressionFields[2].name,"emissive");
	CHECK(!Ral_MaterialProxyProgramBuild(&material,&bad,&expressionProgram)
		&&!memcmp(&expressionProgram,&beforeProgram,sizeof(expressionProgram)));
	bad=p;bad.expressionInstructions[10].b=33u;CHECK(Ral_MaterialProxyProgramBuild(&material,&bad,&expressionProgram));
	before=expression;CHECK(!Ral_MaterialProxyEvaluate(&material,&expressionProgram,&inputs,
		RAL_MATERIAL_PROXY_DOMAIN_RUNTIME,RAL_MATERIAL_PROXY_VALUE_COUNT,&expression)
		&&!memcmp(&expression,&before,sizeof(expression)));

	/* Explicit v1 receipts still build and evaluate with byte-for-byte values. */
	p=Program(&material);p.schemaVersion=RAL_MATERIAL_PROXY_LEGACY_SCHEMA_VERSION;
	CHECK(Ral_MaterialProxyProgramBuild(&material,&p,&legacyProgram));
	CHECK(Ral_MaterialProxyEvaluate(&material,&legacyProgram,&inputs,
		RAL_MATERIAL_PROXY_DOMAIN_RUNTIME,RAL_MATERIAL_PROXY_VALUE_COUNT,&legacy));
	CHECK(legacy.valuesQ16[RAL_MATERIAL_PROXY_EMISSIVE_R]
		==RAL_MATERIAL_PROXY_Q16_ONE+RAL_MATERIAL_PROXY_Q16_ONE/2);
	CHECK(legacy.valuesQ16[RAL_MATERIAL_PROXY_ROUGHNESS]
		==(RAL_MATERIAL_PROXY_Q16_ONE*5)/16);
	puts("ral_material_proxy_test: ok");return 0;
}
