// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_proxy_lower.h"

#include <stdio.h>
#include <string.h>

#define CHECK(value) do { if ( !( value ) ) { \
	fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #value ); \
	return 1; } } while ( 0 )

static qboolean Material( ralMaterialReceipt_t *out ) {
	ralMaterialDescription_t description;
	memset( &description, 0, sizeof( description ) );
	description.schemaVersion = RAL_MATERIAL_SCHEMA_VERSION;
	description.materialGeneration = 3u;
	description.provenanceHash = 33u;
	description.source = RAL_MATERIAL_SOURCE_Q3;
	for ( uint32_t i = 0u; i < 4u; ++i )
		description.factors.baseColor[i] = 1.0f;
	description.factors.roughness = 1.0f;
	description.factors.normalScale = 1.0f;
	description.factors.occlusionStrength = 1.0f;
	description.renderPolicy.depthTest = qtrue;
	return Ral_MaterialCompile( &description, out );
}

static qboolean Source( ralMaterialSourceReceipt_t *out ) {
	ralMaterialSourceIr_t source;
	ralMaterialSourceDiagnostic_t diagnostic;
	memset( &source, 0, sizeof( source ) );
	source.schemaVersion = RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION;
	source.generation = 7u;
	source.provenance = RAL_MATERIAL_PROVENANCE_Q3_SHADER;
	source.declaration.sourceId = 41u;
	source.declaration.line = source.declaration.column = 1u;
	strcpy( source.semanticName, "textures/tests/proxy_lower" );
	source.diffuseReflectance[0] = source.diffuseReflectance[1] =
		source.diffuseReflectance[2] = 1.0f;
	source.pbrFactors.baseColor[0] = source.pbrFactors.baseColor[1] =
		source.pbrFactors.baseColor[2] = source.pbrFactors.baseColor[3] = 1.0f;
	source.pbrFactors.roughness = 1.0f;
	source.pbrFactors.normalScale = source.pbrFactors.occlusionStrength = 1.0f;
	source.pbrRenderPolicy.depthTest = qtrue;
	source.stageCount = 1u;
	source.stages[0].span = source.declaration;
	source.stages[0].mapKind = RAL_MATERIAL_MAP_WHITE;
	source.stages[0].dependencyIndex = UINT32_MAX;
	source.stages[0].rgbGen = RAL_MATERIAL_COLOR_WAVE;
	source.stages[0].rgbWave.function = RAL_MATERIAL_WAVE_SIN;
	source.stages[0].rgbWave.base = 1.0f;
	source.stages[0].rgbWave.amplitude = 0.5f;
	source.stages[0].rgbWave.frequency = 1.0f;
	source.stages[0].tcModCount = 4u;
	source.stages[0].tcMods[0].type = RAL_MATERIAL_TCMOD_SCROLL;
	source.stages[0].tcMods[0].parameters[0] = 0.25f;
	source.stages[0].tcMods[0].parameters[1] = -0.5f;
	source.stages[0].tcMods[1].type = RAL_MATERIAL_TCMOD_ROTATE;
	source.stages[0].tcMods[1].parameters[0] = 90.0f;
	source.stages[0].tcMods[2].type = RAL_MATERIAL_TCMOD_STRETCH;
	source.stages[0].tcMods[2].parameters[0] = RAL_MATERIAL_WAVE_SQUARE;
	source.stages[0].tcMods[2].parameters[1] = 2.0f;
	source.stages[0].tcMods[2].parameters[2] = 1.0f;
	source.stages[0].tcMods[2].parameters[4] = 1.0f;
	source.stages[0].tcMods[3].type = RAL_MATERIAL_TCMOD_TRANSFORM;
	for ( uint32_t i = 0u; i < 6u; ++i )
		source.stages[0].tcMods[3].parameters[i] = (float)i + 1.0f;
	for ( uint32_t i = 0u; i < source.stages[0].tcModCount; ++i )
		source.stages[0].tcMods[i].span = source.declaration;
	source.deformCount = 1u;
	source.deforms[0].span = source.declaration;
	source.deforms[0].type = RAL_MATERIAL_DEFORM_MOVE;
	source.deforms[0].parameters[3] = RAL_MATERIAL_WAVE_NOISE;
	source.deforms[0].parameters[4] = 0.5f;
	source.deforms[0].parameters[5] = 0.25f;
	source.deforms[0].parameters[7] = 1.0f;
	return Ral_MaterialSourceCompile( &source, out, &diagnostic );
}

