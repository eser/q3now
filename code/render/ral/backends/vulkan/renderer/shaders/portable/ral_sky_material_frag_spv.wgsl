struct MaterialTcMod {
    type_: u32,
    p0_: f32,
    p1_: f32,
    p2_: f32,
    p3_: f32,
    p4_: f32,
    p5_: f32,
    reserved: u32,
}

struct MaterialStageParams {
    meta_: vec4<u32>,
    generators: vec4<u32>,
    params: vec4<f32>,
    scaleScroll: vec4<f32>,
    turbulence: vec4<f32>,
    stretch: vec4<f32>,
    constantColor: vec4<f32>,
    rgbWave: vec4<f32>,
    alphaWave: vec4<f32>,
    tcGenVectorS: vec4<f32>,
    tcGenVectorT: vec4<f32>,
    tcModMeta: vec4<u32>,
    tcMods: array<MaterialTcMod, 8>,
}

struct ProductDraw {
    viewOriginDrawSpace: vec4<f32>,
    viewForward: vec4<f32>,
    viewLeft: vec4<f32>,
    viewUp: vec4<f32>,
    projectionLightmap: vec4<f32>,
    viewportAlpha: vec4<f32>,
    atmosphereEyeDensity: vec4<f32>,
    atmosphereColorVisibility: vec4<f32>,
    atmosphereHeightSky: vec4<f32>,
    atmosphereFroxelGrid: vec4<u32>,
    localSh: array<vec4<f32>, 4>,
    staticLighting: vec4<u32>,
    emissiveRadiance: vec4<f32>,
}

struct SkyMaterial {
    meta_: vec4<u32>,
    timeHeight: vec4<f32>,
    stages: array<MaterialStageParams, 8>,
}

@group(0) @binding(0)
var<uniform> productDraw: ProductDraw;
@group(1) @binding(0)
var stageImage0_: texture_2d<f32>;
@group(1) @binding(1)
var stageSampler0_: sampler;
@group(1) @binding(2)
var stageImage1_: texture_2d<f32>;
@group(1) @binding(3)
var stageSampler1_: sampler;
@group(1) @binding(4)
var stageImage2_: texture_2d<f32>;
@group(1) @binding(5)
var stageSampler2_: sampler;
@group(1) @binding(6)
var stageImage3_: texture_2d<f32>;
@group(1) @binding(7)
var stageSampler3_: sampler;
@group(1) @binding(8)
var stageImage4_: texture_2d<f32>;
@group(1) @binding(9)
var stageSampler4_: sampler;
@group(1) @binding(10)
var stageImage5_: texture_2d<f32>;
@group(1) @binding(11)
var stageSampler5_: sampler;
@group(1) @binding(12)
var stageImage6_: texture_2d<f32>;
@group(1) @binding(13)
var stageSampler6_: sampler;
@group(1) @binding(14)
var stageImage7_: texture_2d<f32>;
@group(1) @binding(15)
var stageSampler7_: sampler;
@group(1) @binding(16)
var outerImage: texture_2d<f32>;
@group(1) @binding(17)
var outerSampler: sampler;
var<private> worldPosition_1: vec3<f32>;
@group(2) @binding(1)
var<uniform> skyMaterial: SkyMaterial;
var<private> lightmapCoord_1: vec2<f32>;
var<private> worldNormal_1: vec3<f32>;
var<private> color_2: vec4<f32>;
@group(1) @binding(18)
var physicalImage: texture_2d<f32>;
@group(1) @binding(19)
var physicalSampler: sampler;
var<private> outColor: vec4<f32>;
var<private> texCoord_1: vec2<f32>;

