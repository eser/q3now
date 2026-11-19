// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_proxy_lower.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define Q3_FUNCTION_TABLE_SIZE 1024u
#define Q3_NOISE_SIZE 256u
#define Q3_NOISE_MASK ( Q3_NOISE_SIZE - 1u )
#define Q3_NOISE_SEED UINT32_C(0x6D6F6953)
#define Q30_ONE INT64_C(1073741824)
#define Q30_HALF INT64_C(536870912)
#define Q30_SIN_STEP INT64_C(6588355)
#define Q30_COS_STEP INT64_C(1073721611)
#define Q30_SIN_BIAS INT64_C(107374)
#define Q30_COS_BIAS INT64_C(1073741818)

typedef struct {
	ralMaterialProxyProgram_t program;
	ralMaterialProxyLowerDiagnostic_t *diagnostic;
	uint32_t nextRegister;
	uint32_t nextOutput;
	uint32_t tableByFunction[RAL_MATERIAL_WAVE_NOISE + 1u];
} proxyBuilder_t;

static void Fail( proxyBuilder_t *builder,
		ralMaterialProxyLowerFailure_t reason, uint32_t stageIndex,
		uint32_t operationIndex, ralMaterialSourceSpan_t span ) {
	if ( !builder || !builder->diagnostic ) return;
	memset( builder->diagnostic, 0, sizeof( *builder->diagnostic ) );
	builder->diagnostic->reason = reason;
	builder->diagnostic->stageIndex = stageIndex;
	builder->diagnostic->operationIndex = operationIndex;
	builder->diagnostic->span = span;
}

static qboolean FloatQ16( float value, int32_t *out ) {
	double scaled;
	if ( !out || !isfinite( value ) ) return qfalse;
	scaled = (double)value * RAL_MATERIAL_PROXY_Q16_ONE;
	if ( scaled < INT32_MIN || scaled > INT32_MAX ) return qfalse;
	*out = (int32_t)( scaled < 0.0 ? scaled - 0.5 : scaled + 0.5 );
	return qtrue;
}

static int64_t Q30MulAdd( int64_t a, int64_t b, int64_t c, int64_t d ) {
	int64_t value = a * b + c * d;
	return value < 0 ? ( value - Q30_HALF ) / Q30_ONE
		: ( value + Q30_HALF ) / Q30_ONE;
}

static int32_t Q30ToQ16( int64_t value ) {
	const int64_t divisor = INT64_C(16384);
	return (int32_t)( value < 0 ? ( value - divisor / 2 ) / divisor
		: ( value + divisor / 2 ) / divisor );
}

static qboolean Constant( proxyBuilder_t *builder, const float lane[4],
		uint16_t *outIndex ) {
	ralMaterialExpressionValue_t value;
	uint32_t i;
	for ( i = 0u; i < 4u; ++i )
		if ( !FloatQ16( lane[i], &value.lane[i] ) ) return qfalse;
	for ( i = 0u; i < builder->program.expressionConstantCount; ++i ) {
		if ( !memcmp( &builder->program.expressionConstants[i], &value,
				sizeof( value ) ) ) {
			*outIndex = (uint16_t)i; return qtrue;
		}
	}
	if ( builder->program.expressionConstantCount
			>= RAL_MATERIAL_PROXY_EXPRESSION_MAX_CONSTANTS ) return qfalse;
	i = builder->program.expressionConstantCount++;
	builder->program.expressionConstants[i] = value;
	*outIndex = (uint16_t)i;
	return qtrue;
}

static qboolean Emit( proxyBuilder_t *builder,
		ralMaterialExpressionOpcode_t opcode, uint16_t a, uint16_t b,
		uint16_t *outRegister ) {
	ralMaterialExpressionInstruction_t *instruction;
	if ( builder->program.expressionInstructionCount
			>= RAL_MATERIAL_PROXY_EXPRESSION_MAX_INSTRUCTIONS
			|| builder->nextRegister
				>= RAL_MATERIAL_PROXY_EXPRESSION_MAX_REGISTERS ) return qfalse;
	instruction = &builder->program.expressionInstructions[
		builder->program.expressionInstructionCount++];
	instruction->opcode = (uint16_t)opcode;
	instruction->destination = (uint16_t)builder->nextRegister++;
	instruction->a = a;
	instruction->b = b;
	*outRegister = instruction->destination;
	return qtrue;
}

