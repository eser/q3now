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

int main(void){
	ralMaterialReceipt_t material,stale;ralMaterialProxyProgram_t p,bad;
	ralMaterialProxyProgramReceipt_t program,beforeProgram;
	ralMaterialProxyInputs_t inputs;ralMaterialProxyEvaluationReceipt_t runtime,preview,before;
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
	puts("ral_material_proxy_test: ok");return 0;
}