static const ralMaterialExpressionField_t *FindField(
		const ralMaterialProxyProgramReceipt_t *receipt, const char *name ) {
	for ( uint32_t i = 0u; i < receipt->program.expressionFieldCount; ++i )
		if ( !strcmp( receipt->program.expressionFields[i].name, name ) )
			return &receipt->program.expressionFields[i];
	return NULL;
}

int main( void ) {
	ralMaterialReceipt_t material, invalidMaterial;
	ralMaterialSourceReceipt_t source, staticSource;
	ralMaterialProxyProgramReceipt_t receipt, beforeReceipt;
	ralMaterialProxyEvaluationReceipt_t evaluated;
	ralMaterialProxyLowerDiagnostic_t diagnostic;
	ralMaterialProxyInputs_t inputs;
	const ralMaterialExpressionField_t *field;
	qboolean hasProgram = qfalse, beforeHas;
	CHECK( Material( &material ) && Source( &source ) );
	CHECK( Ral_MaterialProxyLowerSource( &source, &material, &hasProgram,
		&receipt, &diagnostic ) && hasProgram );
	CHECK( Ral_MaterialProxyProgramReceiptValid( &receipt ) );
	CHECK( receipt.program.expressionInstructionCount > 0u );
	CHECK( receipt.program.expressionTableCount == 3u );
	CHECK( FindField( &receipt, "stage0.rgbWave" ) );
	CHECK( FindField( &receipt, "stage0.tc0" ) );
	CHECK( FindField( &receipt, "stage0.tc1" ) );
	CHECK( FindField( &receipt, "stage0.tc2" ) );
	CHECK( FindField( &receipt, "stage0.tc3" ) );
	CHECK( FindField( &receipt, "stage0.tc3.tail" ) );
	CHECK( FindField( &receipt, "deform0.wave" ) );
	memset( &inputs, 0, sizeof( inputs ) );
	inputs.evaluationGeneration = 1u;
	inputs.timeMilliseconds = 1000u;
	inputs.scalarQ16 = RAL_MATERIAL_PROXY_Q16_ONE;
	CHECK( Ral_MaterialProxyEvaluate( &material, &receipt, &inputs,
		RAL_MATERIAL_PROXY_DOMAIN_RUNTIME, RAL_MATERIAL_PROXY_VALUE_COUNT,
		&evaluated ) );
	field = FindField( &receipt, "stage0.rgbWave" );
	CHECK( field && evaluated.valuesQ16[field->outputValueOffset]
		>= RAL_MATERIAL_PROXY_Q16_ONE - 8
		&& evaluated.valuesQ16[field->outputValueOffset]
		<= RAL_MATERIAL_PROXY_Q16_ONE + 8 );
	field = FindField( &receipt, "stage0.tc0" );
	CHECK( field && evaluated.valuesQ16[field->outputValueOffset]
		== RAL_MATERIAL_PROXY_Q16_ONE / 4 );
	CHECK( evaluated.valuesQ16[field->outputValueOffset + 1u]
		== RAL_MATERIAL_PROXY_Q16_ONE / 2 );
	field = FindField( &receipt, "stage0.tc2" );
	CHECK( field && evaluated.valuesQ16[field->outputValueOffset]
		== RAL_MATERIAL_PROXY_Q16_ONE / 3 );

	/* No authored proxy facts is a successful, explicit empty product. */
	staticSource = source;
	staticSource.source.stages[0].rgbGen = RAL_MATERIAL_COLOR_IDENTITY;
	staticSource.source.stages[0].tcModCount = 0u;
	staticSource.source.deformCount = 0u;
	CHECK( Ral_MaterialSourceCompile( &staticSource.source, &staticSource, NULL ) );
	CHECK( Ral_MaterialProxyLowerSource( &staticSource, &material, &hasProgram,
		&receipt, &diagnostic ) && !hasProgram );
	CHECK( !memcmp( &receipt, &(ralMaterialProxyProgramReceipt_t){ 0 },
		sizeof( receipt ) ) );

	/* Failure never publishes a partial program or mutates the has-program bit. */
	CHECK( Ral_MaterialProxyLowerSource( &source, &material, &hasProgram,
		&receipt, &diagnostic ) );
	beforeReceipt = receipt; beforeHas = hasProgram;
	invalidMaterial = material; invalidMaterial.artifactHash++;
	CHECK( !Ral_MaterialProxyLowerSource( &source, &invalidMaterial, &hasProgram,
		&receipt, &diagnostic ) );
	CHECK( !memcmp( &receipt, &beforeReceipt, sizeof( receipt ) )
		&& hasProgram == beforeHas
		&& diagnostic.reason == RAL_MATERIAL_PROXY_LOWER_INVALID_MATERIAL );
	puts( "ral_material_proxy_lower_test: ok" );
	return 0;
}