static qboolean ConstantRegister( proxyBuilder_t *builder,
		const float lane[4], uint16_t *outRegister ) {
	uint16_t constant;
	return Constant( builder, lane, &constant )
		&& Emit( builder, RAL_MATERIAL_EXPRESSION_UNMASKED_MOVE_CONSTANT,
			constant, 0u, outRegister );
}

static qboolean Field( proxyBuilder_t *builder, const char *name,
		uint16_t registerIndex, uint32_t componentCount ) {
	ralMaterialExpressionField_t *field;
	uint32_t byteOffset;
	if ( !name || !name[0] || strlen( name ) >= RAL_MATERIAL_PROXY_FIELD_NAME
			|| !componentCount || componentCount > 4u
			|| builder->program.expressionFieldCount
				>= RAL_MATERIAL_PROXY_EXPRESSION_MAX_FIELDS
			|| builder->nextOutput + componentCount
				> RAL_MATERIAL_PROXY_VALUE_COUNT ) return qfalse;
	byteOffset = builder->nextOutput * 4u;
	if ( byteOffset + componentCount * 4u
			> RAL_MATERIAL_PROXY_INLINE_MAX_BYTES ) return qfalse;
	field = &builder->program.expressionFields[
		builder->program.expressionFieldCount++];
	(void)snprintf( field->name, sizeof( field->name ), "%s", name );
	field->registerIndex = registerIndex;
	field->inlineByteOffset = (uint16_t)byteOffset;
	field->componentCount = (uint8_t)componentCount;
	field->outputValueOffset = (uint8_t)builder->nextOutput;
	builder->nextOutput += componentCount;
	return qtrue;
}

static uint32_t NoiseNext( uint32_t *state ) {
	uint32_t value = *state;
	value ^= value << 13;
	value ^= value >> 17;
	value ^= value << 5;
	*state = value;
	return value;
}

static uint32_t NoiseIndex( const uint8_t *permutation, uint32_t t ) {
	uint32_t index = permutation[t & Q3_NOISE_MASK];
	index = permutation[index & Q3_NOISE_MASK];
	index = permutation[index & Q3_NOISE_MASK];
	return permutation[index & Q3_NOISE_MASK];
}