fn wired_display_visibility_u0028_vf3_u003b(sceneColor: ptr<function, vec3<f32>>) -> vec3<f32> {
    var exposed: vec3<f32>;
    var luminance: f32;
    var curved: f32;
    var phi_1646_: bool;
    var phi_1652_: bool;

    let _e519 = (*sceneColor);
    let _e523 = productDraw.viewForward[3u];
    exposed = (max(_e519, vec3<f32>(0f, 0f, 0f)) * _e523);
    let _e525 = exposed;
    luminance = dot(_e525, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e527 = luminance;
    let _e528 = (_e527 > 0f);
    phi_1646_ = _e528;
    if _e528 {
        let _e529 = luminance;
        let _e532 = productDraw.viewUp[3u];
        phi_1646_ = (_e529 < _e532);
    }
    let _e535 = phi_1646_;
    phi_1652_ = _e535;
    if _e535 {
        let _e538 = productDraw.viewLeft[3u];
        phi_1652_ = (_e538 != 1f);
    }
    let _e541 = phi_1652_;
    if _e541 {
        let _e542 = luminance;
        let _e545 = productDraw.viewUp[3u];
        let _e549 = productDraw.viewLeft[3u];
        let _e553 = productDraw.viewUp[3u];
        curved = (pow((_e542 / _e545), _e549) * _e553);
        let _e555 = curved;
        let _e556 = luminance;
        let _e558 = exposed;
        exposed = (_e558 * (_e555 / _e556));
    }
    let _e560 = exposed;
    return _e560;
}

fn wired_compose_authored_physical_sky_u0028_vf3_u003b_vf3_u003b_f1_u003b(authored: ptr<function, vec3<f32>>, physical: ptr<function, vec3<f32>>, blend: ptr<function, f32>) -> vec3<f32> {
    var authoredEnergy: f32;
    var physicalEnergy: f32;
    var scale: f32;
    var physicalTint: vec3<f32>;
    var local: vec3<f32>;
    var relit: vec3<f32>;

    let _e524 = (*authored);
    authoredEnergy = dot(max(_e524, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e527 = (*physical);
    physicalEnergy = dot(max(_e527, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e530 = authoredEnergy;
    if (_e530 <= 0.00001f) {
        let _e532 = (*authored);
        let _e533 = (*physical);
        let _e534 = (*blend);
        return mix(_e532, _e533, vec3(_e534));
    }
    let _e537 = physicalEnergy;
    let _e538 = authoredEnergy;
    scale = clamp((_e537 / max(_e538, 0.18f)), 0.65f, 1.75f);
    let _e542 = physicalEnergy;
    if (_e542 > 0.00001f) {
        let _e544 = (*physical);
        let _e545 = physicalEnergy;
        local = (_e544 / vec3(_e545));
    } else {
        local = vec3<f32>(1f, 1f, 1f);
    }
    let _e548 = local;
    physicalTint = _e548;
    let _e549 = (*authored);
    let _e550 = scale;
    let _e552 = physicalTint;
    let _e557 = (*physical);
    relit = (((_e549 * _e550) * mix(vec3<f32>(1f, 1f, 1f), clamp(_e552, vec3<f32>(0.5f, 0.5f, 0.5f), vec3<f32>(2f, 2f, 2f)), vec3(0.12f))) + (_e557 * 0.08f));
    let _e560 = (*authored);
    let _e561 = relit;
    let _e562 = (*blend);
    return mix(_e560, _e561, vec3(_e562));
}

fn wired_material_blend_factor_u0028_u1_u003b_vf4_u003b_vf4_u003b(factor: ptr<function, u32>, source: ptr<function, vec4<f32>>, destination: ptr<function, vec4<f32>>) -> vec4<f32> {
    let _e518 = (*factor);
    if (_e518 == 0u) {
        return vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e520 = (*factor);
    if (_e520 == 1u) {
        return vec4<f32>(1f, 1f, 1f, 1f);
    }
    let _e522 = (*factor);
    if (_e522 == 2u) {
        let _e524 = (*source);
        return _e524;
    }
    let _e525 = (*factor);
    if (_e525 == 3u) {
        let _e527 = (*source);
        return (vec4(1f) - _e527);
    }
    let _e530 = (*factor);
    if (_e530 == 4u) {
        let _e532 = (*destination);
        return _e532;
    }
    let _e533 = (*factor);
    if (_e533 == 5u) {
        let _e535 = (*destination);
        return (vec4(1f) - _e535);
    }
    let _e538 = (*factor);
    if (_e538 == 6u) {
        let _e541 = (*source)[3u];
        return vec4(_e541);
    }
    let _e543 = (*factor);
    if (_e543 == 7u) {
        let _e546 = (*source)[3u];
        return vec4((1f - _e546));
    }
    let _e549 = (*factor);
    if (_e549 == 8u) {
        let _e552 = (*destination)[3u];
        return vec4(_e552);
    }
    let _e554 = (*factor);
    if (_e554 == 9u) {
        let _e557 = (*destination)[3u];
        return vec4((1f - _e557));
    }
    let _e561 = (*source)[3u];
    let _e563 = (*destination)[3u];
    return vec4(min(_e561, (1f - _e563)));
}

fn wired_stage_srgb_to_linear_u0028_vf3_u003b(value: ptr<function, vec3<f32>>) -> vec3<f32> {
    var low: vec3<f32>;
    var high: vec3<f32>;

    let _e518 = (*value);
    (*value) = max(_e518, vec3<f32>(0f, 0f, 0f));
    let _e520 = (*value);
    low = (_e520 / vec3(12.92f));
    let _e523 = (*value);
    high = pow(((_e523 + vec3(0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e529 = high;
    let _e530 = low;
    let _e531 = (*value);
    return select(_e529, _e530, (_e531 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f)));
}

fn wired_material_table_sine_u0028_u1_u003b(index: ptr<function, u32>) -> f32 {
    let _e516 = (*index);
    return sin(((f32((_e516 & 1023u)) * 0.0061359233f) + 0.0001f));
}

fn wired_stage_remainder_u0028_f1_u003b_f1_u003b(x: ptr<function, f32>, y: ptr<function, f32>) -> f32 {
    let _e517 = (*x);
    let _e518 = (*x);
    let _e519 = (*y);
    let _e522 = (*y);
    return (_e517 - (trunc((_e518 / _e519)) * _e522));
}

fn wired_material_table_index_u0028_f1_u003b(phase: ptr<function, f32>) -> u32 {
    var param: f32;
    var param_1: f32;

    let _e518 = (*phase);
    param = _e518;
    param_1 = 1f;
    let _e519 = wired_stage_remainder_u0028_f1_u003b_f1_u003b((&param), (&param_1));
    return (bitcast<u32>(i32((_e519 * 1024f))) & 1023u);
}

fn wired_material_noise_sample_u0028_u1_u003b(cell: ptr<function, u32>) -> f32 {
    var index_1: u32;
    var axis: u32;
    var indexable: array<u32, 256>;
    var indexable_1: array<f32, 256>;

    let _e520 = (*cell);
    index_1 = (_e520 & 255u);
    axis = 0u;
    loop {
        let _e522 = axis;
        if (_e522 < 4u) {
            let _e524 = index_1;
            indexable = array<u32, 256>(18u, 42u, 224u, 18u, 127u, 202u, 208u, 24u, 70u, 254u, 54u, 61u, 200u, 211u, 205u, 144u, 254u, 100u, 134u, 97u, 96u, 130u, 241u, 133u, 221u, 203u, 90u, 77u, 213u, 58u, 234u, 220u, 0u, 158u, 246u, 141u, 157u, 3u, 238u, 146u, 168u, 162u, 43u, 113u, 73u, 133u, 254u, 103u, 137u, 108u, 231u, 93u, 14u, 104u, 164u, 75u, 225u, 179u, 249u, 27u, 131u, 250u, 17u, 93u, 101u, 177u, 30u, 133u, 92u, 155u, 93u, 97u, 253u, 2u, 131u, 9u, 70u, 98u, 208u, 22u, 63u, 231u, 183u, 196u, 206u, 251u, 169u, 33u, 209u, 18u, 13u, 170u, 240u, 196u, 181u, 236u, 93u, 149u, 75u, 253u, 126u, 14u, 125u, 250u, 195u, 117u, 124u, 15u, 248u, 143u, 153u, 77u, 142u, 193u, 14u, 204u, 142u, 42u, 250u, 246u, 7u, 17u, 183u, 101u, 210u, 38u, 134u, 52u, 38u, 76u, 172u, 16u, 127u, 119u, 94u, 61u, 160u, 103u, 11u, 134u, 240u, 122u, 247u, 105u, 47u, 120u, 125u, 36u, 112u, 9u, 99u, 101u, 206u, 228u, 212u, 1u, 55u, 170u, 41u, 16u, 217u, 127u, 134u, 51u, 114u, 170u, 36u, 202u, 133u, 185u, 143u, 195u, 100u, 18u, 124u, 76u, 131u, 73u, 17u, 189u, 195u, 61u, 38u, 140u, 245u, 123u, 135u, 255u, 191u, 241u, 205u, 244u, 111u, 129u, 227u, 23u, 90u, 200u, 136u, 9u, 87u, 102u, 183u, 173u, 22u, 253u, 147u, 248u, 0u, 55u, 241u, 95u, 57u, 69u, 221u, 111u, 175u, 80u, 227u, 117u, 47u, 120u, 20u, 165u, 15u, 200u, 191u, 192u, 182u, 62u, 28u, 42u, 208u, 122u, 132u, 249u, 58u, 236u, 143u, 112u, 3u, 250u, 146u, 57u, 127u, 74u, 34u, 215u, 148u, 148u, 86u, 223u, 57u, 243u, 124u, 5u);
            let _e526 = indexable[_e524];
            index_1 = _e526;
            continue;
        } else {
            break;
        }
        continuing {
            let _e527 = axis;
            axis = (_e527 + bitcast<u32>(1i));
        }
    }
    let _e530 = index_1;
    indexable_1 = array<f32, 256>(0.8469031f, 0.47616637f, 0.21855819f, -0.39615327f, 0.39833248f, -0.23092157f, 0.016733766f, 0.32460082f, 0.8264301f, 0.7141863f, 0.54239666f, -0.25759047f, 0.37415636f, -0.31669623f, -0.29938978f, -0.68850935f, 0.29332948f, -0.04988092f, 0.71274126f, -0.25778574f, 0.103577495f, -0.41324478f, 0.85156703f, -0.044953406f, -0.2957458f, 0.16786718f, 0.15998745f, -0.86378145f, 0.053961515f, 0.66703904f, 0.9344952f, 0.2743982f, 0.88002634f, 0.9776896f, 0.994329f, 0.12602079f, 0.43463063f, -0.042842925f, 0.9703553f, -0.61084926f, -0.77226996f, -0.6985575f, 0.8035594f, -0.26996213f, -0.572654f, 0.6617917f, -0.5263469f, 0.5106157f, 0.6724851f, 0.20376396f, -0.897998f, -0.35112864f, -0.6254122f, 0.21080363f, 0.40637946f, -0.12594455f, 0.7685201f, 0.0058778524f, 0.10742557f, -0.3571027f, 0.7327f, -0.13852674f, -0.49482614f, -0.8643447f, 0.3038231f, -0.23077399f, 0.6168364f, 0.21628118f, 0.93993735f, -0.35099918f, -0.15524966f, -0.021139681f, 0.09912431f, -0.4076547f, 0.38609362f, 0.25821447f, 0.046231627f, 0.8098897f, -0.37749976f, 0.9888071f, -0.6303477f, -0.26814288f, -0.31183988f, 0.15625179f, -0.7796062f, 0.09842801f, -0.05072981f, 0.582808f, 0.81344366f, 0.14925206f, -0.9560602f, 0.61637414f, -0.092731535f, -0.6954373f, 0.8092247f, 0.5324365f, -0.6154182f, -0.29161483f, -0.70310366f, 0.21504116f, -0.7022008f, 0.9377383f, 0.97705483f, 0.03877473f, -0.69478965f, 0.38984442f, 0.6148282f, -0.57611346f, 0.91212595f, 0.74049485f, 0.7025621f, 0.3395071f, 0.74446225f, 0.94713306f, -0.67959726f, 0.120883346f, 0.16995502f, -0.5482242f, 0.97989154f, -0.48022193f, -0.37604982f, 0.7015846f, -0.32008308f, -0.41527098f, 0.6949965f, 0.99238575f, -0.46843714f, -0.92288625f, 0.9688833f, 0.15485096f, 0.10692072f, -0.35389692f, 0.0017485619f, -0.09468871f, 0.5620551f, -0.8520483f, -0.28308982f, -0.49305362f, -0.6841395f, -0.4818645f, -0.7125609f, 0.13562608f, 0.12707162f, -0.32404405f, -0.9284431f, 0.8934244f, -0.564576f, 0.8527281f, -0.25694007f, -0.97056794f, 0.47224367f, 0.2523452f, 0.6429974f, -0.28594452f, 0.6529108f, -0.40799206f, 0.75336504f, -0.9466928f, -0.84182966f, -0.26508647f, 0.5766138f, 0.60028374f, 0.51994145f, -0.23579544f, -0.08374125f, -0.33356935f, -0.30988055f, 0.5323293f, -0.996716f, -0.55436146f, 0.2665515f, -0.9786258f, 0.50795853f, 0.7976849f, -0.10953015f, -0.44617242f, 0.8404442f, 0.60429657f, -0.07405478f, 0.7842498f, 0.698086f, -0.37783688f, -0.00074344873f, -0.5424825f, -0.6844158f, 0.9215293f, -0.37232345f, 0.14719069f, -0.74398446f, 0.21657062f, -0.12075871f, 0.88769686f, -0.32179815f, -0.028252065f, 0.63584507f, 0.70286345f, -0.08597416f, 0.65325916f, -0.149723f, 0.5486723f, -0.08077663f, -0.8520131f, 0.6952628f, 0.74621975f, -0.9170834f, 0.4172858f, 0.95932746f, 0.2251041f, -0.59962666f, 0.5296767f, -0.60249746f, 0.7849034f, -0.9684094f, -0.8301374f, 0.3747933f, -0.73390484f, -0.71786344f, -0.71185696f, 0.4871862f, 0.45765078f, -0.94725704f, -0.25358766f, 0.55516243f, -0.54417217f, -0.4209221f, 0.29355347f, 0.90264106f, 0.069580674f, -0.31801087f, -0.7033422f, 0.6022372f, 0.9443332f, 0.43932247f, -0.7470752f, -0.86122906f, -0.14853984f, -0.2159577f, 0.28032398f, -0.66258276f, 0.92431784f, 0.821825f, 0.8368082f, 0.0973649f, -0.13240129f, -0.8209977f, -0.90698063f, 0.9713893f, -0.2620738f, 0.8247925f, -0.5282723f, 0.14774466f, 0.07994926f, 0.63386023f, -0.48508543f, 0.43871665f, 0.6949941f);
    let _e532 = indexable_1[_e530];
    return _e532;
}

fn wired_material_wave_u0028_u1_u003b_vf4_u003b_f1_u003b(function_: ptr<function, u32>, wave: ptr<function, vec4<f32>>, time: ptr<function, f32>) -> f32 {
    var position: f32;
    var wrapped: f32;
    var param_2: f32;
    var param_3: f32;
    var cell_1: u32;
    var a: f32;
    var param_4: u32;
    var b: f32;
    var param_5: u32;
    var phase_1: f32;
    var index_2: u32;
    var param_6: f32;
    var cycle: f32;
    var value_1: f32;
    var param_7: u32;
    var halfIndex: u32;
    var quarterIndex: u32;
    var local_1: f32;

    let _e536 = (*function_);
    if (_e536 == 6u) {
        let _e538 = (*time);
        let _e540 = (*wave)[2u];
        let _e543 = (*wave)[3u];
        position = ((_e538 + _e540) * _e543);
        let _e545 = position;
        param_2 = _e545;
        param_3 = 256f;
        let _e546 = wired_stage_remainder_u0028_f1_u003b_f1_u003b((&param_2), (&param_3));
        wrapped = _e546;
        let _e547 = wrapped;
        cell_1 = (bitcast<u32>(i32(floor(_e547))) & 255u);
        let _e552 = cell_1;
        param_4 = _e552;
        let _e553 = wired_material_noise_sample_u0028_u1_u003b((&param_4));
        a = _e553;
        let _e554 = cell_1;
        param_5 = (_e554 + 1u);
        let _e556 = wired_material_noise_sample_u0028_u1_u003b((&param_5));
        b = _e556;
        let _e558 = (*wave)[0u];
        let _e560 = (*wave)[1u];
        let _e561 = a;
        let _e562 = b;
        let _e563 = a;
        let _e565 = wrapped;
        return (_e558 + (_e560 * (_e561 + ((_e562 - _e563) * fract(_e565)))));
    }
    let _e572 = (*wave)[2u];
    let _e573 = (*time);
    let _e575 = (*wave)[3u];
    phase_1 = (_e572 + (_e573 * _e575));
    let _e578 = phase_1;
    param_6 = _e578;
    let _e579 = wired_material_table_index_u0028_f1_u003b((&param_6));
    index_2 = _e579;
    let _e580 = index_2;
    cycle = (f32(_e580) / 1024f);
    let _e583 = (*function_);
    if (_e583 == 1u) {
        let _e585 = index_2;
        param_7 = _e585;
        let _e586 = wired_material_table_sine_u0028_u1_u003b((&param_7));
        value_1 = _e586;
    } else {
        let _e587 = (*function_);
        if (_e587 == 2u) {
            let _e589 = index_2;
            value_1 = select(-1f, 1f, (_e589 < 512u));
        } else {
            let _e592 = (*function_);
            if (_e592 == 3u) {
                let _e594 = index_2;
                halfIndex = (_e594 & 511u);
                let _e596 = halfIndex;
                quarterIndex = (_e596 & 255u);
                let _e598 = quarterIndex;
                if (_e598 == 0u) {
                    local_1 = 0.000001f;
                } else {
                    let _e600 = quarterIndex;
                    local_1 = (f32(_e600) / 256f);
                }
                let _e603 = local_1;
                value_1 = _e603;
                let _e604 = halfIndex;
                if (_e604 >= 256u) {
                    let _e606 = value_1;
                    value_1 = (1f - _e606);
                }
                let _e608 = index_2;
                if (_e608 >= 512u) {
                    let _e610 = value_1;
                    value_1 = -(_e610);
                }
            } else {
                let _e612 = (*function_);
                if (_e612 == 4u) {
                    let _e614 = index_2;
                    let _e616 = cycle;
                    value_1 = select(_e616, 0.000001f, (_e614 == 0u));
                } else {
                    let _e618 = (*function_);
                    if (_e618 == 5u) {
                        let _e620 = index_2;
                        let _e622 = cycle;
                        value_1 = (1f - select(_e622, 0.000001f, (_e620 == 0u)));
                    } else {
                        value_1 = 0f;
                    }
                }
            }
        }
    }
    let _e626 = (*wave)[0u];
    let _e628 = (*wave)[1u];
    let _e629 = value_1;
    return (_e626 + (_e628 * _e629));
}

fn wired_material_stage_generators_u0028_vf4_u003b_struct_u002d_MaterialStageParams_u002d_vu4_u002d_vu4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vu4_u002d_struct_u002d_MaterialTcMod_u002d_u1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_u11_u005b_8_u005d_1_u003b_vf4_u003b_vf4_u003b_vf3_u003b_f1_u003b_f1_u003b(source_1: ptr<function, vec4<f32>>, stage: ptr<function, MaterialStageParams>, vertexColor: ptr<function, vec4<f32>>, entityColor: ptr<function, vec4<f32>>, diffuseLighting: ptr<function, vec3<f32>>, vertexAlpha: ptr<function, f32>, time_1: ptr<function, f32>) -> vec4<f32> {
    var rgbGen: u32;
    var alphaGen: u32;
    var rgb: vec3<f32>;
    var alpha: f32;
    var local_2: vec3<f32>;
    var local_3: f32;
    var param_8: u32;
    var param_9: vec4<f32>;
    var param_10: f32;
    var color_1: vec4<f32>;
    var param_11: vec3<f32>;
    var local_4: f32;
    var param_12: u32;
    var param_13: vec4<f32>;
    var param_14: f32;

    let _e539 = (*stage).generators[0u];
    rgbGen = _e539;
    let _e542 = (*stage).generators[1u];
    alphaGen = _e542;
    rgb = vec3<f32>(1f, 1f, 1f);
    alpha = 1f;
    let _e543 = rgbGen;
    let _e545 = rgbGen;
    if ((_e543 == 3u) || (_e545 == 4u)) {
        let _e548 = rgbGen;
        if (_e548 == 3u) {
            let _e550 = (*entityColor);
            local_2 = _e550.xyz;
        } else {
            let _e552 = (*entityColor);
            local_2 = (vec3(1f) - _e552.xyz);
        }
        let _e556 = local_2;
        rgb = _e556;
        let _e557 = rgbGen;
        if (_e557 == 3u) {
            let _e560 = (*entityColor)[3u];
            local_3 = _e560;
        } else {
            let _e562 = (*entityColor)[3u];
            local_3 = (1f - _e562);
        }
        let _e564 = local_3;
        alpha = _e564;
    } else {
        let _e565 = rgbGen;
        let _e567 = rgbGen;
        if ((_e565 == 5u) || (_e567 == 6u)) {
            let _e570 = (*vertexColor);
            rgb = _e570.xyz;
            let _e573 = (*vertexColor)[3u];
            alpha = _e573;
        } else {
            let _e574 = rgbGen;
            if (_e574 == 7u) {
                let _e576 = (*vertexColor);
                rgb = (vec3(1f) - _e576.xyz);
            } else {
                let _e580 = rgbGen;
                if (_e580 == 8u) {
                    let _e584 = (*stage).generators[2u];
                    param_8 = _e584;
                    let _e586 = (*stage).rgbWave;
                    param_9 = _e586;
                    let _e587 = (*time_1);
                    param_10 = _e587;
                    let _e588 = wired_material_wave_u0028_u1_u003b_vf4_u003b_f1_u003b((&param_8), (&param_9), (&param_10));
                    rgb = vec3((floor((clamp(_e588, 0f, 1f) * 255f)) / 255f));
                } else {
                    let _e594 = rgbGen;
                    if (_e594 == 9u) {
                        let _e596 = (*diffuseLighting);
                        rgb = _e596;
                    } else {
                        let _e597 = rgbGen;
                        if (_e597 == 11u) {
                            let _e600 = (*stage).constantColor;
                            color_1 = (floor((clamp(_e600, vec4(0f), vec4(1f)) * 255f)) / vec4(255f));
                            let _e608 = color_1;
                            rgb = _e608.xyz;
                            let _e611 = color_1[3u];
                            alpha = _e611;
                        }
                    }
                }
            }
        }
    }
    let _e612 = rgbGen;
    if (_e612 != 9u) {
        let _e614 = rgb;
        param_11 = _e614;
        let _e615 = wired_stage_srgb_to_linear_u0028_vf3_u003b((&param_11));
        rgb = _e615;
    }
    let _e616 = alphaGen;
    let _e618 = rgbGen;
    if ((_e616 == 0u) && (_e618 != 6u)) {
        alpha = 1f;
    } else {
        let _e621 = alphaGen;
        let _e623 = alphaGen;
        if ((_e621 == 2u) || (_e623 == 3u)) {
            let _e626 = alphaGen;
            if (_e626 == 2u) {
                let _e629 = (*entityColor)[3u];
                local_4 = _e629;
            } else {
                let _e631 = (*entityColor)[3u];
                local_4 = (1f - _e631);
            }
            let _e633 = local_4;
            alpha = _e633;
        } else {
            let _e634 = alphaGen;
            if (_e634 == 4u) {
                let _e637 = (*vertexColor)[3u];
                alpha = _e637;
            } else {
                let _e638 = alphaGen;
                if (_e638 == 5u) {
                    let _e641 = (*vertexColor)[3u];
                    alpha = (1f - _e641);
                } else {
                    let _e643 = alphaGen;
                    if (_e643 == 7u) {
                        let _e647 = (*stage).generators[3u];
                        param_12 = _e647;
                        let _e649 = (*stage).alphaWave;
                        param_13 = _e649;
                        let _e650 = (*time_1);
                        param_14 = _e650;
                        let _e651 = wired_material_wave_u0028_u1_u003b_vf4_u003b_f1_u003b((&param_12), (&param_13), (&param_14));
                        alpha = (floor((clamp(_e651, 0f, 1f) * 255f)) / 255f);
                    } else {
                        let _e656 = alphaGen;
                        let _e658 = alphaGen;
                        if ((_e656 == 6u) || (_e658 == 8u)) {
                            let _e661 = (*vertexAlpha);
                            alpha = _e661;
                        } else {
                            let _e662 = alphaGen;
                            if (_e662 == 9u) {
                                let _e666 = (*stage).constantColor[3u];
                                alpha = (floor((clamp(_e666, 0f, 1f) * 255f)) / 255f);
                            }
                        }
                    }
                }
            }
        }
    }
    let _e671 = rgb;
    let _e672 = (*source_1);
    let _e674 = (_e672.xyz * _e671);
    (*source_1)[0u] = _e674.x;
    (*source_1)[1u] = _e674.y;
    (*source_1)[2u] = _e674.z;
    let _e681 = alpha;
    let _e683 = (*source_1)[3u];
    (*source_1)[3u] = (_e683 * _e681);
    let _e686 = (*source_1);
    return _e686;
}

fn wired_sky_stage_sample_u0028_u1_u003b_vf2_u003b(slot: ptr<function, u32>, uv: ptr<function, vec2<f32>>) -> vec4<f32> {
    let _e517 = (*slot);
    switch bitcast<i32>(_e517) {
        case 0: {
            let _e521 = (*uv);
            let _e522 = textureSample(stageImage0_, stageSampler0_, _e521);
            return _e522;
        }
        case 1: {
            let _e523 = (*uv);
            let _e524 = textureSample(stageImage1_, stageSampler1_, _e523);
            return _e524;
        }
        case 2: {
            let _e525 = (*uv);
            let _e526 = textureSample(stageImage2_, stageSampler2_, _e525);
            return _e526;
        }
        case 3: {
            let _e527 = (*uv);
            let _e528 = textureSample(stageImage3_, stageSampler3_, _e527);
            return _e528;
        }
        case 4: {
            let _e529 = (*uv);
            let _e530 = textureSample(stageImage4_, stageSampler4_, _e529);
            return _e530;
        }
        case 5: {
            let _e531 = (*uv);
            let _e532 = textureSample(stageImage5_, stageSampler5_, _e531);
            return _e532;
        }
        case 6: {
            let _e533 = (*uv);
            let _e534 = textureSample(stageImage6_, stageSampler6_, _e533);
            return _e534;
        }
        default: {
            let _e519 = (*uv);
            let _e520 = textureSample(stageImage7_, stageSampler7_, _e519);
            return _e520;
        }
    }
}

fn wired_stage_copy_sign_u0028_f1_u003b_f1_u003b(magnitude: ptr<function, f32>, signSource: ptr<function, f32>) -> f32 {
    let _e517 = (*magnitude);
    let _e520 = (*signSource);
    return bitcast<f32>(((bitcast<u32>(_e517) & 2147483647u) | (bitcast<u32>(_e520) & 2147483648u)));
}

fn wired_material_stage_uv_u0028_vf2_u003b_struct_u002d_MaterialStageParams_u002d_vu4_u002d_vu4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vu4_u002d_struct_u002d_MaterialTcMod_u002d_u1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_u11_u005b_8_u005d_1_u003b_f1_u003b_vf3_u003b(authoredUv: ptr<function, vec2<f32>>, stage_1: ptr<function, MaterialStageParams>, time_2: ptr<function, f32>, position_1: ptr<function, vec3<f32>>) -> vec2<f32> {
    var uv_1: vec2<f32>;
    var i: u32;
    var op: MaterialTcMod;
    var index_3: u32;
    var param_15: f32;
    var sine: f32;
    var param_16: u32;
    var cosine: f32;
    var param_17: u32;
    var centered: vec2<f32>;
    var phase_2: f32;
    var coordinate: vec2<f32>;
    var param_18: f32;
    var param_19: u32;
    var param_20: f32;
    var param_21: u32;
    var wave_1: f32;
    var param_22: u32;
    var param_23: vec4<f32>;
    var param_24: f32;
    var param_25: f32;
    var param_26: f32;
    var uv_2: vec2<f32>;
    var wave_2: f32;
    var angle: f32;
    var sine_1: f32;
    var cosine_1: f32;
    var centered_1: vec2<f32>;
    var phase_3: f32;
    var offset: f32;

    let _e551 = (*stage_1).tcModMeta[0u];
    if (_e551 > 0u) {
        let _e553 = (*authoredUv);
        uv_1 = _e553;
        i = 0u;
        loop {
            let _e554 = i;
            let _e557 = (*stage_1).tcModMeta[0u];
            if (_e554 < min(_e557, 8u)) {
                let _e560 = i;
                let _e563 = (*stage_1).tcMods[_e560];
                op = _e563;
                let _e565 = op.type_;
                if (_e565 == 1u) {
                    let _e568 = op.p0_;
                    let _e570 = op.p1_;
                    let _e572 = uv_1;
                    uv_1 = (_e572 * vec2<f32>(_e568, _e570));
                } else {
                    let _e575 = op.type_;
                    if (_e575 == 2u) {
                        let _e578 = op.p0_;
                        let _e580 = op.p1_;
                        let _e582 = (*time_2);
                        let _e585 = uv_1;
                        uv_1 = (_e585 + fract((vec2<f32>(_e578, _e580) * _e582)));
                    } else {
                        let _e588 = op.type_;
                        if (_e588 == 3u) {
                            let _e591 = op.p0_;
                            let _e593 = (*time_2);
                            param_15 = ((-(_e591) * _e593) / 360f);
                            let _e596 = wired_material_table_index_u0028_f1_u003b((&param_15));
                            index_3 = _e596;
                            let _e597 = index_3;
                            param_16 = _e597;
                            let _e598 = wired_material_table_sine_u0028_u1_u003b((&param_16));
                            sine = _e598;
                            let _e599 = index_3;
                            param_17 = (_e599 + 256u);
                            let _e601 = wired_material_table_sine_u0028_u1_u003b((&param_17));
                            cosine = _e601;
                            let _e602 = uv_1;
                            centered = (_e602 - vec2(0.5f));
                            let _e606 = centered[0u];
                            let _e607 = cosine;
                            let _e610 = centered[1u];
                            let _e611 = sine;
                            let _e615 = centered[0u];
                            let _e616 = sine;
                            let _e619 = centered[1u];
                            let _e620 = cosine;
                            uv_1 = (vec2<f32>(((_e606 * _e607) - (_e610 * _e611)), ((_e615 * _e616) + (_e619 * _e620))) + vec2(0.5f));
                        } else {
                            let _e627 = op.type_;
                            if (_e627 == 4u) {
                                let _e630 = op.p2_;
                                let _e631 = (*time_2);
                                let _e633 = op.p3_;
                                phase_2 = (_e630 + (_e631 * _e633));
                                let _e637 = (*position_1)[0u];
                                let _e639 = (*position_1)[2u];
                                let _e642 = (*position_1)[1u];
                                let _e646 = phase_2;
                                coordinate = ((vec2<f32>((_e637 + _e639), _e642) / vec2(1024f)) + vec2(_e646));
                                let _e650 = coordinate[0u];
                                param_18 = _e650;
                                let _e651 = wired_material_table_index_u0028_f1_u003b((&param_18));
                                param_19 = _e651;
                                let _e652 = wired_material_table_sine_u0028_u1_u003b((&param_19));
                                let _e654 = coordinate[1u];
                                param_20 = _e654;
                                let _e655 = wired_material_table_index_u0028_f1_u003b((&param_20));
                                param_21 = _e655;
                                let _e656 = wired_material_table_sine_u0028_u1_u003b((&param_21));
                                let _e659 = op.p1_;
                                let _e661 = uv_1;
                                uv_1 = (_e661 + (vec2<f32>(_e652, _e656) * _e659));
                            } else {
                                let _e664 = op.type_;
                                if (_e664 == 5u) {
                                    let _e667 = op.p0_;
                                    let _e670 = op.p1_;
                                    let _e672 = op.p2_;
                                    let _e674 = op.p3_;
                                    let _e676 = op.p4_;
                                    param_22 = u32(_e667);
                                    param_23 = vec4<f32>(_e670, _e672, _e674, _e676);
                                    let _e678 = (*time_2);
                                    param_24 = _e678;
                                    let _e679 = wired_material_wave_u0028_u1_u003b_vf4_u003b_f1_u003b((&param_22), (&param_23), (&param_24));
                                    wave_1 = _e679;
                                    let _e680 = wave_1;
                                    param_25 = max(abs(_e680), 0.0001f);
                                    let _e683 = wave_1;
                                    param_26 = _e683;
                                    let _e684 = wired_stage_copy_sign_u0028_f1_u003b_f1_u003b((&param_25), (&param_26));
                                    wave_1 = _e684;
                                    let _e685 = uv_1;
                                    let _e688 = wave_1;
                                    uv_1 = (((_e685 - vec2(0.5f)) / vec2(_e688)) + vec2(0.5f));
                                } else {
                                    let _e694 = op.type_;
                                    if (_e694 == 6u) {
                                        let _e697 = uv_1[0u];
                                        let _e699 = op.p0_;
                                        let _e702 = uv_1[1u];
                                        let _e704 = op.p2_;
                                        let _e708 = op.p4_;
                                        let _e711 = uv_1[0u];
                                        let _e713 = op.p1_;
                                        let _e716 = uv_1[1u];
                                        let _e718 = op.p3_;
                                        let _e722 = op.p5_;
                                        uv_1 = vec2<f32>((((_e697 * _e699) + (_e702 * _e704)) + _e708), (((_e711 * _e713) + (_e716 * _e718)) + _e722));
                                    }
                                }
                            }
                        }
                    }
                }
                continue;
            } else {
                break;
            }
            continuing {
                let _e725 = i;
                i = (_e725 + bitcast<u32>(1i));
            }
        }
        let _e728 = uv_1;
        return _e728;
    }
    let _e729 = (*authoredUv);
    let _e731 = (*stage_1).scaleScroll;
    let _e735 = (*stage_1).scaleScroll;
    let _e737 = (*time_2);
    uv_2 = ((_e729 * _e731.xy) + (_e735.zw * _e737));
    let _e742 = (*stage_1).params[3u];
    if (_e742 > 0.5f) {
        let _e746 = (*stage_1).stretch[0u];
        let _e749 = (*stage_1).stretch[1u];
        let _e752 = (*stage_1).stretch[2u];
        let _e753 = (*time_2);
        let _e756 = (*stage_1).stretch[3u];
        wave_2 = (_e746 + (_e749 * sin(((_e752 + (_e753 * _e756)) * 6.2831855f))));
        let _e763 = uv_2;
        let _e766 = wave_2;
        uv_2 = (((_e763 - vec2(0.5f)) / vec2(max(abs(_e766), 0.0001f))) + vec2(0.5f));
    }
    let _e775 = (*stage_1).params[1u];
    if (abs(_e775) > 0.0001f) {
        let _e780 = (*stage_1).params[1u];
        let _e781 = (*time_2);
        angle = ((_e780 * _e781) * 0.017453292f);
        let _e784 = angle;
        sine_1 = sin(_e784);
        let _e786 = angle;
        cosine_1 = cos(_e786);
        let _e788 = uv_2;
        centered_1 = (_e788 - vec2(0.5f));
        let _e792 = centered_1[0u];
        let _e793 = cosine_1;
        let _e796 = centered_1[1u];
        let _e797 = sine_1;
        let _e801 = centered_1[0u];
        let _e802 = sine_1;
        let _e805 = centered_1[1u];
        let _e806 = cosine_1;
        uv_2 = (vec2<f32>(((_e792 * _e793) - (_e796 * _e797)), ((_e801 * _e802) + (_e805 * _e806))) + vec2(0.5f));
    }
    let _e814 = (*stage_1).params[2u];
    if (_e814 > 0.5f) {
        let _e818 = (*stage_1).turbulence[2u];
        let _e819 = (*time_2);
        let _e822 = (*stage_1).turbulence[3u];
        phase_3 = (_e818 + (_e819 * _e822));
        let _e827 = (*stage_1).turbulence[0u];
        let _e830 = (*stage_1).turbulence[1u];
        let _e832 = uv_2[0u];
        let _e834 = uv_2[1u];
        let _e836 = phase_3;
        offset = (_e827 + (_e830 * sin((((_e832 + _e834) + _e836) * 6.2831855f))));
        let _e842 = offset;
        let _e843 = uv_2;
        uv_2 = (_e843 + vec2(_e842));
    }
    let _e846 = uv_2;
    return _e846;
}

fn wired_sky_authored_atlas_uv_inset_u0028_vf3_u003b_vf2_u003b(direction: ptr<function, vec3<f32>>, inset: ptr<function, vec2<f32>>) -> vec2<f32> {
    var ad: vec3<f32>;
    var face: u32;
    var faceUv: vec2<f32>;
    var phi_1092_: bool;

    let _e520 = (*direction);
    ad = abs(_e520);
    let _e523 = ad[0u];
    let _e525 = ad[1u];
    let _e526 = (_e523 >= _e525);
    phi_1092_ = _e526;
    if _e526 {
        let _e528 = ad[0u];
        let _e530 = ad[2u];
        phi_1092_ = (_e528 >= _e530);
    }
    let _e533 = phi_1092_;
    if _e533 {
        let _e535 = (*direction)[0u];
        if (_e535 >= 0f) {
            face = 0u;
            let _e538 = (*direction)[1u];
            let _e541 = (*direction)[2u];
            let _e545 = ad[0u];
            faceUv = (vec2<f32>(-(_e538), -(_e541)) / vec2(_e545));
        } else {
            face = 2u;
            let _e549 = (*direction)[1u];
            let _e551 = (*direction)[2u];
            let _e555 = ad[0u];
            faceUv = (vec2<f32>(_e549, -(_e551)) / vec2(_e555));
        }
    } else {
        let _e559 = ad[1u];
        let _e561 = ad[2u];
        if (_e559 >= _e561) {
            let _e564 = (*direction)[1u];
            if (_e564 >= 0f) {
                face = 1u;
                let _e567 = (*direction)[0u];
                let _e569 = (*direction)[2u];
                let _e573 = ad[1u];
                faceUv = (vec2<f32>(_e567, -(_e569)) / vec2(_e573));
            } else {
                face = 3u;
                let _e577 = (*direction)[0u];
                let _e580 = (*direction)[2u];
                let _e584 = ad[1u];
                faceUv = (vec2<f32>(-(_e577), -(_e580)) / vec2(_e584));
            }
        } else {
            let _e588 = (*direction)[2u];
            if (_e588 >= 0f) {
                face = 4u;
                let _e591 = (*direction)[1u];
                let _e594 = (*direction)[0u];
                let _e597 = ad[2u];
                faceUv = (vec2<f32>(-(_e591), _e594) / vec2(_e597));
            } else {
                face = 5u;
                let _e601 = (*direction)[1u];
                let _e604 = (*direction)[0u];
                let _e608 = ad[2u];
                faceUv = (vec2<f32>(-(_e601), -(_e604)) / vec2(_e608));
            }
        }
    }
    let _e611 = faceUv;
    let _e615 = (*inset);
    let _e616 = (*inset);
    faceUv = clamp(((_e611 * 0.5f) + vec2(0.5f)), _e615, (vec2<f32>(1f, 1f) - _e616));
    let _e619 = faceUv;
    let _e620 = face;
    let _e623 = face;
    return ((_e619 + vec2<f32>(f32((_e620 % 3u)), f32((_e623 / 3u)))) / vec2<f32>(3f, 2f));
}

fn wired_sky_authored_atlas_uv_sized_u0028_vf3_u003b_vf2_u003b(direction_1: ptr<function, vec3<f32>>, atlasSize: ptr<function, vec2<f32>>) -> vec2<f32> {
    var param_27: vec3<f32>;
    var param_28: vec2<f32>;

    let _e519 = (*atlasSize);
    let _e522 = (*direction_1);
    param_27 = _e522;
    param_28 = (vec2<f32>(1.5f, 1f) / max(_e519, vec2<f32>(3f, 2f)));
    let _e523 = wired_sky_authored_atlas_uv_inset_u0028_vf3_u003b_vf2_u003b((&param_27), (&param_28));
    return _e523;
}

fn wired_sky_outer_sample_u0028_vf3_u003b(direction_2: ptr<function, vec3<f32>>) -> vec4<f32> {
    var atlasUv: vec2<f32>;
    var param_29: vec3<f32>;
    var param_30: vec2<f32>;

    let _e519 = textureDimensions(outerImage, 0i);
    let _e522 = (*direction_2);
    param_29 = _e522;
    param_30 = vec2<f32>(vec2<i32>(_e519));
    let _e523 = wired_sky_authored_atlas_uv_sized_u0028_vf3_u003b_vf2_u003b((&param_29), (&param_30));
    atlasUv = _e523;
    let _e524 = atlasUv;
    let _e525 = textureSampleLevel(outerImage, outerSampler, _e524, 0f);
    return _e525;
}

fn wired_sky_cloud_uv_u0028_vf3_u003b_f1_u003b(direction_3: ptr<function, vec3<f32>>, height: ptr<function, f32>) -> vec2<f32> {
    var cloudHeight: f32;
    var rayDistance: f32;
    var cloudDirection: vec3<f32>;

    let _e520 = (*height);
    cloudHeight = max(_e520, 1f);
    let _e523 = (*direction_3)[2u];
    let _e527 = (*direction_3)[2u];
    let _e529 = (*direction_3)[2u];
    let _e533 = cloudHeight;
    let _e536 = cloudHeight;
    let _e537 = cloudHeight;
    rayDistance = ((-(_e523) * 4096f) + sqrt((((((_e527 * _e529) * 4096f) * 4096f) + (8192f * _e533)) + (_e536 * _e537))));
    let _e542 = (*direction_3);
    let _e543 = rayDistance;
    cloudDirection = normalize(((_e542 * _e543) + vec3<f32>(0f, 0f, 4096f)));
    let _e547 = cloudDirection;
    return acos(clamp(_e547.xy, vec2(-1f), vec2(1f)));
}

fn main_1() {
    var direction_4: vec3<f32>;
    var cloudUv: vec2<f32>;
    var param_31: vec3<f32>;
    var param_32: f32;
    var accumulated: vec4<f32>;
    var param_33: vec3<f32>;
    var i_1: u32;
    var stage_2: MaterialStageParams;
    var imageSource: u32;
    var tcGen: u32;
    var uv_3: vec2<f32>;
    var reflected: vec3<f32>;
    var param_34: vec2<f32>;
    var param_35: MaterialStageParams;
    var param_36: f32;
    var param_37: vec3<f32>;
    var source_2: vec4<f32>;
    var local_5: vec4<f32>;
    var param_38: u32;
    var param_39: vec2<f32>;
    var vertexAlpha_1: f32;
    var param_40: vec4<f32>;
    var param_41: MaterialStageParams;
    var param_42: vec4<f32>;
    var param_43: vec4<f32>;
    var param_44: vec3<f32>;
    var param_45: f32;
    var param_46: f32;
    var rejected: bool;
    var param_47: u32;
    var param_48: vec4<f32>;
    var param_49: vec4<f32>;
    var param_50: u32;
    var param_51: vec4<f32>;
    var param_52: vec4<f32>;
    var uv_4: vec2<f32>;
    var param_53: vec3<f32>;
    var param_54: vec3<f32>;
    var param_55: f32;
    var param_56: vec3<f32>;
    var phi_2193_: bool;
    var phi_2207_: bool;
    var phi_2208_: bool;
    var phi_2222_: bool;
    var phi_2223_: bool;

    let _e555 = worldPosition_1;
    let _e557 = productDraw.atmosphereEyeDensity;
    direction_4 = normalize((_e555 - _e557.xyz));
    let _e561 = direction_4;
    param_31 = _e561;
    let _e564 = skyMaterial.timeHeight[1u];
    param_32 = _e564;
    let _e565 = wired_sky_cloud_uv_u0028_vf3_u003b_f1_u003b((&param_31), (&param_32));
    cloudUv = _e565;
    accumulated = vec4<f32>(0f, 0f, 0f, 0f);
    let _e568 = skyMaterial.meta_[1u];
    if (_e568 != 0u) {
        let _e570 = direction_4;
        param_33 = _e570;
        let _e571 = wired_sky_outer_sample_u0028_vf3_u003b((&param_33));
        accumulated = _e571;
    }
    i_1 = 0u;
    loop {
        let _e572 = i_1;
        let _e575 = skyMaterial.meta_[0u];
        if (_e572 < min(_e575, 8u)) {
            let _e578 = i_1;
            let _e581 = skyMaterial.stages[_e578];
            stage_2.meta_ = _e581.meta_;
            stage_2.generators = _e581.generators;
            stage_2.params = _e581.params;
            stage_2.scaleScroll = _e581.scaleScroll;
            stage_2.turbulence = _e581.turbulence;
            stage_2.stretch = _e581.stretch;
            stage_2.constantColor = _e581.constantColor;
            stage_2.rgbWave = _e581.rgbWave;
            stage_2.alphaWave = _e581.alphaWave;
            stage_2.tcGenVectorS = _e581.tcGenVectorS;
            stage_2.tcGenVectorT = _e581.tcGenVectorT;
            stage_2.tcModMeta = _e581.tcModMeta;
            stage_2.tcMods[0i].type_ = _e581.tcMods[0].type_;
            stage_2.tcMods[0i].p0_ = _e581.tcMods[0].p0_;
            stage_2.tcMods[0i].p1_ = _e581.tcMods[0].p1_;
            stage_2.tcMods[0i].p2_ = _e581.tcMods[0].p2_;
            stage_2.tcMods[0i].p3_ = _e581.tcMods[0].p3_;
            stage_2.tcMods[0i].p4_ = _e581.tcMods[0].p4_;
            stage_2.tcMods[0i].p5_ = _e581.tcMods[0].p5_;
            stage_2.tcMods[0i].reserved = _e581.tcMods[0].reserved;
            stage_2.tcMods[1i].type_ = _e581.tcMods[1].type_;
            stage_2.tcMods[1i].p0_ = _e581.tcMods[1].p0_;
            stage_2.tcMods[1i].p1_ = _e581.tcMods[1].p1_;
            stage_2.tcMods[1i].p2_ = _e581.tcMods[1].p2_;
            stage_2.tcMods[1i].p3_ = _e581.tcMods[1].p3_;
            stage_2.tcMods[1i].p4_ = _e581.tcMods[1].p4_;
            stage_2.tcMods[1i].p5_ = _e581.tcMods[1].p5_;
            stage_2.tcMods[1i].reserved = _e581.tcMods[1].reserved;
            stage_2.tcMods[2i].type_ = _e581.tcMods[2].type_;
            stage_2.tcMods[2i].p0_ = _e581.tcMods[2].p0_;
            stage_2.tcMods[2i].p1_ = _e581.tcMods[2].p1_;
            stage_2.tcMods[2i].p2_ = _e581.tcMods[2].p2_;
            stage_2.tcMods[2i].p3_ = _e581.tcMods[2].p3_;
            stage_2.tcMods[2i].p4_ = _e581.tcMods[2].p4_;
            stage_2.tcMods[2i].p5_ = _e581.tcMods[2].p5_;
            stage_2.tcMods[2i].reserved = _e581.tcMods[2].reserved;
            stage_2.tcMods[3i].type_ = _e581.tcMods[3].type_;
            stage_2.tcMods[3i].p0_ = _e581.tcMods[3].p0_;
            stage_2.tcMods[3i].p1_ = _e581.tcMods[3].p1_;
            stage_2.tcMods[3i].p2_ = _e581.tcMods[3].p2_;
            stage_2.tcMods[3i].p3_ = _e581.tcMods[3].p3_;
            stage_2.tcMods[3i].p4_ = _e581.tcMods[3].p4_;
            stage_2.tcMods[3i].p5_ = _e581.tcMods[3].p5_;
            stage_2.tcMods[3i].reserved = _e581.tcMods[3].reserved;
            stage_2.tcMods[4i].type_ = _e581.tcMods[4].type_;
            stage_2.tcMods[4i].p0_ = _e581.tcMods[4].p0_;
            stage_2.tcMods[4i].p1_ = _e581.tcMods[4].p1_;
            stage_2.tcMods[4i].p2_ = _e581.tcMods[4].p2_;
            stage_2.tcMods[4i].p3_ = _e581.tcMods[4].p3_;
            stage_2.tcMods[4i].p4_ = _e581.tcMods[4].p4_;
            stage_2.tcMods[4i].p5_ = _e581.tcMods[4].p5_;
            stage_2.tcMods[4i].reserved = _e581.tcMods[4].reserved;
            stage_2.tcMods[5i].type_ = _e581.tcMods[5].type_;
            stage_2.tcMods[5i].p0_ = _e581.tcMods[5].p0_;
            stage_2.tcMods[5i].p1_ = _e581.tcMods[5].p1_;
            stage_2.tcMods[5i].p2_ = _e581.tcMods[5].p2_;
            stage_2.tcMods[5i].p3_ = _e581.tcMods[5].p3_;
            stage_2.tcMods[5i].p4_ = _e581.tcMods[5].p4_;
            stage_2.tcMods[5i].p5_ = _e581.tcMods[5].p5_;
            stage_2.tcMods[5i].reserved = _e581.tcMods[5].reserved;
            stage_2.tcMods[6i].type_ = _e581.tcMods[6].type_;
            stage_2.tcMods[6i].p0_ = _e581.tcMods[6].p0_;
            stage_2.tcMods[6i].p1_ = _e581.tcMods[6].p1_;
            stage_2.tcMods[6i].p2_ = _e581.tcMods[6].p2_;
            stage_2.tcMods[6i].p3_ = _e581.tcMods[6].p3_;
            stage_2.tcMods[6i].p4_ = _e581.tcMods[6].p4_;
            stage_2.tcMods[6i].p5_ = _e581.tcMods[6].p5_;
            stage_2.tcMods[6i].reserved = _e581.tcMods[6].reserved;
            stage_2.tcMods[7i].type_ = _e581.tcMods[7].type_;
            stage_2.tcMods[7i].p0_ = _e581.tcMods[7].p0_;
            stage_2.tcMods[7i].p1_ = _e581.tcMods[7].p1_;
            stage_2.tcMods[7i].p2_ = _e581.tcMods[7].p2_;
            stage_2.tcMods[7i].p3_ = _e581.tcMods[7].p3_;
            stage_2.tcMods[7i].p4_ = _e581.tcMods[7].p4_;
            stage_2.tcMods[7i].p5_ = _e581.tcMods[7].p5_;
            stage_2.tcMods[7i].reserved = _e581.tcMods[7].reserved;
            let _e754 = stage_2.meta_[2u];
            imageSource = (_e754 & 255u);
            let _e758 = stage_2.meta_[2u];
            tcGen = (_e758 >> bitcast<u32>(8u));
            let _e761 = imageSource;
            let _e763 = tcGen;
            let _e766 = lightmapCoord_1;
            let _e767 = cloudUv;
            uv_3 = select(_e767, _e766, vec2(((_e761 == 1u) || (_e763 == 1u))));
            let _e770 = tcGen;
            if (_e770 == 2u) {
                let _e772 = direction_4;
                let _e773 = worldNormal_1;
                reflected = reflect(_e772, normalize(_e773));
                let _e777 = reflected[1u];
                let _e781 = reflected[2u];
                uv_3 = vec2<f32>((0.5f + (_e777 * 0.5f)), (0.5f - (_e781 * 0.5f)));
            } else {
                let _e785 = tcGen;
                if (_e785 == 3u) {
                    let _e787 = worldPosition_1;
                    let _e789 = stage_2.tcGenVectorS;
                    let _e792 = worldPosition_1;
                    let _e794 = stage_2.tcGenVectorT;
                    uv_3 = vec2<f32>(dot(_e787, _e789.xyz), dot(_e792, _e794.xyz));
                }
            }
            let _e798 = uv_3;
            param_34 = _e798;
            let _e799 = stage_2;
            param_35 = _e799;
            let _e802 = skyMaterial.timeHeight[0u];
            param_36 = _e802;
            let _e803 = worldPosition_1;
            param_37 = _e803;
            let _e804 = wired_material_stage_uv_u0028_vf2_u003b_struct_u002d_MaterialStageParams_u002d_vu4_u002d_vu4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vu4_u002d_struct_u002d_MaterialTcMod_u002d_u1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_u11_u005b_8_u005d_1_u003b_f1_u003b_vf3_u003b((&param_34), (&param_35), (&param_36), (&param_37));
            uv_3 = _e804;
            let _e805 = imageSource;
            if (_e805 == 2u) {
                local_5 = vec4<f32>(1f, 1f, 1f, 1f);
            } else {
                let _e807 = i_1;
                param_38 = _e807;
                let _e808 = uv_3;
                param_39 = _e808;
                let _e809 = wired_sky_stage_sample_u0028_u1_u003b_vf2_u003b((&param_38), (&param_39));
                local_5 = _e809;
            }
            let _e810 = local_5;
            source_2 = _e810;
            let _e812 = color_2[3u];
            vertexAlpha_1 = _e812;
            let _e815 = stage_2.generators[1u];
            if (_e815 == 8u) {
                let _e817 = worldPosition_1;
                let _e819 = productDraw.atmosphereEyeDensity;
                let _e825 = stage_2.tcGenVectorS[3u];
                vertexAlpha_1 = (floor((clamp((length((_e817 - _e819.xyz)) / max(_e825, 0.001f)), 0f, 1f) * 255f)) / 255f);
            }
            let _e832 = source_2;
            param_40 = _e832;
            let _e833 = stage_2;
            param_41 = _e833;
            let _e834 = color_2;
            param_42 = _e834;
            param_43 = vec4<f32>(1f, 1f, 1f, 1f);
            param_44 = vec3<f32>(1f, 1f, 1f);
            let _e835 = vertexAlpha_1;
            param_45 = _e835;
            let _e838 = skyMaterial.timeHeight[0u];
            param_46 = _e838;
            let _e839 = wired_material_stage_generators_u0028_vf4_u003b_struct_u002d_MaterialStageParams_u002d_vu4_u002d_vu4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vu4_u002d_struct_u002d_MaterialTcMod_u002d_u1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_u11_u005b_8_u005d_1_u003b_vf4_u003b_vf4_u003b_vf3_u003b_f1_u003b_f1_u003b((&param_40), (&param_41), (&param_42), (&param_43), (&param_44), (&param_45), (&param_46));
            source_2 = _e839;
            let _e842 = stage_2.meta_[3u];
            let _e843 = (_e842 == 1u);
            phi_2193_ = _e843;
            if _e843 {
                let _e845 = source_2[3u];
                phi_2193_ = (_e845 == 0f);
            }
            let _e848 = phi_2193_;
            phi_2208_ = _e848;
            if !(_e848) {
                let _e852 = stage_2.meta_[3u];
                let _e853 = (_e852 == 2u);
                phi_2207_ = _e853;
                if _e853 {
                    let _e855 = source_2[3u];
                    let _e858 = stage_2.params[0u];
                    phi_2207_ = (_e855 >= _e858);
                }
                let _e861 = phi_2207_;
                phi_2208_ = _e861;
            }
            let _e863 = phi_2208_;
            phi_2223_ = _e863;
            if !(_e863) {
                let _e867 = stage_2.meta_[3u];
                let _e868 = (_e867 == 3u);
                phi_2222_ = _e868;
                if _e868 {
                    let _e870 = source_2[3u];
                    let _e873 = stage_2.params[0u];
                    phi_2222_ = (_e870 < _e873);
                }
                let _e876 = phi_2222_;
                phi_2223_ = _e876;
            }
            let _e878 = phi_2223_;
            rejected = _e878;
            let _e879 = rejected;
            if !(_e879) {
                let _e881 = source_2;
                let _e884 = stage_2.meta_[0u];
                param_47 = _e884;
                let _e885 = source_2;
                param_48 = _e885;
                let _e886 = accumulated;
                param_49 = _e886;
                let _e887 = wired_material_blend_factor_u0028_u1_u003b_vf4_u003b_vf4_u003b((&param_47), (&param_48), (&param_49));
                let _e889 = accumulated;
                let _e892 = stage_2.meta_[1u];
                param_50 = _e892;
                let _e893 = source_2;
                param_51 = _e893;
                let _e894 = accumulated;
                param_52 = _e894;
                let _e895 = wired_material_blend_factor_u0028_u1_u003b_vf4_u003b_vf4_u003b((&param_50), (&param_51), (&param_52));
                accumulated = ((_e881 * _e887) + (_e889 * _e895));
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e898 = i_1;
            i_1 = (_e898 + bitcast<u32>(1i));
        }
    }
    let _e903 = productDraw.atmosphereHeightSky[2u];
    if (_e903 > 0f) {
        let _e906 = direction_4[1u];
        let _e908 = direction_4[0u];
        let _e913 = direction_4[2u];
        uv_4 = vec2<f32>(((atan2(_e906, _e908) / 6.2831855f) + 0.5f), ((asin(clamp(_e913, -1f, 1f)) / 3.1415927f) + 0.5f));
        let _e919 = uv_4;
        let _e920 = textureSample(physicalImage, physicalSampler, _e919);
        let _e923 = productDraw.atmosphereHeightSky[2u];
        let _e925 = accumulated;
        param_53 = _e925.xyz;
        param_54 = _e920.xyz;
        param_55 = clamp(_e923, 0f, 1f);
        let _e928 = wired_compose_authored_physical_sky_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_53), (&param_54), (&param_55));
        accumulated[0u] = _e928.x;
        accumulated[1u] = _e928.y;
        accumulated[2u] = _e928.z;
    }
    let _e935 = accumulated;
    param_56 = _e935.xyz;
    let _e937 = wired_display_visibility_u0028_vf3_u003b((&param_56));
    outColor = vec4<f32>(_e937.x, _e937.y, _e937.z, 1f);
    return;
}

@fragment
fn main(@location(3) worldPosition: vec3<f32>, @location(1) lightmapCoord: vec2<f32>, @location(4) worldNormal: vec3<f32>, @location(2) color: vec4<f32>, @location(0) texCoord: vec2<f32>) -> @location(0) vec4<f32> {
    worldPosition_1 = worldPosition;
    lightmapCoord_1 = lightmapCoord;
    worldNormal_1 = worldNormal;
    color_2 = color;
    texCoord_1 = texCoord;
    main_1();
    let _e11 = outColor;
    return _e11;
}
