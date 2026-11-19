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

static qboolean LegacyProgramValid( const ralMaterialProxyProgram_t *p ) {
	uint32_t i;
	qboolean hasWriter = qfalse;
	ralMaterialProxyTarget_t target = RAL_MATERIAL_PROXY_BASE_COLOR_R;
	if ( p->instructionCount > RAL_MATERIAL_PROXY_MAX_INSTRUCTIONS ) return qfalse;
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

static qboolean FieldNameValid( const char *name ) {
	size_t length;
	if ( !name || !name[0] ) return qfalse;
	length = strnlen( name, RAL_MATERIAL_PROXY_FIELD_NAME );
	return length > 0u && length < RAL_MATERIAL_PROXY_FIELD_NAME;
}

static qboolean ExpressionProgramValid( const ralMaterialProxyProgram_t *p ) {
	uint32_t i, j;
	if ( p->schemaVersion != RAL_MATERIAL_PROXY_SCHEMA_VERSION
			|| p->instructionCount != 0u
			|| p->expressionRegisterCount
				< RAL_MATERIAL_PROXY_REGISTER_BASE_VALUES
					+ RAL_MATERIAL_PROXY_BASE_VALUE_COUNT
			|| p->expressionRegisterCount
				> RAL_MATERIAL_PROXY_EXPRESSION_MAX_REGISTERS
			|| p->expressionConstantCount
				> RAL_MATERIAL_PROXY_EXPRESSION_MAX_CONSTANTS
			|| !p->expressionInstructionCount
			|| p->expressionInstructionCount
				> RAL_MATERIAL_PROXY_EXPRESSION_MAX_INSTRUCTIONS
			|| !p->expressionFieldCount
			|| p->expressionFieldCount
				> RAL_MATERIAL_PROXY_EXPRESSION_MAX_FIELDS
			|| p->expressionTableCount
				> RAL_MATERIAL_PROXY_EXPRESSION_MAX_TABLES ) return qfalse;
	for ( i = 0u; i < p->expressionTableCount; ++i ) {
		if ( p->expressionTables[i].sampleCount < 2u
				|| p->expressionTables[i].sampleCount
					> RAL_MATERIAL_PROXY_EXPRESSION_TABLE_SAMPLES
				|| ( p->expressionTables[i].flags
					& ~( RAL_MATERIAL_EXPRESSION_TABLE_WRAP
						| RAL_MATERIAL_EXPRESSION_TABLE_SNAP ) ) ) return qfalse;
	}
	for ( i = 0u; i < p->expressionInstructionCount; ++i ) {
		const ralMaterialExpressionInstruction_t *op =
			&p->expressionInstructions[i];
		if ( op->opcode >= RAL_MATERIAL_EXPRESSION_OPCODE_COUNT
				|| op->destination >= p->expressionRegisterCount ) return qfalse;
		switch ( op->opcode ) {
		case RAL_MATERIAL_EXPRESSION_UNMASKED_MOVE_CONSTANT:
			if ( op->a >= p->expressionConstantCount || op->b ) return qfalse;
			break;
		case RAL_MATERIAL_EXPRESSION_UNMASKED_MOVE:
			if ( op->a >= p->expressionRegisterCount || op->b ) return qfalse;
			break;
		case RAL_MATERIAL_EXPRESSION_MOVE_CONSTANT:
			if ( op->a >= p->expressionConstantCount || !( op->b & 15u )
					|| ( op->b & ~15u ) ) return qfalse;
			break;
		case RAL_MATERIAL_EXPRESSION_MOVE:
			if ( op->a >= p->expressionRegisterCount || !( op->b & 15u )
					|| ( op->b & ~15u ) ) return qfalse;
			break;
		case RAL_MATERIAL_EXPRESSION_SWIZZLE:
			if ( op->a >= p->expressionRegisterCount || ( op->b & ~255u ) )
				return qfalse;
			break;
		case RAL_MATERIAL_EXPRESSION_TABLE:
			if ( op->a >= p->expressionRegisterCount
					|| op->b >= p->expressionTableCount ) return qfalse;
			break;
		default:
			if ( op->a >= p->expressionRegisterCount
					|| op->b >= p->expressionRegisterCount ) return qfalse;
			break;
		}
	}
	for ( i = 0u; i < p->expressionFieldCount; ++i ) {
		const ralMaterialExpressionField_t *field = &p->expressionFields[i];
		uint32_t inlineEnd, outputEnd;
		if ( !FieldNameValid( field->name ) || field->reserved
				|| field->registerIndex >= p->expressionRegisterCount
				|| !field->componentCount || field->componentCount > 4u
				|| ( field->inlineByteOffset & 3u ) ) return qfalse;
		inlineEnd = field->inlineByteOffset + field->componentCount * 4u;
		outputEnd = field->outputValueOffset + field->componentCount;
		if ( inlineEnd > RAL_MATERIAL_PROXY_INLINE_MAX_BYTES
				|| outputEnd > RAL_MATERIAL_PROXY_VALUE_COUNT )
			return qfalse;
		for ( j = 0u; j < i; ++j ) {
			const ralMaterialExpressionField_t *previous =
				&p->expressionFields[j];
			uint32_t previousInlineEnd = previous->inlineByteOffset
				+ previous->componentCount * 4u;
			uint32_t previousOutputEnd = previous->outputValueOffset
				+ previous->componentCount;
			if ( !strcmp( field->name, previous->name )
					|| ( field->inlineByteOffset < previousInlineEnd
						&& previous->inlineByteOffset < inlineEnd )
					|| ( field->outputValueOffset < previousOutputEnd
						&& previous->outputValueOffset < outputEnd ) ) return qfalse;
		}
	}
	return qtrue;
}

static qboolean ProgramValid( const ralMaterialProxyProgram_t *p ) {
	if ( !p || !p->materialArtifactHash
			|| ( p->schemaVersion != RAL_MATERIAL_PROXY_LEGACY_SCHEMA_VERSION
				&& p->schemaVersion != RAL_MATERIAL_PROXY_SCHEMA_VERSION ) )
		return qfalse;
	if ( p->expressionInstructionCount || p->expressionFieldCount
			|| p->expressionRegisterCount || p->expressionConstantCount
			|| p->expressionTableCount ) return ExpressionProgramValid( p );
	return LegacyProgramValid( p );
}

static uint64_t ProgramHash( const ralMaterialProxyProgram_t *p ) {
	uint64_t h=FNV64_OFFSET;uint32_t i;
	h=HashU32(h,RAL_MATERIAL_PROXY_RECEIPT_SCHEMA_VERSION);
	h=HashU32(h,p->schemaVersion);
	h=HashU64(h,p->materialArtifactHash);h=HashU32(h,p->instructionCount);
	for(i=0u;i<p->instructionCount;i++){
		const ralMaterialProxyInstruction_t*v=&p->instructions[i];
		h=HashU32(h,(uint32_t)v->target);h=HashU32(h,(uint32_t)v->opcode);
		h=HashU32(h,(uint32_t)v->input);h=HashU32(h,(uint32_t)v->aQ16);
		h=HashU32(h,(uint32_t)v->bQ16);h=HashU32(h,(uint32_t)v->cQ16);
	}
	h=HashU32(h,p->expressionRegisterCount);
	h=HashU32(h,p->expressionConstantCount);
	for(i=0u;i<p->expressionConstantCount;i++){
		uint32_t lane;for(lane=0u;lane<4u;lane++)h=HashU32(h,
			(uint32_t)p->expressionConstants[i].lane[lane]);
	}
	h=HashU32(h,p->expressionInstructionCount);
	for(i=0u;i<p->expressionInstructionCount;i++){
		const ralMaterialExpressionInstruction_t *op=&p->expressionInstructions[i];
		h=HashU32(h,op->opcode);h=HashU32(h,op->destination);
		h=HashU32(h,op->a);h=HashU32(h,op->b);
	}
	h=HashU32(h,p->expressionFieldCount);
	for(i=0u;i<p->expressionFieldCount;i++){
		const ralMaterialExpressionField_t *field=&p->expressionFields[i];
		uint32_t character;
		for(character=0u;character<RAL_MATERIAL_PROXY_FIELD_NAME;character++)
			h=HashByte(h,(unsigned char)field->name[character]);
		h=HashU32(h,field->registerIndex);h=HashU32(h,field->inlineByteOffset);
		h=HashU32(h,field->componentCount);h=HashU32(h,field->outputValueOffset);
	}
	h=HashU32(h,p->expressionTableCount);
	for(i=0u;i<p->expressionTableCount;i++){
		uint32_t sample;h=HashU32(h,p->expressionTables[i].sampleCount);
		h=HashU32(h,p->expressionTables[i].flags);
		for(sample=0u;sample<p->expressionTables[i].sampleCount;sample++)
			h=HashU32(h,(uint32_t)p->expressionTables[i].samplesQ16[sample]);
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

static qboolean Sub( int32_t a, int32_t b, int32_t *out ) {
	int64_t value = (int64_t)a - b;
	if ( value < INT32_MIN || value > INT32_MAX ) return qfalse;
	*out = (int32_t)value; return qtrue;
}

static qboolean Div( int32_t a, int32_t b, int32_t *out ) {
	int64_t value;
	if ( !b ) return qfalse;
	value = ( (int64_t)a * RAL_MATERIAL_PROXY_Q16_ONE ) / b;
	if ( value < INT32_MIN || value > INT32_MAX ) return qfalse;
	*out = (int32_t)value; return qtrue;
}

static qboolean Mod( int32_t a, int32_t b, int32_t *out ) {
	if ( !b || ( a == INT32_MIN && b == -1 ) ) return qfalse;
	*out = a % b; return qtrue;
}

static qboolean ExpressionBinaryLane( uint16_t opcode, int32_t a, int32_t b,
		int32_t *out ) {
	switch ( opcode ) {
	case RAL_MATERIAL_EXPRESSION_ADD: return Add( a, b, out );
	case RAL_MATERIAL_EXPRESSION_SUBTRACT: return Sub( a, b, out );
	case RAL_MATERIAL_EXPRESSION_MULTIPLY: return Mul( a, b, out );
	case RAL_MATERIAL_EXPRESSION_DIVIDE: return Div( a, b, out );
	case RAL_MATERIAL_EXPRESSION_MOD: return Mod( a, b, out );
	case RAL_MATERIAL_EXPRESSION_GT: *out = a > b ? RAL_MATERIAL_PROXY_Q16_ONE : 0; return qtrue;
	case RAL_MATERIAL_EXPRESSION_GE: *out = a >= b ? RAL_MATERIAL_PROXY_Q16_ONE : 0; return qtrue;
	case RAL_MATERIAL_EXPRESSION_LT: *out = a < b ? RAL_MATERIAL_PROXY_Q16_ONE : 0; return qtrue;
	case RAL_MATERIAL_EXPRESSION_LE: *out = a <= b ? RAL_MATERIAL_PROXY_Q16_ONE : 0; return qtrue;
	case RAL_MATERIAL_EXPRESSION_EQ: *out = a == b ? RAL_MATERIAL_PROXY_Q16_ONE : 0; return qtrue;
	case RAL_MATERIAL_EXPRESSION_NE: *out = a != b ? RAL_MATERIAL_PROXY_Q16_ONE : 0; return qtrue;
	case RAL_MATERIAL_EXPRESSION_AND: *out = a && b ? RAL_MATERIAL_PROXY_Q16_ONE : 0; return qtrue;
	case RAL_MATERIAL_EXPRESSION_OR: *out = a || b ? RAL_MATERIAL_PROXY_Q16_ONE : 0; return qtrue;
	default: return qfalse;
	}
}

static qboolean ExpressionTableLane( const ralMaterialExpressionTable_t *table,
		int32_t coordinate, int32_t *out ) {
	uint32_t interval, fraction, intervalCount;
	int64_t scaled;
	if ( table->flags & RAL_MATERIAL_EXPRESSION_TABLE_WRAP ) {
		coordinate %= RAL_MATERIAL_PROXY_Q16_ONE;
		if ( coordinate < 0 ) coordinate += RAL_MATERIAL_PROXY_Q16_ONE;
	} else {
		if ( coordinate <= 0 ) { *out = table->samplesQ16[0]; return qtrue; }
		if ( coordinate >= RAL_MATERIAL_PROXY_Q16_ONE ) {
			*out = table->samplesQ16[table->sampleCount - 1u]; return qtrue;
		}
	}
	intervalCount = ( table->flags & RAL_MATERIAL_EXPRESSION_TABLE_WRAP )
		? table->sampleCount : table->sampleCount - 1u;
	scaled = (int64_t)coordinate * intervalCount;
	interval = (uint32_t)( scaled / RAL_MATERIAL_PROXY_Q16_ONE );
	if ( table->flags & RAL_MATERIAL_EXPRESSION_TABLE_SNAP ) {
		if ( interval >= table->sampleCount ) interval = table->sampleCount - 1u;
		*out = table->samplesQ16[interval]; return qtrue;
	}
	fraction = (uint32_t)( scaled % RAL_MATERIAL_PROXY_Q16_ONE );
	return Lerp( table->samplesQ16[interval],
		table->samplesQ16[( interval + 1u ) % table->sampleCount],
		(int32_t)fraction, out );
}

static qboolean EvaluateExpression( const ralMaterialReceipt_t *material,
		const ralMaterialProxyProgram_t *program,
		const ralMaterialProxyInputs_t *inputs, int32_t *outValues ) {
	ralMaterialExpressionValue_t registers[
		RAL_MATERIAL_PROXY_EXPRESSION_MAX_REGISTERS];
	int32_t baseValues[RAL_MATERIAL_PROXY_BASE_VALUE_COUNT];
	uint32_t i, lane;
	uint64_t timeQ16;
	memset( registers, 0, sizeof( registers ) );
	if ( !BaseValues( material, baseValues )
			|| inputs->timeMilliseconds > UINT64_MAX / RAL_MATERIAL_PROXY_Q16_ONE )
		return qfalse;
	timeQ16 = inputs->timeMilliseconds * RAL_MATERIAL_PROXY_Q16_ONE / 1000u;
	if ( timeQ16 > INT32_MAX ) return qfalse;
	for ( lane = 0u; lane < 4u; ++lane ) {
		registers[RAL_MATERIAL_PROXY_REGISTER_TIME].lane[lane] = (int32_t)timeQ16;
		registers[RAL_MATERIAL_PROXY_REGISTER_SCALAR].lane[lane] = inputs->scalarQ16;
	}
	for ( i = 0u; i < RAL_MATERIAL_PROXY_BASE_VALUE_COUNT; ++i )
		for ( lane = 0u; lane < 4u; ++lane )
			registers[RAL_MATERIAL_PROXY_REGISTER_BASE_VALUES + i].lane[lane]
				= baseValues[i];
	for ( i = 0u; i < program->expressionInstructionCount; ++i ) {
		const ralMaterialExpressionInstruction_t *op =
			&program->expressionInstructions[i];
		ralMaterialExpressionValue_t result = registers[op->destination];
		const ralMaterialExpressionValue_t *a;
		const ralMaterialExpressionValue_t *b;
		switch ( op->opcode ) {
		case RAL_MATERIAL_EXPRESSION_UNMASKED_MOVE_CONSTANT:
			result = program->expressionConstants[op->a]; break;
		case RAL_MATERIAL_EXPRESSION_UNMASKED_MOVE:
			result = registers[op->a]; break;
		case RAL_MATERIAL_EXPRESSION_MOVE_CONSTANT:
			for ( lane = 0u; lane < 4u; ++lane ) if ( op->b & ( 1u << lane ) )
				result.lane[lane] = program->expressionConstants[op->a].lane[lane];
			break;
		case RAL_MATERIAL_EXPRESSION_MOVE:
			for ( lane = 0u; lane < 4u; ++lane ) if ( op->b & ( 1u << lane ) )
				result.lane[lane] = registers[op->a].lane[lane];
			break;
		case RAL_MATERIAL_EXPRESSION_SWIZZLE:
			a = &registers[op->a];
			for ( lane = 0u; lane < 4u; ++lane )
				result.lane[lane] = a->lane[( op->b >> ( lane * 2u ) ) & 3u];
			break;
		case RAL_MATERIAL_EXPRESSION_DOT3:
		case RAL_MATERIAL_EXPRESSION_DOT4: {
			int64_t sum = 0;
			uint32_t componentCount = op->opcode == RAL_MATERIAL_EXPRESSION_DOT3
				? 3u : 4u;
			a = &registers[op->a]; b = &registers[op->b];
			for ( lane = 0u; lane < componentCount; ++lane )
				sum += ( (int64_t)a->lane[lane] * b->lane[lane] )
					/ RAL_MATERIAL_PROXY_Q16_ONE;
			if ( sum < INT32_MIN || sum > INT32_MAX ) return qfalse;
			for ( lane = 0u; lane < 4u; ++lane ) result.lane[lane] = (int32_t)sum;
			break;
		}
		case RAL_MATERIAL_EXPRESSION_TABLE:
			a = &registers[op->a];
			for ( lane = 0u; lane < 4u; ++lane ) if ( !ExpressionTableLane(
					&program->expressionTables[op->b], a->lane[lane],
					&result.lane[lane] ) ) return qfalse;
			break;
		default:
			a = &registers[op->a]; b = &registers[op->b];
			for ( lane = 0u; lane < 4u; ++lane ) if ( !ExpressionBinaryLane(
					op->opcode, a->lane[lane], b->lane[lane],
					&result.lane[lane] ) ) return qfalse;
			break;
		}
		registers[op->destination] = result;
	}
	memset( outValues, 0,
		RAL_MATERIAL_PROXY_VALUE_COUNT * sizeof( outValues[0] ) );
	memcpy( outValues, baseValues, sizeof( baseValues ) );
	for ( i = 0u; i < program->expressionFieldCount; ++i ) {
		const ralMaterialExpressionField_t *field = &program->expressionFields[i];
		for ( lane = 0u; lane < field->componentCount; ++lane )
			outValues[field->outputValueOffset + lane]
				= registers[field->registerIndex].lane[lane];
	}
	return qtrue;
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
	memset(&v,0,sizeof(v));
	if(program->program.expressionInstructionCount){
		if(!EvaluateExpression(material,&program->program,inputs,v.valuesQ16))return qfalse;
	}else{
	if(!BaseValues(material,v.valuesQ16))return qfalse;
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