static qboolean BuildWaveTable( proxyBuilder_t *builder,
		ralMaterialWaveFunction_t function, uint16_t *outTable ) {
	ralMaterialExpressionTable_t *table;
	uint32_t tableIndex, sample;
	if ( function <= RAL_MATERIAL_WAVE_NONE
			|| function > RAL_MATERIAL_WAVE_NOISE ) return qfalse;
	if ( builder->tableByFunction[function] != UINT32_MAX ) {
		*outTable = (uint16_t)builder->tableByFunction[function]; return qtrue;
	}
	if ( builder->program.expressionTableCount
			>= RAL_MATERIAL_PROXY_EXPRESSION_MAX_TABLES ) return qfalse;
	tableIndex = builder->program.expressionTableCount++;
	table = &builder->program.expressionTables[tableIndex];
	if ( function == RAL_MATERIAL_WAVE_NOISE ) {
		float values[Q3_NOISE_SIZE];
		uint8_t permutation[Q3_NOISE_SIZE];
		uint32_t state = Q3_NOISE_SEED;
		table->sampleCount = Q3_NOISE_SIZE;
		table->flags = RAL_MATERIAL_EXPRESSION_TABLE_WRAP;
		for ( sample = 0u; sample < Q3_NOISE_SIZE; ++sample ) {
			values[sample] = ( (float)( NoiseNext( &state ) & 0xFFFFFFu )
				/ (float)0xFFFFFFu ) * 2.0f - 1.0f;
			permutation[sample] = (uint8_t)( NoiseNext( &state ) & 0xFFu );
		}
		for ( sample = 0u; sample < Q3_NOISE_SIZE; ++sample ) {
			if ( !FloatQ16( values[NoiseIndex( permutation, sample )],
					&table->samplesQ16[sample] ) ) return qfalse;
		}
	} else {
		int64_t sineQ30 = Q30_SIN_BIAS;
		int64_t cosineQ30 = Q30_COS_BIAS;
		table->sampleCount = Q3_FUNCTION_TABLE_SIZE;
		table->flags = RAL_MATERIAL_EXPRESSION_TABLE_WRAP
			| RAL_MATERIAL_EXPRESSION_TABLE_SNAP;
		for ( sample = 0u; sample < Q3_FUNCTION_TABLE_SIZE; ++sample ) {
			float value;
			switch ( function ) {
			case RAL_MATERIAL_WAVE_SIN:
				table->samplesQ16[sample] = Q30ToQ16( sineQ30 );
				{
					const int64_t nextSine = Q30MulAdd( sineQ30,
						Q30_COS_STEP, cosineQ30, Q30_SIN_STEP );
					const int64_t nextCosine = Q30MulAdd( cosineQ30,
						Q30_COS_STEP, -sineQ30, Q30_SIN_STEP );
					sineQ30 = nextSine; cosineQ30 = nextCosine;
				}
				continue;
			case RAL_MATERIAL_WAVE_SQUARE:
				value = sample < Q3_FUNCTION_TABLE_SIZE / 2u ? 1.0f : -1.0f;
				break;
			case RAL_MATERIAL_WAVE_TRIANGLE:
				if ( sample < Q3_FUNCTION_TABLE_SIZE / 4u )
					value = sample ? (float)sample
						/ ( Q3_FUNCTION_TABLE_SIZE / 4u ) : 0.000001f;
				else if ( sample < Q3_FUNCTION_TABLE_SIZE / 2u )
					value = 1.0f - (float)( sample
						- Q3_FUNCTION_TABLE_SIZE / 4u )
						/ ( Q3_FUNCTION_TABLE_SIZE / 4u );
				else if ( sample < 3u * Q3_FUNCTION_TABLE_SIZE / 4u )
					value = -(float)( sample - Q3_FUNCTION_TABLE_SIZE / 2u )
						/ ( Q3_FUNCTION_TABLE_SIZE / 4u );
				else value = -1.0f + (float)( sample
						- 3u * Q3_FUNCTION_TABLE_SIZE / 4u )
						/ ( Q3_FUNCTION_TABLE_SIZE / 4u );
				break;
			case RAL_MATERIAL_WAVE_SAWTOOTH:
				value = sample ? (float)sample / Q3_FUNCTION_TABLE_SIZE
					: 0.000001f; break;
			case RAL_MATERIAL_WAVE_INVERSE_SAWTOOTH:
				value = 1.0f - ( sample ? (float)sample
					/ Q3_FUNCTION_TABLE_SIZE : 0.000001f ); break;
			default: return qfalse;
			}
			if ( !FloatQ16( value, &table->samplesQ16[sample] ) ) return qfalse;
		}
	}
	builder->tableByFunction[function] = tableIndex;
	*outTable = (uint16_t)tableIndex;
	return qtrue;
}

static qboolean Wave( proxyBuilder_t *builder,
		ralMaterialWaveFunction_t function, float base, float amplitude,
		float phase, float frequency, uint16_t *outRegister ) {
	float frequencyLane[4] = { frequency, frequency, frequency, frequency };
	float phaseLane[4] = { phase, phase, phase, phase };
	float amplitudeLane[4] = { amplitude, amplitude, amplitude, amplitude };
	float baseLane[4] = { base, base, base, base };
	uint16_t frequencyRegister, phaseRegister, amplitudeRegister, baseRegister;
	uint16_t table, scaledTime, coordinate, sampled, scaled;
	if ( function == RAL_MATERIAL_WAVE_NOISE ) {
		phaseLane[0] = phaseLane[1] = phaseLane[2] = phaseLane[3]
			= phase * frequency;
	}
	if ( !ConstantRegister( builder, frequencyLane, &frequencyRegister )
			|| !ConstantRegister( builder, phaseLane, &phaseRegister )
			|| !ConstantRegister( builder, amplitudeLane, &amplitudeRegister )
			|| !ConstantRegister( builder, baseLane, &baseRegister )
			|| !BuildWaveTable( builder, function, &table )
			|| !Emit( builder, RAL_MATERIAL_EXPRESSION_MULTIPLY,
				RAL_MATERIAL_PROXY_REGISTER_TIME, frequencyRegister, &scaledTime )
			|| !Emit( builder, RAL_MATERIAL_EXPRESSION_ADD, scaledTime,
				phaseRegister, &coordinate ) ) return qfalse;
	if ( function == RAL_MATERIAL_WAVE_NOISE ) {
		float divisorLane[4] = { 256.0f, 256.0f, 256.0f, 256.0f };
		uint16_t divisor, oneCycle;
		if ( !ConstantRegister( builder, divisorLane, &divisor )
				|| !Emit( builder, RAL_MATERIAL_EXPRESSION_DIVIDE,
					coordinate, divisor, &oneCycle ) ) return qfalse;
		coordinate = oneCycle;
	}
	return Emit( builder, RAL_MATERIAL_EXPRESSION_TABLE, coordinate, table,
			&sampled )
		&& Emit( builder, RAL_MATERIAL_EXPRESSION_MULTIPLY, sampled,
			amplitudeRegister, &scaled )
		&& Emit( builder, RAL_MATERIAL_EXPRESSION_ADD, scaled, baseRegister,
			outRegister );
}

static qboolean StaticField( proxyBuilder_t *builder, const char *name,
		const float values[4], uint32_t componentCount ) {
	uint16_t valueRegister;
	return ConstantRegister( builder, values, &valueRegister )
		&& Field( builder, name, valueRegister, componentCount );
}

static qboolean ScrollField( proxyBuilder_t *builder, const char *name,
		const float speeds[2] ) {
	float speedLane[4] = { speeds[0], speeds[1], 0.0f, 0.0f };
	float oneLane[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float zeroLane[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	uint16_t speed, one, zero, raw, remainder, negative, correction, wrapped;
	return ConstantRegister( builder, speedLane, &speed )
		&& ConstantRegister( builder, oneLane, &one )
		&& ConstantRegister( builder, zeroLane, &zero )
		&& Emit( builder, RAL_MATERIAL_EXPRESSION_MULTIPLY,
			RAL_MATERIAL_PROXY_REGISTER_TIME, speed, &raw )
		&& Emit( builder, RAL_MATERIAL_EXPRESSION_MOD, raw, one, &remainder )
		&& Emit( builder, RAL_MATERIAL_EXPRESSION_LT, remainder, zero, &negative )
		&& Emit( builder, RAL_MATERIAL_EXPRESSION_MULTIPLY,
			negative, one, &correction )
		&& Emit( builder, RAL_MATERIAL_EXPRESSION_ADD,
			remainder, correction, &wrapped )
		&& Field( builder, name, wrapped, 2u );
}

static qboolean RotateField( proxyBuilder_t *builder, const char *name,
		float degreesPerSecond ) {
	float speedLane[4] = { -degreesPerSecond / 360.0f,
		-degreesPerSecond / 360.0f, 0.0f, 0.0f };
	float phaseLane[4] = { 0.0f, 0.25f, 0.0f, 0.0f };
	uint16_t speed, phase, coordinate, phased, table, sineCosine;
	return ConstantRegister( builder, speedLane, &speed )
		&& ConstantRegister( builder, phaseLane, &phase )
		&& BuildWaveTable( builder, RAL_MATERIAL_WAVE_SIN, &table )
		&& Emit( builder, RAL_MATERIAL_EXPRESSION_MULTIPLY,
			RAL_MATERIAL_PROXY_REGISTER_TIME, speed, &coordinate )
		&& Emit( builder, RAL_MATERIAL_EXPRESSION_ADD,
			coordinate, phase, &phased )
		&& Emit( builder, RAL_MATERIAL_EXPRESSION_TABLE,
			phased, table, &sineCosine )
		&& Field( builder, name, sineCosine, 2u );
}

static qboolean TurbulenceField( proxyBuilder_t *builder, const char *name,
		const float parameters[6] ) {
	float rateLane[4] = { parameters[3], 0.0f, 0.0f, 0.0f };
	float valueLane[4] = { parameters[2], parameters[1], parameters[0], 0.0f };
	uint16_t rate, value, time, output;
	return ConstantRegister( builder, rateLane, &rate )
		&& ConstantRegister( builder, valueLane, &value )
		&& Emit( builder, RAL_MATERIAL_EXPRESSION_MULTIPLY,
			RAL_MATERIAL_PROXY_REGISTER_TIME, rate, &time )
		&& Emit( builder, RAL_MATERIAL_EXPRESSION_ADD, time, value, &output )
		&& Field( builder, name, output, 3u );
}

static qboolean LowerStage( proxyBuilder_t *builder,
		const ralMaterialSourceStage_t *stage, uint32_t stageIndex ) {
	char name[RAL_MATERIAL_PROXY_FIELD_NAME];
	uint16_t value;
	if ( stage->rgbGen == RAL_MATERIAL_COLOR_WAVE ) {
		if ( !Wave( builder, stage->rgbWave.function, stage->rgbWave.base,
				stage->rgbWave.amplitude, stage->rgbWave.phase,
				stage->rgbWave.frequency, &value ) ) return qfalse;
		(void)snprintf( name, sizeof( name ), "stage%u.rgbWave", stageIndex );
		if ( !Field( builder, name, value, 1u ) ) return qfalse;
	}
	if ( stage->alphaGen == RAL_MATERIAL_ALPHA_WAVE ) {
		if ( !Wave( builder, stage->alphaWave.function, stage->alphaWave.base,
				stage->alphaWave.amplitude, stage->alphaWave.phase,
				stage->alphaWave.frequency, &value ) ) return qfalse;
		(void)snprintf( name, sizeof( name ), "stage%u.alphaWave", stageIndex );
		if ( !Field( builder, name, value, 1u ) ) return qfalse;
	}
	for ( uint32_t i = 0u; i < stage->tcModCount; ++i ) {
		const ralMaterialSourceTcMod_t *tcMod = &stage->tcMods[i];
		float lanes[4] = { tcMod->parameters[0], tcMod->parameters[1],
			tcMod->parameters[2], tcMod->parameters[3] };
		(void)snprintf( name, sizeof( name ), "stage%u.tc%u", stageIndex, i );
		switch ( tcMod->type ) {
		case RAL_MATERIAL_TCMOD_SCALE:
			if ( !StaticField( builder, name, lanes, 2u ) ) return qfalse; break;
		case RAL_MATERIAL_TCMOD_SCROLL:
			if ( !ScrollField( builder, name, tcMod->parameters ) ) return qfalse; break;
		case RAL_MATERIAL_TCMOD_ROTATE:
			if ( !RotateField( builder, name, tcMod->parameters[0] ) ) return qfalse;
			break;
		case RAL_MATERIAL_TCMOD_TURBULENCE:
			if ( !TurbulenceField( builder, name, tcMod->parameters ) ) return qfalse;
			break;
		case RAL_MATERIAL_TCMOD_STRETCH: {
			float oneLane[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			uint16_t one, reciprocal;
			if ( !Wave( builder, (ralMaterialWaveFunction_t)(uint32_t)
					tcMod->parameters[0], tcMod->parameters[1],
					tcMod->parameters[2], tcMod->parameters[3],
					tcMod->parameters[4], &value )
					|| !ConstantRegister( builder, oneLane, &one )
					|| !Emit( builder, RAL_MATERIAL_EXPRESSION_DIVIDE,
						one, value, &reciprocal )
					|| !Field( builder, name, reciprocal, 1u ) ) return qfalse;
			break;
		}
		case RAL_MATERIAL_TCMOD_TRANSFORM:
			if ( !StaticField( builder, name, lanes, 4u ) ) return qfalse;
			(void)snprintf( name, sizeof( name ), "stage%u.tc%u.tail",
				stageIndex, i );
			lanes[0] = tcMod->parameters[4]; lanes[1] = tcMod->parameters[5];
			lanes[2] = lanes[3] = 0.0f;
			if ( !StaticField( builder, name, lanes, 2u ) ) return qfalse; break;
		default: return qfalse;
		}
	}
	return qtrue;
}

static qboolean LowerDeforms( proxyBuilder_t *builder,
		const ralMaterialSourceIr_t *source ) {
	for ( uint32_t i = 0u; i < source->deformCount; ++i ) {
		const ralMaterialSourceDeform_t *deform = &source->deforms[i];
		char name[RAL_MATERIAL_PROXY_FIELD_NAME];
		uint16_t value;
		if ( deform->type == RAL_MATERIAL_DEFORM_WAVE ) {
			if ( !Wave( builder, (ralMaterialWaveFunction_t)(uint32_t)
					deform->parameters[1], deform->parameters[2],
					deform->parameters[3], deform->parameters[4],
					deform->parameters[5], &value ) ) return qfalse;
		} else if ( deform->type == RAL_MATERIAL_DEFORM_MOVE ) {
			if ( !Wave( builder, (ralMaterialWaveFunction_t)(uint32_t)
					deform->parameters[3], deform->parameters[4],
					deform->parameters[5], deform->parameters[6],
					deform->parameters[7], &value ) ) return qfalse;
		} else continue;
		(void)snprintf( name, sizeof( name ), "deform%u.wave", i );
		if ( !Field( builder, name, value, 1u ) ) return qfalse;
	}
	return qtrue;
}

qboolean Ral_MaterialProxyLowerSource(
		const ralMaterialSourceReceipt_t *source,
		const ralMaterialReceipt_t *material, qboolean *outHasProgram,
		ralMaterialProxyProgramReceipt_t *outReceipt,
		ralMaterialProxyLowerDiagnostic_t *outDiagnostic ) {
	proxyBuilder_t builder;
	ralMaterialProxyProgramReceipt_t candidate;
	if ( outDiagnostic ) memset( outDiagnostic, 0, sizeof( *outDiagnostic ) );
	if ( !source || !outHasProgram || !outReceipt
			|| !Ral_MaterialSourceReceiptValid( source ) ) {
		if ( outDiagnostic ) outDiagnostic->reason =
			RAL_MATERIAL_PROXY_LOWER_INVALID_SOURCE;
		return qfalse;
	}
	if ( !material || !Ral_MaterialReceiptValid( material ) ) {
		if ( outDiagnostic ) outDiagnostic->reason =
			RAL_MATERIAL_PROXY_LOWER_INVALID_MATERIAL;
		return qfalse;
	}
	if ( source->source.hasAuthoredProxy ) {
		ralMaterialProxyProgram_t program = source->source.authoredProxy;
		program.materialArtifactHash = material->artifactHash;
		if ( !Ral_MaterialProxyProgramBuild( material, &program, &candidate ) ) {
			if ( outDiagnostic ) {
				outDiagnostic->reason = RAL_MATERIAL_PROXY_LOWER_PROGRAM_REJECTED;
				outDiagnostic->stageIndex = UINT32_MAX;
				outDiagnostic->span = source->source.declaration;
			}
			return qfalse;
		}
		*outHasProgram = qtrue;
		*outReceipt = candidate;
		return qtrue;
	}
	memset( &builder, 0, sizeof( builder ) );
	builder.diagnostic = outDiagnostic;
	builder.program.schemaVersion = RAL_MATERIAL_PROXY_SCHEMA_VERSION;
	builder.program.materialArtifactHash = material->artifactHash;
	builder.nextRegister = RAL_MATERIAL_PROXY_REGISTER_BASE_VALUES
		+ RAL_MATERIAL_PROXY_BASE_VALUE_COUNT;
	builder.nextOutput = RAL_MATERIAL_PROXY_BASE_VALUE_COUNT;
	for ( uint32_t i = 0u; i <= RAL_MATERIAL_WAVE_NOISE; ++i )
		builder.tableByFunction[i] = UINT32_MAX;
	for ( uint32_t i = 0u; i < source->source.stageCount; ++i ) {
		if ( !LowerStage( &builder, &source->source.stages[i], i ) ) {
			Fail( &builder, RAL_MATERIAL_PROXY_LOWER_OVER_CAPACITY, i, 0u,
				source->source.stages[i].span ); return qfalse;
		}
	}
	if ( !LowerDeforms( &builder, &source->source ) ) {
		Fail( &builder, RAL_MATERIAL_PROXY_LOWER_OVER_CAPACITY, UINT32_MAX,
			0u, source->source.declaration ); return qfalse;
	}
	if ( !builder.program.expressionFieldCount ) {
		ralMaterialProxyProgramReceipt_t empty;
		memset( &empty, 0, sizeof( empty ) );
		*outHasProgram = qfalse; *outReceipt = empty; return qtrue;
	}
	builder.program.expressionRegisterCount = builder.nextRegister;
	if ( !Ral_MaterialProxyProgramBuild( material, &builder.program, &candidate ) ) {
		Fail( &builder, RAL_MATERIAL_PROXY_LOWER_PROGRAM_REJECTED,
			UINT32_MAX, 0u, source->source.declaration ); return qfalse;
	}
	*outHasProgram = qtrue;
	*outReceipt = candidate;
	return qtrue;
}
