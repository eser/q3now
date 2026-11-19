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

@group(0) @binding(9)
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
@group(0) @binding(0)
var outerRt: texture_2d<f32>;
@group(0) @binding(6)
var outerSampler: sampler;
@group(0) @binding(1)
var outerBk: texture_2d<f32>;
@group(0) @binding(2)
var outerLf: texture_2d<f32>;
@group(0) @binding(3)
var outerFt: texture_2d<f32>;
@group(0) @binding(4)
var outerUp: texture_2d<f32>;
@group(0) @binding(5)
var outerDn: texture_2d<f32>;
var<private> worldPosition_1: vec3<f32>;
@group(2) @binding(1)
var<uniform> skyMaterial: SkyMaterial;
var<private> lightmapCoord_1: vec2<f32>;
var<private> worldNormal_1: vec3<f32>;
var<private> color_2: vec4<f32>;
@group(0) @binding(7)
var physicalImage: texture_2d<f32>;
@group(0) @binding(8)
var physicalSampler: sampler;
var<private> outColor: vec4<f32>;
var<private> texCoord_1: vec2<f32>;

fn wired_display_visibility_u0028_vf3_u003b(sceneColor: ptr<function, vec3<f32>>) -> vec3<f32> {
    var exposed: vec3<f32>;
    var luminance: f32;
    var curved: f32;
    var phi_1643_: bool;
    var phi_1649_: bool;

    let _e523 = (*sceneColor);
    let _e527 = productDraw.viewForward[3u];
    exposed = (max(_e523, vec3<f32>(0f, 0f, 0f)) * _e527);
    let _e529 = exposed;
    luminance = dot(_e529, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e531 = luminance;
    let _e532 = (_e531 > 0f);
    phi_1643_ = _e532;
    if _e532 {
        let _e533 = luminance;
        let _e536 = productDraw.viewUp[3u];
        phi_1643_ = (_e533 < _e536);
    }
    let _e539 = phi_1643_;
    phi_1649_ = _e539;
    if _e539 {
        let _e542 = productDraw.viewLeft[3u];
        phi_1649_ = (_e542 != 1f);
    }
    let _e545 = phi_1649_;
    if _e545 {
        let _e546 = luminance;
        let _e549 = productDraw.viewUp[3u];
        let _e553 = productDraw.viewLeft[3u];
        let _e557 = productDraw.viewUp[3u];
        curved = (pow((_e546 / _e549), _e553) * _e557);
        let _e559 = curved;
        let _e560 = luminance;
        let _e562 = exposed;
        exposed = (_e562 * (_e559 / _e560));
    }
    let _e564 = exposed;
    return _e564;
}

fn wired_compose_authored_physical_sky_u0028_vf3_u003b_vf3_u003b_f1_u003b(authored: ptr<function, vec3<f32>>, physical: ptr<function, vec3<f32>>, blend: ptr<function, f32>) -> vec3<f32> {
    var authoredEnergy: f32;
    var physicalEnergy: f32;
    var scale: f32;
    var physicalTint: vec3<f32>;
    var local: vec3<f32>;
    var relit: vec3<f32>;

    let _e528 = (*authored);
    authoredEnergy = dot(max(_e528, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e531 = (*physical);
    physicalEnergy = dot(max(_e531, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e534 = authoredEnergy;
    if (_e534 <= 0.00001f) {
        let _e536 = (*authored);
        let _e537 = (*physical);
        let _e538 = (*blend);
        return mix(_e536, _e537, vec3(_e538));
    }
    let _e541 = physicalEnergy;
    let _e542 = authoredEnergy;
    scale = clamp((_e541 / max(_e542, 0.18f)), 0.65f, 1.75f);
    let _e546 = physicalEnergy;
    if (_e546 > 0.00001f) {
        let _e548 = (*physical);
        let _e549 = physicalEnergy;
        local = (_e548 / vec3(_e549));
    } else {
        local = vec3<f32>(1f, 1f, 1f);
    }
    let _e552 = local;
    physicalTint = _e552;
    let _e553 = (*authored);
    let _e554 = scale;
    let _e556 = physicalTint;
    let _e561 = (*physical);
    relit = (((_e553 * _e554) * mix(vec3<f32>(1f, 1f, 1f), clamp(_e556, vec3<f32>(0.5f, 0.5f, 0.5f), vec3<f32>(2f, 2f, 2f)), vec3(0.12f))) + (_e561 * 0.08f));
    let _e564 = (*authored);
    let _e565 = relit;
    let _e566 = (*blend);
    return mix(_e564, _e565, vec3(_e566));
}

fn wired_material_blend_factor_u0028_u1_u003b_vf4_u003b_vf4_u003b(factor: ptr<function, u32>, source: ptr<function, vec4<f32>>, destination: ptr<function, vec4<f32>>) -> vec4<f32> {
    let _e522 = (*factor);
    if (_e522 == 0u) {
        return vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e524 = (*factor);
    if (_e524 == 1u) {
        return vec4<f32>(1f, 1f, 1f, 1f);
    }
    let _e526 = (*factor);
    if (_e526 == 2u) {
        let _e528 = (*source);
        return _e528;
    }
    let _e529 = (*factor);
    if (_e529 == 3u) {
        let _e531 = (*source);
        return (vec4(1f) - _e531);
    }
    let _e534 = (*factor);
    if (_e534 == 4u) {
        let _e536 = (*destination);
        return _e536;
    }
    let _e537 = (*factor);
    if (_e537 == 5u) {
        let _e539 = (*destination);
        return (vec4(1f) - _e539);
    }
    let _e542 = (*factor);
    if (_e542 == 6u) {
        let _e545 = (*source)[3u];
        return vec4(_e545);
    }
    let _e547 = (*factor);
    if (_e547 == 7u) {
        let _e550 = (*source)[3u];
        return vec4((1f - _e550));
    }
    let _e553 = (*factor);
    if (_e553 == 8u) {
        let _e556 = (*destination)[3u];
        return vec4(_e556);
    }
    let _e558 = (*factor);
    if (_e558 == 9u) {
        let _e561 = (*destination)[3u];
        return vec4((1f - _e561));
    }
    let _e565 = (*source)[3u];
    let _e567 = (*destination)[3u];
    return vec4(min(_e565, (1f - _e567)));
}

fn wired_stage_srgb_to_linear_u0028_vf3_u003b(value: ptr<function, vec3<f32>>) -> vec3<f32> {
    var low: vec3<f32>;
    var high: vec3<f32>;

    let _e522 = (*value);
    (*value) = max(_e522, vec3<f32>(0f, 0f, 0f));
    let _e524 = (*value);
    low = (_e524 / vec3(12.92f));
    let _e527 = (*value);
    high = pow(((_e527 + vec3(0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e533 = high;
    let _e534 = low;
    let _e535 = (*value);
    return select(_e533, _e534, (_e535 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f)));
}

fn wired_material_table_sine_u0028_u1_u003b(index: ptr<function, u32>) -> f32 {
    let _e520 = (*index);
    return sin(((f32((_e520 & 1023u)) * 0.0061359233f) + 0.0001f));
}

fn wired_stage_remainder_u0028_f1_u003b_f1_u003b(x: ptr<function, f32>, y: ptr<function, f32>) -> f32 {
    let _e521 = (*x);
    let _e522 = (*x);
    let _e523 = (*y);
    let _e526 = (*y);
    return (_e521 - (trunc((_e522 / _e523)) * _e526));
}

fn wired_material_table_index_u0028_f1_u003b(phase: ptr<function, f32>) -> u32 {
    var param: f32;
    var param_1: f32;

    let _e522 = (*phase);
    param = _e522;
    param_1 = 1f;
    let _e523 = wired_stage_remainder_u0028_f1_u003b_f1_u003b((&param), (&param_1));
    return (bitcast<u32>(i32((_e523 * 1024f))) & 1023u);
}

fn wired_material_noise_sample_u0028_u1_u003b(cell: ptr<function, u32>) -> f32 {
    var index_1: u32;
    var axis: u32;
    var indexable: array<u32, 256>;
    var indexable_1: array<f32, 256>;

    let _e524 = (*cell);
    index_1 = (_e524 & 255u);
    axis = 0u;
    loop {
        let _e526 = axis;
        if (_e526 < 4u) {
            let _e528 = index_1;
            indexable = array<u32, 256>(18u, 42u, 224u, 18u, 127u, 202u, 208u, 24u, 70u, 254u, 54u, 61u, 200u, 211u, 205u, 144u, 254u, 100u, 134u, 97u, 96u, 130u, 241u, 133u, 221u, 203u, 90u, 77u, 213u, 58u, 234u, 220u, 0u, 158u, 246u, 141u, 157u, 3u, 238u, 146u, 168u, 162u, 43u, 113u, 73u, 133u, 254u, 103u, 137u, 108u, 231u, 93u, 14u, 104u, 164u, 75u, 225u, 179u, 249u, 27u, 131u, 250u, 17u, 93u, 101u, 177u, 30u, 133u, 92u, 155u, 93u, 97u, 253u, 2u, 131u, 9u, 70u, 98u, 208u, 22u, 63u, 231u, 183u, 196u, 206u, 251u, 169u, 33u, 209u, 18u, 13u, 170u, 240u, 196u, 181u, 236u, 93u, 149u, 75u, 253u, 126u, 14u, 125u, 250u, 195u, 117u, 124u, 15u, 248u, 143u, 153u, 77u, 142u, 193u, 14u, 204u, 142u, 42u, 250u, 246u, 7u, 17u, 183u, 101u, 210u, 38u, 134u, 52u, 38u, 76u, 172u, 16u, 127u, 119u, 94u, 61u, 160u, 103u, 11u, 134u, 240u, 122u, 247u, 105u, 47u, 120u, 125u, 36u, 112u, 9u, 99u, 101u, 206u, 228u, 212u, 1u, 55u, 170u, 41u, 16u, 217u, 127u, 134u, 51u, 114u, 170u, 36u, 202u, 133u, 185u, 143u, 195u, 100u, 18u, 124u, 76u, 131u, 73u, 17u, 189u, 195u, 61u, 38u, 140u, 245u, 123u, 135u, 255u, 191u, 241u, 205u, 244u, 111u, 129u, 227u, 23u, 90u, 200u, 136u, 9u, 87u, 102u, 183u, 173u, 22u, 253u, 147u, 248u, 0u, 55u, 241u, 95u, 57u, 69u, 221u, 111u, 175u, 80u, 227u, 117u, 47u, 120u, 20u, 165u, 15u, 200u, 191u, 192u, 182u, 62u, 28u, 42u, 208u, 122u, 132u, 249u, 58u, 236u, 143u, 112u, 3u, 250u, 146u, 57u, 127u, 74u, 34u, 215u, 148u, 148u, 86u, 223u, 57u, 243u, 124u, 5u);
            let _e530 = indexable[_e528];
            index_1 = _e530;
            continue;
        } else {
            break;
        }
        continuing {
            let _e531 = axis;
            axis = (_e531 + bitcast<u32>(1i));
        }
    }
    let _e534 = index_1;
    indexable_1 = array<f32, 256>(0.8469031f, 0.47616637f, 0.21855819f, -0.39615327f, 0.39833248f, -0.23092157f, 0.016733766f, 0.32460082f, 0.8264301f, 0.7141863f, 0.54239666f, -0.25759047f, 0.37415636f, -0.31669623f, -0.29938978f, -0.68850935f, 0.29332948f, -0.04988092f, 0.71274126f, -0.25778574f, 0.103577495f, -0.41324478f, 0.85156703f, -0.044953406f, -0.2957458f, 0.16786718f, 0.15998745f, -0.86378145f, 0.053961515f, 0.66703904f, 0.9344952f, 0.2743982f, 0.88002634f, 0.9776896f, 0.994329f, 0.12602079f, 0.43463063f, -0.042842925f, 0.9703553f, -0.61084926f, -0.77226996f, -0.6985575f, 0.8035594f, -0.26996213f, -0.572654f, 0.6617917f, -0.5263469f, 0.5106157f, 0.6724851f, 0.20376396f, -0.897998f, -0.35112864f, -0.6254122f, 0.21080363f, 0.40637946f, -0.12594455f, 0.7685201f, 0.0058778524f, 0.10742557f, -0.3571027f, 0.7327f, -0.13852674f, -0.49482614f, -0.8643447f, 0.3038231f, -0.23077399f, 0.6168364f, 0.21628118f, 0.93993735f, -0.35099918f, -0.15524966f, -0.021139681f, 0.09912431f, -0.4076547f, 0.38609362f, 0.25821447f, 0.046231627f, 0.8098897f, -0.37749976f, 0.9888071f, -0.6303477f, -0.26814288f, -0.31183988f, 0.15625179f, -0.7796062f, 0.09842801f, -0.05072981f, 0.582808f, 0.81344366f, 0.14925206f, -0.9560602f, 0.61637414f, -0.092731535f, -0.6954373f, 0.8092247f, 0.5324365f, -0.6154182f, -0.29161483f, -0.70310366f, 0.21504116f, -0.7022008f, 0.9377383f, 0.97705483f, 0.03877473f, -0.69478965f, 0.38984442f, 0.6148282f, -0.57611346f, 0.91212595f, 0.74049485f, 0.7025621f, 0.3395071f, 0.74446225f, 0.94713306f, -0.67959726f, 0.120883346f, 0.16995502f, -0.5482242f, 0.97989154f, -0.48022193f, -0.37604982f, 0.7015846f, -0.32008308f, -0.41527098f, 0.6949965f, 0.99238575f, -0.46843714f, -0.92288625f, 0.9688833f, 0.15485096f, 0.10692072f, -0.35389692f, 0.0017485619f, -0.09468871f, 0.5620551f, -0.8520483f, -0.28308982f, -0.49305362f, -0.6841395f, -0.4818645f, -0.7125609f, 0.13562608f, 0.12707162f, -0.32404405f, -0.9284431f, 0.8934244f, -0.564576f, 0.8527281f, -0.25694007f, -0.97056794f, 0.47224367f, 0.2523452f, 0.6429974f, -0.28594452f, 0.6529108f, -0.40799206f, 0.75336504f, -0.9466928f, -0.84182966f, -0.26508647f, 0.5766138f, 0.60028374f, 0.51994145f, -0.23579544f, -0.08374125f, -0.33356935f, -0.30988055f, 0.5323293f, -0.996716f, -0.55436146f, 0.2665515f, -0.9786258f, 0.50795853f, 0.7976849f, -0.10953015f, -0.44617242f, 0.8404442f, 0.60429657f, -0.07405478f, 0.7842498f, 0.698086f, -0.37783688f, -0.00074344873f, -0.5424825f, -0.6844158f, 0.9215293f, -0.37232345f, 0.14719069f, -0.74398446f, 0.21657062f, -0.12075871f, 0.88769686f, -0.32179815f, -0.028252065f, 0.63584507f, 0.70286345f, -0.08597416f, 0.65325916f, -0.149723f, 0.5486723f, -0.08077663f, -0.8520131f, 0.6952628f, 0.74621975f, -0.9170834f, 0.4172858f, 0.95932746f, 0.2251041f, -0.59962666f, 0.5296767f, -0.60249746f, 0.7849034f, -0.9684094f, -0.8301374f, 0.3747933f, -0.73390484f, -0.71786344f, -0.71185696f, 0.4871862f, 0.45765078f, -0.94725704f, -0.25358766f, 0.55516243f, -0.54417217f, -0.4209221f, 0.29355347f, 0.90264106f, 0.069580674f, -0.31801087f, -0.7033422f, 0.6022372f, 0.9443332f, 0.43932247f, -0.7470752f, -0.86122906f, -0.14853984f, -0.2159577f, 0.28032398f, -0.66258276f, 0.92431784f, 0.821825f, 0.8368082f, 0.0973649f, -0.13240129f, -0.8209977f, -0.90698063f, 0.9713893f, -0.2620738f, 0.8247925f, -0.5282723f, 0.14774466f, 0.07994926f, 0.63386023f, -0.48508543f, 0.43871665f, 0.6949941f);
    let _e536 = indexable_1[_e534];
    return _e536;
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

    let _e540 = (*function_);
    if (_e540 == 6u) {
        let _e542 = (*time);
        let _e544 = (*wave)[2u];
        let _e547 = (*wave)[3u];
        position = ((_e542 + _e544) * _e547);
        let _e549 = position;
        param_2 = _e549;
        param_3 = 256f;
        let _e550 = wired_stage_remainder_u0028_f1_u003b_f1_u003b((&param_2), (&param_3));
        wrapped = _e550;
        let _e551 = wrapped;
        cell_1 = (bitcast<u32>(i32(floor(_e551))) & 255u);
        let _e556 = cell_1;
        param_4 = _e556;
        let _e557 = wired_material_noise_sample_u0028_u1_u003b((&param_4));
        a = _e557;
        let _e558 = cell_1;
        param_5 = (_e558 + 1u);
        let _e560 = wired_material_noise_sample_u0028_u1_u003b((&param_5));
        b = _e560;
        let _e562 = (*wave)[0u];
        let _e564 = (*wave)[1u];
        let _e565 = a;
        let _e566 = b;
        let _e567 = a;
        let _e569 = wrapped;
        return (_e562 + (_e564 * (_e565 + ((_e566 - _e567) * fract(_e569)))));
    }
    let _e576 = (*wave)[2u];
    let _e577 = (*time);
    let _e579 = (*wave)[3u];
    phase_1 = (_e576 + (_e577 * _e579));
    let _e582 = phase_1;
    param_6 = _e582;
    let _e583 = wired_material_table_index_u0028_f1_u003b((&param_6));
    index_2 = _e583;
    let _e584 = index_2;
    cycle = (f32(_e584) / 1024f);
    let _e587 = (*function_);
    if (_e587 == 1u) {
        let _e589 = index_2;
        param_7 = _e589;
        let _e590 = wired_material_table_sine_u0028_u1_u003b((&param_7));
        value_1 = _e590;
    } else {
        let _e591 = (*function_);
        if (_e591 == 2u) {
            let _e593 = index_2;
            value_1 = select(-1f, 1f, (_e593 < 512u));
        } else {
            let _e596 = (*function_);
            if (_e596 == 3u) {
                let _e598 = index_2;
                halfIndex = (_e598 & 511u);
                let _e600 = halfIndex;
                quarterIndex = (_e600 & 255u);
                let _e602 = quarterIndex;
                if (_e602 == 0u) {
                    local_1 = 0.000001f;
                } else {
                    let _e604 = quarterIndex;
                    local_1 = (f32(_e604) / 256f);
                }
                let _e607 = local_1;
                value_1 = _e607;
                let _e608 = halfIndex;
                if (_e608 >= 256u) {
                    let _e610 = value_1;
                    value_1 = (1f - _e610);
                }
                let _e612 = index_2;
                if (_e612 >= 512u) {
                    let _e614 = value_1;
                    value_1 = -(_e614);
                }
            } else {
                let _e616 = (*function_);
                if (_e616 == 4u) {
                    let _e618 = index_2;
                    let _e620 = cycle;
                    value_1 = select(_e620, 0.000001f, (_e618 == 0u));
                } else {
                    let _e622 = (*function_);
                    if (_e622 == 5u) {
                        let _e624 = index_2;
                        let _e626 = cycle;
                        value_1 = (1f - select(_e626, 0.000001f, (_e624 == 0u)));
                    } else {
                        value_1 = 0f;
                    }
                }
            }
        }
    }
    let _e630 = (*wave)[0u];
    let _e632 = (*wave)[1u];
    let _e633 = value_1;
    return (_e630 + (_e632 * _e633));
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

    let _e543 = (*stage).generators[0u];
    rgbGen = _e543;
    let _e546 = (*stage).generators[1u];
    alphaGen = _e546;
    rgb = vec3<f32>(1f, 1f, 1f);
    alpha = 1f;
    let _e547 = rgbGen;
    let _e549 = rgbGen;
    if ((_e547 == 3u) || (_e549 == 4u)) {
        let _e552 = rgbGen;
        if (_e552 == 3u) {
            let _e554 = (*entityColor);
            local_2 = _e554.xyz;
        } else {
            let _e556 = (*entityColor);
            local_2 = (vec3(1f) - _e556.xyz);
        }
        let _e560 = local_2;
        rgb = _e560;
        let _e561 = rgbGen;
        if (_e561 == 3u) {
            let _e564 = (*entityColor)[3u];
            local_3 = _e564;
        } else {
            let _e566 = (*entityColor)[3u];
            local_3 = (1f - _e566);
        }
        let _e568 = local_3;
        alpha = _e568;
    } else {
        let _e569 = rgbGen;
        let _e571 = rgbGen;
        if ((_e569 == 5u) || (_e571 == 6u)) {
            let _e574 = (*vertexColor);
            rgb = _e574.xyz;
            let _e577 = (*vertexColor)[3u];
            alpha = _e577;
        } else {
            let _e578 = rgbGen;
            if (_e578 == 7u) {
                let _e580 = (*vertexColor);
                rgb = (vec3(1f) - _e580.xyz);
            } else {
                let _e584 = rgbGen;
                if (_e584 == 8u) {
                    let _e588 = (*stage).generators[2u];
                    param_8 = _e588;
                    let _e590 = (*stage).rgbWave;
                    param_9 = _e590;
                    let _e591 = (*time_1);
                    param_10 = _e591;
                    let _e592 = wired_material_wave_u0028_u1_u003b_vf4_u003b_f1_u003b((&param_8), (&param_9), (&param_10));
                    rgb = vec3((floor((clamp(_e592, 0f, 1f) * 255f)) / 255f));
                } else {
                    let _e598 = rgbGen;
                    if (_e598 == 9u) {
                        let _e600 = (*diffuseLighting);
                        rgb = _e600;
                    } else {
                        let _e601 = rgbGen;
                        if (_e601 == 11u) {
                            let _e604 = (*stage).constantColor;
                            color_1 = (floor((clamp(_e604, vec4(0f), vec4(1f)) * 255f)) / vec4(255f));
                            let _e612 = color_1;
                            rgb = _e612.xyz;
                            let _e615 = color_1[3u];
                            alpha = _e615;
                        }
                    }
                }
            }
        }
    }
    let _e616 = rgbGen;
    if (_e616 != 9u) {
        let _e618 = rgb;
        param_11 = _e618;
        let _e619 = wired_stage_srgb_to_linear_u0028_vf3_u003b((&param_11));
        rgb = _e619;
    }
    let _e620 = alphaGen;
    let _e622 = rgbGen;
    if ((_e620 == 0u) && (_e622 != 6u)) {
        alpha = 1f;
    } else {
        let _e625 = alphaGen;
        let _e627 = alphaGen;
        if ((_e625 == 2u) || (_e627 == 3u)) {
            let _e630 = alphaGen;
            if (_e630 == 2u) {
                let _e633 = (*entityColor)[3u];
                local_4 = _e633;
            } else {
                let _e635 = (*entityColor)[3u];
                local_4 = (1f - _e635);
            }
            let _e637 = local_4;
            alpha = _e637;
        } else {
            let _e638 = alphaGen;
            if (_e638 == 4u) {
                let _e641 = (*vertexColor)[3u];
                alpha = _e641;
            } else {
                let _e642 = alphaGen;
                if (_e642 == 5u) {
                    let _e645 = (*vertexColor)[3u];
                    alpha = (1f - _e645);
                } else {
                    let _e647 = alphaGen;
                    if (_e647 == 7u) {
                        let _e651 = (*stage).generators[3u];
                        param_12 = _e651;
                        let _e653 = (*stage).alphaWave;
                        param_13 = _e653;
                        let _e654 = (*time_1);
                        param_14 = _e654;
                        let _e655 = wired_material_wave_u0028_u1_u003b_vf4_u003b_f1_u003b((&param_12), (&param_13), (&param_14));
                        alpha = (floor((clamp(_e655, 0f, 1f) * 255f)) / 255f);
                    } else {
                        let _e660 = alphaGen;
                        let _e662 = alphaGen;
                        if ((_e660 == 6u) || (_e662 == 8u)) {
                            let _e665 = (*vertexAlpha);
                            alpha = _e665;
                        } else {
                            let _e666 = alphaGen;
                            if (_e666 == 9u) {
                                let _e670 = (*stage).constantColor[3u];
                                alpha = (floor((clamp(_e670, 0f, 1f) * 255f)) / 255f);
                            }
                        }
                    }
                }
            }
        }
    }
    let _e675 = rgb;
    let _e676 = (*source_1);
    let _e678 = (_e676.xyz * _e675);
    (*source_1)[0u] = _e678.x;
    (*source_1)[1u] = _e678.y;
    (*source_1)[2u] = _e678.z;
    let _e685 = alpha;
    let _e687 = (*source_1)[3u];
    (*source_1)[3u] = (_e687 * _e685);
    let _e690 = (*source_1);
    return _e690;
}

fn wired_sky_stage_sample_u0028_u1_u003b_vf2_u003b(slot: ptr<function, u32>, uv: ptr<function, vec2<f32>>) -> vec4<f32> {
    let _e521 = (*slot);
    switch bitcast<i32>(_e521) {
        case 0: {
            let _e525 = (*uv);
            let _e526 = textureSample(stageImage0_, stageSampler0_, _e525);
            return _e526;
        }
        case 1: {
            let _e527 = (*uv);
            let _e528 = textureSample(stageImage1_, stageSampler1_, _e527);
            return _e528;
        }
        case 2: {
            let _e529 = (*uv);
            let _e530 = textureSample(stageImage2_, stageSampler2_, _e529);
            return _e530;
        }
        case 3: {
            let _e531 = (*uv);
            let _e532 = textureSample(stageImage3_, stageSampler3_, _e531);
            return _e532;
        }
        case 4: {
            let _e533 = (*uv);
            let _e534 = textureSample(stageImage4_, stageSampler4_, _e533);
            return _e534;
        }
        case 5: {
            let _e535 = (*uv);
            let _e536 = textureSample(stageImage5_, stageSampler5_, _e535);
            return _e536;
        }
        case 6: {
            let _e537 = (*uv);
            let _e538 = textureSample(stageImage6_, stageSampler6_, _e537);
            return _e538;
        }
        default: {
            let _e523 = (*uv);
            let _e524 = textureSample(stageImage7_, stageSampler7_, _e523);
            return _e524;
        }
    }
}

fn wired_stage_copy_sign_u0028_f1_u003b_f1_u003b(magnitude: ptr<function, f32>, signSource: ptr<function, f32>) -> f32 {
    let _e521 = (*magnitude);
    let _e524 = (*signSource);
    return bitcast<f32>(((bitcast<u32>(_e521) & 2147483647u) | (bitcast<u32>(_e524) & 2147483648u)));
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

    let _e555 = (*stage_1).tcModMeta[0u];
    if (_e555 > 0u) {
        let _e557 = (*authoredUv);
        uv_1 = _e557;
        i = 0u;
        loop {
            let _e558 = i;
            let _e561 = (*stage_1).tcModMeta[0u];
            if (_e558 < min(_e561, 8u)) {
                let _e564 = i;
                let _e567 = (*stage_1).tcMods[_e564];
                op = _e567;
                let _e569 = op.type_;
                if (_e569 == 1u) {
                    let _e572 = op.p0_;
                    let _e574 = op.p1_;
                    let _e576 = uv_1;
                    uv_1 = (_e576 * vec2<f32>(_e572, _e574));
                } else {
                    let _e579 = op.type_;
                    if (_e579 == 2u) {
                        let _e582 = op.p0_;
                        let _e584 = op.p1_;
                        let _e586 = (*time_2);
                        let _e589 = uv_1;
                        uv_1 = (_e589 + fract((vec2<f32>(_e582, _e584) * _e586)));
                    } else {
                        let _e592 = op.type_;
                        if (_e592 == 3u) {
                            let _e595 = op.p0_;
                            let _e597 = (*time_2);
                            param_15 = ((-(_e595) * _e597) / 360f);
                            let _e600 = wired_material_table_index_u0028_f1_u003b((&param_15));
                            index_3 = _e600;
                            let _e601 = index_3;
                            param_16 = _e601;
                            let _e602 = wired_material_table_sine_u0028_u1_u003b((&param_16));
                            sine = _e602;
                            let _e603 = index_3;
                            param_17 = (_e603 + 256u);
                            let _e605 = wired_material_table_sine_u0028_u1_u003b((&param_17));
                            cosine = _e605;
                            let _e606 = uv_1;
                            centered = (_e606 - vec2(0.5f));
                            let _e610 = centered[0u];
                            let _e611 = cosine;
                            let _e614 = centered[1u];
                            let _e615 = sine;
                            let _e619 = centered[0u];
                            let _e620 = sine;
                            let _e623 = centered[1u];
                            let _e624 = cosine;
                            uv_1 = (vec2<f32>(((_e610 * _e611) - (_e614 * _e615)), ((_e619 * _e620) + (_e623 * _e624))) + vec2(0.5f));
                        } else {
                            let _e631 = op.type_;
                            if (_e631 == 4u) {
                                let _e634 = op.p2_;
                                let _e635 = (*time_2);
                                let _e637 = op.p3_;
                                phase_2 = (_e634 + (_e635 * _e637));
                                let _e641 = (*position_1)[0u];
                                let _e643 = (*position_1)[2u];
                                let _e646 = (*position_1)[1u];
                                let _e650 = phase_2;
                                coordinate = ((vec2<f32>((_e641 + _e643), _e646) / vec2(1024f)) + vec2(_e650));
                                let _e654 = coordinate[0u];
                                param_18 = _e654;
                                let _e655 = wired_material_table_index_u0028_f1_u003b((&param_18));
                                param_19 = _e655;
                                let _e656 = wired_material_table_sine_u0028_u1_u003b((&param_19));
                                let _e658 = coordinate[1u];
                                param_20 = _e658;
                                let _e659 = wired_material_table_index_u0028_f1_u003b((&param_20));
                                param_21 = _e659;
                                let _e660 = wired_material_table_sine_u0028_u1_u003b((&param_21));
                                let _e663 = op.p1_;
                                let _e665 = uv_1;
                                uv_1 = (_e665 + (vec2<f32>(_e656, _e660) * _e663));
                            } else {
                                let _e668 = op.type_;
                                if (_e668 == 5u) {
                                    let _e671 = op.p0_;
                                    let _e674 = op.p1_;
                                    let _e676 = op.p2_;
                                    let _e678 = op.p3_;
                                    let _e680 = op.p4_;
                                    param_22 = u32(_e671);
                                    param_23 = vec4<f32>(_e674, _e676, _e678, _e680);
                                    let _e682 = (*time_2);
                                    param_24 = _e682;
                                    let _e683 = wired_material_wave_u0028_u1_u003b_vf4_u003b_f1_u003b((&param_22), (&param_23), (&param_24));
                                    wave_1 = _e683;
                                    let _e684 = wave_1;
                                    param_25 = max(abs(_e684), 0.0001f);
                                    let _e687 = wave_1;
                                    param_26 = _e687;
                                    let _e688 = wired_stage_copy_sign_u0028_f1_u003b_f1_u003b((&param_25), (&param_26));
                                    wave_1 = _e688;
                                    let _e689 = uv_1;
                                    let _e692 = wave_1;
                                    uv_1 = (((_e689 - vec2(0.5f)) / vec2(_e692)) + vec2(0.5f));
                                } else {
                                    let _e698 = op.type_;
                                    if (_e698 == 6u) {
                                        let _e701 = uv_1[0u];
                                        let _e703 = op.p0_;
                                        let _e706 = uv_1[1u];
                                        let _e708 = op.p2_;
                                        let _e712 = op.p4_;
                                        let _e715 = uv_1[0u];
                                        let _e717 = op.p1_;
                                        let _e720 = uv_1[1u];
                                        let _e722 = op.p3_;
                                        let _e726 = op.p5_;
                                        uv_1 = vec2<f32>((((_e701 * _e703) + (_e706 * _e708)) + _e712), (((_e715 * _e717) + (_e720 * _e722)) + _e726));
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
                let _e729 = i;
                i = (_e729 + bitcast<u32>(1i));
            }
        }
        let _e732 = uv_1;
        return _e732;
    }
    let _e733 = (*authoredUv);
    let _e735 = (*stage_1).scaleScroll;
    let _e739 = (*stage_1).scaleScroll;
    let _e741 = (*time_2);
    uv_2 = ((_e733 * _e735.xy) + (_e739.zw * _e741));
    let _e746 = (*stage_1).params[3u];
    if (_e746 > 0.5f) {
        let _e750 = (*stage_1).stretch[0u];
        let _e753 = (*stage_1).stretch[1u];
        let _e756 = (*stage_1).stretch[2u];
        let _e757 = (*time_2);
        let _e760 = (*stage_1).stretch[3u];
        wave_2 = (_e750 + (_e753 * sin(((_e756 + (_e757 * _e760)) * 6.2831855f))));
        let _e767 = uv_2;
        let _e770 = wave_2;
        uv_2 = (((_e767 - vec2(0.5f)) / vec2(max(abs(_e770), 0.0001f))) + vec2(0.5f));
    }
    let _e779 = (*stage_1).params[1u];
    if (abs(_e779) > 0.0001f) {
        let _e784 = (*stage_1).params[1u];
        let _e785 = (*time_2);
        angle = ((_e784 * _e785) * 0.017453292f);
        let _e788 = angle;
        sine_1 = sin(_e788);
        let _e790 = angle;
        cosine_1 = cos(_e790);
        let _e792 = uv_2;
        centered_1 = (_e792 - vec2(0.5f));
        let _e796 = centered_1[0u];
        let _e797 = cosine_1;
        let _e800 = centered_1[1u];
        let _e801 = sine_1;
        let _e805 = centered_1[0u];
        let _e806 = sine_1;
        let _e809 = centered_1[1u];
        let _e810 = cosine_1;
        uv_2 = (vec2<f32>(((_e796 * _e797) - (_e800 * _e801)), ((_e805 * _e806) + (_e809 * _e810))) + vec2(0.5f));
    }
    let _e818 = (*stage_1).params[2u];
    if (_e818 > 0.5f) {
        let _e822 = (*stage_1).turbulence[2u];
        let _e823 = (*time_2);
        let _e826 = (*stage_1).turbulence[3u];
        phase_3 = (_e822 + (_e823 * _e826));
        let _e831 = (*stage_1).turbulence[0u];
        let _e834 = (*stage_1).turbulence[1u];
        let _e836 = uv_2[0u];
        let _e838 = uv_2[1u];
        let _e840 = phase_3;
        offset = (_e831 + (_e834 * sin((((_e836 + _e838) + _e840) * 6.2831855f))));
        let _e846 = offset;
        let _e847 = uv_2;
        uv_2 = (_e847 + vec2(_e846));
    }
    let _e850 = uv_2;
    return _e850;
}

fn wired_sky_authored_atlas_uv_inset_u0028_vf3_u003b_vf2_u003b(direction: ptr<function, vec3<f32>>, inset: ptr<function, vec2<f32>>) -> vec2<f32> {
    var ad: vec3<f32>;
    var face: u32;
    var faceUv: vec2<f32>;
    var phi_1092_: bool;

    let _e524 = (*direction);
    ad = abs(_e524);
    let _e527 = ad[0u];
    let _e529 = ad[1u];
    let _e530 = (_e527 >= _e529);
    phi_1092_ = _e530;
    if _e530 {
        let _e532 = ad[0u];
        let _e534 = ad[2u];
        phi_1092_ = (_e532 >= _e534);
    }
    let _e537 = phi_1092_;
    if _e537 {
        let _e539 = (*direction)[0u];
        if (_e539 >= 0f) {
            face = 0u;
            let _e542 = (*direction)[1u];
            let _e545 = (*direction)[2u];
            let _e549 = ad[0u];
            faceUv = (vec2<f32>(-(_e542), -(_e545)) / vec2(_e549));
        } else {
            face = 2u;
            let _e553 = (*direction)[1u];
            let _e555 = (*direction)[2u];
            let _e559 = ad[0u];
            faceUv = (vec2<f32>(_e553, -(_e555)) / vec2(_e559));
        }
    } else {
        let _e563 = ad[1u];
        let _e565 = ad[2u];
        if (_e563 >= _e565) {
            let _e568 = (*direction)[1u];
            if (_e568 >= 0f) {
                face = 1u;
                let _e571 = (*direction)[0u];
                let _e573 = (*direction)[2u];
                let _e577 = ad[1u];
                faceUv = (vec2<f32>(_e571, -(_e573)) / vec2(_e577));
            } else {
                face = 3u;
                let _e581 = (*direction)[0u];
                let _e584 = (*direction)[2u];
                let _e588 = ad[1u];
                faceUv = (vec2<f32>(-(_e581), -(_e584)) / vec2(_e588));
            }
        } else {
            let _e592 = (*direction)[2u];
            if (_e592 >= 0f) {
                face = 4u;
                let _e595 = (*direction)[1u];
                let _e598 = (*direction)[0u];
                let _e601 = ad[2u];
                faceUv = (vec2<f32>(-(_e595), _e598) / vec2(_e601));
            } else {
                face = 5u;
                let _e605 = (*direction)[1u];
                let _e608 = (*direction)[0u];
                let _e612 = ad[2u];
                faceUv = (vec2<f32>(-(_e605), -(_e608)) / vec2(_e612));
            }
        }
    }
    let _e615 = faceUv;
    let _e619 = (*inset);
    let _e620 = (*inset);
    faceUv = clamp(((_e615 * 0.5f) + vec2(0.5f)), _e619, (vec2<f32>(1f, 1f) - _e620));
    let _e623 = faceUv;
    let _e624 = face;
    let _e627 = face;
    return ((_e623 + vec2<f32>(f32((_e624 % 3u)), f32((_e627 / 3u)))) / vec2<f32>(3f, 2f));
}

fn wired_sky_authored_atlas_uv_u0028_vf3_u003b(direction_1: ptr<function, vec3<f32>>) -> vec2<f32> {
    var param_27: vec3<f32>;
    var param_28: vec2<f32>;

    let _e522 = (*direction_1);
    param_27 = _e522;
    param_28 = vec2<f32>(0.001f, 0.001f);
    let _e523 = wired_sky_authored_atlas_uv_inset_u0028_vf3_u003b_vf2_u003b((&param_27), (&param_28));
    return _e523;
}

fn wired_sky_outer_sample_u0028_vf3_u003b(direction_2: ptr<function, vec3<f32>>) -> vec4<f32> {
    var atlasUv: vec2<f32>;
    var param_29: vec3<f32>;
    var cellUv: vec2<f32>;
    var face_1: u32;
    var uv_3: vec2<f32>;

    let _e525 = (*direction_2);
    param_29 = _e525;
    let _e526 = wired_sky_authored_atlas_uv_u0028_vf3_u003b((&param_29));
    atlasUv = _e526;
    let _e527 = atlasUv;
    cellUv = (_e527 * vec2<f32>(3f, 2f));
    let _e530 = cellUv[1u];
    let _e535 = cellUv[0u];
    face_1 = ((u32(floor(_e530)) * 3u) + u32(floor(_e535)));
    let _e539 = cellUv;
    uv_3 = fract(_e539);
    let _e541 = face_1;
    switch bitcast<i32>(_e541) {
        case 0: {
            let _e545 = uv_3;
            let _e546 = textureSample(outerRt, outerSampler, _e545);
            return _e546;
        }
        case 1: {
            let _e547 = uv_3;
            let _e548 = textureSample(outerBk, outerSampler, _e547);
            return _e548;
        }
        case 2: {
            let _e549 = uv_3;
            let _e550 = textureSample(outerLf, outerSampler, _e549);
            return _e550;
        }
        case 3: {
            let _e551 = uv_3;
            let _e552 = textureSample(outerFt, outerSampler, _e551);
            return _e552;
        }
        case 4: {
            let _e553 = uv_3;
            let _e554 = textureSample(outerUp, outerSampler, _e553);
            return _e554;
        }
        default: {
            let _e543 = uv_3;
            let _e544 = textureSample(outerDn, outerSampler, _e543);
            return _e544;
        }
    }
}

fn wired_sky_cloud_uv_u0028_vf3_u003b_f1_u003b(direction_3: ptr<function, vec3<f32>>, height: ptr<function, f32>) -> vec2<f32> {
    var cloudHeight: f32;
    var rayDistance: f32;
    var cloudDirection: vec3<f32>;

    let _e524 = (*height);
    cloudHeight = max(_e524, 1f);
    let _e527 = (*direction_3)[2u];
    let _e531 = (*direction_3)[2u];
    let _e533 = (*direction_3)[2u];
    let _e537 = cloudHeight;
    let _e540 = cloudHeight;
    let _e541 = cloudHeight;
    rayDistance = ((-(_e527) * 4096f) + sqrt((((((_e531 * _e533) * 4096f) * 4096f) + (8192f * _e537)) + (_e540 * _e541))));
    let _e546 = (*direction_3);
    let _e547 = rayDistance;
    cloudDirection = normalize(((_e546 * _e547) + vec3<f32>(0f, 0f, 4096f)));
    let _e551 = cloudDirection;
    return acos(clamp(_e551.xy, vec2(-1f), vec2(1f)));
}

fn main_1() {
    var direction_4: vec3<f32>;
    var cloudUv: vec2<f32>;
    var param_30: vec3<f32>;
    var param_31: f32;
    var accumulated: vec4<f32>;
    var param_32: vec3<f32>;
    var i_1: u32;
    var stage_2: MaterialStageParams;
    var imageSource: u32;
    var tcGen: u32;
    var uv_4: vec2<f32>;
    var reflected: vec3<f32>;
    var param_33: vec2<f32>;
    var param_34: MaterialStageParams;
    var param_35: f32;
    var param_36: vec3<f32>;
    var source_2: vec4<f32>;
    var local_5: vec4<f32>;
    var param_37: u32;
    var param_38: vec2<f32>;
    var vertexAlpha_1: f32;
    var param_39: vec4<f32>;
    var param_40: MaterialStageParams;
    var param_41: vec4<f32>;
    var param_42: vec4<f32>;
    var param_43: vec3<f32>;
    var param_44: f32;
    var param_45: f32;
    var rejected: bool;
    var param_46: u32;
    var param_47: vec4<f32>;
    var param_48: vec4<f32>;
    var param_49: u32;
    var param_50: vec4<f32>;
    var param_51: vec4<f32>;
    var uv_5: vec2<f32>;
    var param_52: vec3<f32>;
    var param_53: vec3<f32>;
    var param_54: f32;
    var param_55: vec3<f32>;
    var phi_2242_: bool;
    var phi_2256_: bool;
    var phi_2257_: bool;
    var phi_2271_: bool;
    var phi_2272_: bool;

    let _e559 = worldPosition_1;
    let _e561 = productDraw.atmosphereEyeDensity;
    direction_4 = normalize((_e559 - _e561.xyz));
    let _e565 = direction_4;
    param_30 = _e565;
    let _e568 = skyMaterial.timeHeight[1u];
    param_31 = _e568;
    let _e569 = wired_sky_cloud_uv_u0028_vf3_u003b_f1_u003b((&param_30), (&param_31));
    cloudUv = _e569;
    accumulated = vec4<f32>(0f, 0f, 0f, 0f);
    let _e572 = skyMaterial.meta_[1u];
    if (_e572 != 0u) {
        let _e574 = direction_4;
        param_32 = _e574;
        let _e575 = wired_sky_outer_sample_u0028_vf3_u003b((&param_32));
        accumulated = _e575;
    }
    i_1 = 0u;
    loop {
        let _e576 = i_1;
        let _e579 = skyMaterial.meta_[0u];
        if (_e576 < min(_e579, 8u)) {
            let _e582 = i_1;
            let _e585 = skyMaterial.stages[_e582];
            stage_2.meta_ = _e585.meta_;
            stage_2.generators = _e585.generators;
            stage_2.params = _e585.params;
            stage_2.scaleScroll = _e585.scaleScroll;
            stage_2.turbulence = _e585.turbulence;
            stage_2.stretch = _e585.stretch;
            stage_2.constantColor = _e585.constantColor;
            stage_2.rgbWave = _e585.rgbWave;
            stage_2.alphaWave = _e585.alphaWave;
            stage_2.tcGenVectorS = _e585.tcGenVectorS;
            stage_2.tcGenVectorT = _e585.tcGenVectorT;
            stage_2.tcModMeta = _e585.tcModMeta;
            stage_2.tcMods[0i].type_ = _e585.tcMods[0].type_;
            stage_2.tcMods[0i].p0_ = _e585.tcMods[0].p0_;
            stage_2.tcMods[0i].p1_ = _e585.tcMods[0].p1_;
            stage_2.tcMods[0i].p2_ = _e585.tcMods[0].p2_;
            stage_2.tcMods[0i].p3_ = _e585.tcMods[0].p3_;
            stage_2.tcMods[0i].p4_ = _e585.tcMods[0].p4_;
            stage_2.tcMods[0i].p5_ = _e585.tcMods[0].p5_;
            stage_2.tcMods[0i].reserved = _e585.tcMods[0].reserved;
            stage_2.tcMods[1i].type_ = _e585.tcMods[1].type_;
            stage_2.tcMods[1i].p0_ = _e585.tcMods[1].p0_;
            stage_2.tcMods[1i].p1_ = _e585.tcMods[1].p1_;
            stage_2.tcMods[1i].p2_ = _e585.tcMods[1].p2_;
            stage_2.tcMods[1i].p3_ = _e585.tcMods[1].p3_;
            stage_2.tcMods[1i].p4_ = _e585.tcMods[1].p4_;
            stage_2.tcMods[1i].p5_ = _e585.tcMods[1].p5_;
            stage_2.tcMods[1i].reserved = _e585.tcMods[1].reserved;
            stage_2.tcMods[2i].type_ = _e585.tcMods[2].type_;
            stage_2.tcMods[2i].p0_ = _e585.tcMods[2].p0_;
            stage_2.tcMods[2i].p1_ = _e585.tcMods[2].p1_;
            stage_2.tcMods[2i].p2_ = _e585.tcMods[2].p2_;
            stage_2.tcMods[2i].p3_ = _e585.tcMods[2].p3_;
            stage_2.tcMods[2i].p4_ = _e585.tcMods[2].p4_;
            stage_2.tcMods[2i].p5_ = _e585.tcMods[2].p5_;
            stage_2.tcMods[2i].reserved = _e585.tcMods[2].reserved;
            stage_2.tcMods[3i].type_ = _e585.tcMods[3].type_;
            stage_2.tcMods[3i].p0_ = _e585.tcMods[3].p0_;
            stage_2.tcMods[3i].p1_ = _e585.tcMods[3].p1_;
            stage_2.tcMods[3i].p2_ = _e585.tcMods[3].p2_;
            stage_2.tcMods[3i].p3_ = _e585.tcMods[3].p3_;
            stage_2.tcMods[3i].p4_ = _e585.tcMods[3].p4_;
            stage_2.tcMods[3i].p5_ = _e585.tcMods[3].p5_;
            stage_2.tcMods[3i].reserved = _e585.tcMods[3].reserved;
            stage_2.tcMods[4i].type_ = _e585.tcMods[4].type_;
            stage_2.tcMods[4i].p0_ = _e585.tcMods[4].p0_;
            stage_2.tcMods[4i].p1_ = _e585.tcMods[4].p1_;
            stage_2.tcMods[4i].p2_ = _e585.tcMods[4].p2_;
            stage_2.tcMods[4i].p3_ = _e585.tcMods[4].p3_;
            stage_2.tcMods[4i].p4_ = _e585.tcMods[4].p4_;
            stage_2.tcMods[4i].p5_ = _e585.tcMods[4].p5_;
            stage_2.tcMods[4i].reserved = _e585.tcMods[4].reserved;
            stage_2.tcMods[5i].type_ = _e585.tcMods[5].type_;
            stage_2.tcMods[5i].p0_ = _e585.tcMods[5].p0_;
            stage_2.tcMods[5i].p1_ = _e585.tcMods[5].p1_;
            stage_2.tcMods[5i].p2_ = _e585.tcMods[5].p2_;
            stage_2.tcMods[5i].p3_ = _e585.tcMods[5].p3_;
            stage_2.tcMods[5i].p4_ = _e585.tcMods[5].p4_;
            stage_2.tcMods[5i].p5_ = _e585.tcMods[5].p5_;
            stage_2.tcMods[5i].reserved = _e585.tcMods[5].reserved;
            stage_2.tcMods[6i].type_ = _e585.tcMods[6].type_;
            stage_2.tcMods[6i].p0_ = _e585.tcMods[6].p0_;
            stage_2.tcMods[6i].p1_ = _e585.tcMods[6].p1_;
            stage_2.tcMods[6i].p2_ = _e585.tcMods[6].p2_;
            stage_2.tcMods[6i].p3_ = _e585.tcMods[6].p3_;
            stage_2.tcMods[6i].p4_ = _e585.tcMods[6].p4_;
            stage_2.tcMods[6i].p5_ = _e585.tcMods[6].p5_;
            stage_2.tcMods[6i].reserved = _e585.tcMods[6].reserved;
            stage_2.tcMods[7i].type_ = _e585.tcMods[7].type_;
            stage_2.tcMods[7i].p0_ = _e585.tcMods[7].p0_;
            stage_2.tcMods[7i].p1_ = _e585.tcMods[7].p1_;
            stage_2.tcMods[7i].p2_ = _e585.tcMods[7].p2_;
            stage_2.tcMods[7i].p3_ = _e585.tcMods[7].p3_;
            stage_2.tcMods[7i].p4_ = _e585.tcMods[7].p4_;
            stage_2.tcMods[7i].p5_ = _e585.tcMods[7].p5_;
            stage_2.tcMods[7i].reserved = _e585.tcMods[7].reserved;
            let _e758 = stage_2.meta_[2u];
            imageSource = (_e758 & 255u);
            let _e762 = stage_2.meta_[2u];
            tcGen = (_e762 >> bitcast<u32>(8u));
            let _e765 = imageSource;
            let _e767 = tcGen;
            let _e770 = lightmapCoord_1;
            let _e771 = cloudUv;
            uv_4 = select(_e771, _e770, vec2(((_e765 == 1u) || (_e767 == 1u))));
            let _e774 = tcGen;
            if (_e774 == 2u) {
                let _e776 = direction_4;
                let _e777 = worldNormal_1;
                reflected = reflect(_e776, normalize(_e777));
                let _e781 = reflected[1u];
                let _e785 = reflected[2u];
                uv_4 = vec2<f32>((0.5f + (_e781 * 0.5f)), (0.5f - (_e785 * 0.5f)));
            } else {
                let _e789 = tcGen;
                if (_e789 == 3u) {
                    let _e791 = worldPosition_1;
                    let _e793 = stage_2.tcGenVectorS;
                    let _e796 = worldPosition_1;
                    let _e798 = stage_2.tcGenVectorT;
                    uv_4 = vec2<f32>(dot(_e791, _e793.xyz), dot(_e796, _e798.xyz));
                }
            }
            let _e802 = uv_4;
            param_33 = _e802;
            let _e803 = stage_2;
            param_34 = _e803;
            let _e806 = skyMaterial.timeHeight[0u];
            param_35 = _e806;
            let _e807 = worldPosition_1;
            param_36 = _e807;
            let _e808 = wired_material_stage_uv_u0028_vf2_u003b_struct_u002d_MaterialStageParams_u002d_vu4_u002d_vu4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vu4_u002d_struct_u002d_MaterialTcMod_u002d_u1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_u11_u005b_8_u005d_1_u003b_f1_u003b_vf3_u003b((&param_33), (&param_34), (&param_35), (&param_36));
            uv_4 = _e808;
            let _e809 = imageSource;
            if (_e809 == 2u) {
                local_5 = vec4<f32>(1f, 1f, 1f, 1f);
            } else {
                let _e811 = i_1;
                param_37 = _e811;
                let _e812 = uv_4;
                param_38 = _e812;
                let _e813 = wired_sky_stage_sample_u0028_u1_u003b_vf2_u003b((&param_37), (&param_38));
                local_5 = _e813;
            }
            let _e814 = local_5;
            source_2 = _e814;
            let _e816 = color_2[3u];
            vertexAlpha_1 = _e816;
            let _e819 = stage_2.generators[1u];
            if (_e819 == 8u) {
                let _e821 = worldPosition_1;
                let _e823 = productDraw.atmosphereEyeDensity;
                let _e829 = stage_2.tcGenVectorS[3u];
                vertexAlpha_1 = (floor((clamp((length((_e821 - _e823.xyz)) / max(_e829, 0.001f)), 0f, 1f) * 255f)) / 255f);
            }
            let _e836 = source_2;
            param_39 = _e836;
            let _e837 = stage_2;
            param_40 = _e837;
            let _e838 = color_2;
            param_41 = _e838;
            param_42 = vec4<f32>(1f, 1f, 1f, 1f);
            param_43 = vec3<f32>(1f, 1f, 1f);
            let _e839 = vertexAlpha_1;
            param_44 = _e839;
            let _e842 = skyMaterial.timeHeight[0u];
            param_45 = _e842;
            let _e843 = wired_material_stage_generators_u0028_vf4_u003b_struct_u002d_MaterialStageParams_u002d_vu4_u002d_vu4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vu4_u002d_struct_u002d_MaterialTcMod_u002d_u1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_u11_u005b_8_u005d_1_u003b_vf4_u003b_vf4_u003b_vf3_u003b_f1_u003b_f1_u003b((&param_39), (&param_40), (&param_41), (&param_42), (&param_43), (&param_44), (&param_45));
            source_2 = _e843;
            let _e846 = stage_2.meta_[3u];
            let _e847 = (_e846 == 1u);
            phi_2242_ = _e847;
            if _e847 {
                let _e849 = source_2[3u];
                phi_2242_ = (_e849 == 0f);
            }
            let _e852 = phi_2242_;
            phi_2257_ = _e852;
            if !(_e852) {
                let _e856 = stage_2.meta_[3u];
                let _e857 = (_e856 == 2u);
                phi_2256_ = _e857;
                if _e857 {
                    let _e859 = source_2[3u];
                    let _e862 = stage_2.params[0u];
                    phi_2256_ = (_e859 >= _e862);
                }
                let _e865 = phi_2256_;
                phi_2257_ = _e865;
            }
            let _e867 = phi_2257_;
            phi_2272_ = _e867;
            if !(_e867) {
                let _e871 = stage_2.meta_[3u];
                let _e872 = (_e871 == 3u);
                phi_2271_ = _e872;
                if _e872 {
                    let _e874 = source_2[3u];
                    let _e877 = stage_2.params[0u];
                    phi_2271_ = (_e874 < _e877);
                }
                let _e880 = phi_2271_;
                phi_2272_ = _e880;
            }
            let _e882 = phi_2272_;
            rejected = _e882;
            let _e883 = rejected;
            if !(_e883) {
                let _e885 = source_2;
                let _e888 = stage_2.meta_[0u];
                param_46 = _e888;
                let _e889 = source_2;
                param_47 = _e889;
                let _e890 = accumulated;
                param_48 = _e890;
                let _e891 = wired_material_blend_factor_u0028_u1_u003b_vf4_u003b_vf4_u003b((&param_46), (&param_47), (&param_48));
                let _e893 = accumulated;
                let _e896 = stage_2.meta_[1u];
                param_49 = _e896;
                let _e897 = source_2;
                param_50 = _e897;
                let _e898 = accumulated;
                param_51 = _e898;
                let _e899 = wired_material_blend_factor_u0028_u1_u003b_vf4_u003b_vf4_u003b((&param_49), (&param_50), (&param_51));
                accumulated = ((_e885 * _e891) + (_e893 * _e899));
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e902 = i_1;
            i_1 = (_e902 + bitcast<u32>(1i));
        }
    }
    let _e907 = productDraw.atmosphereHeightSky[2u];
    if (_e907 > 0f) {
        let _e910 = direction_4[1u];
        let _e912 = direction_4[0u];
        let _e917 = direction_4[2u];
        uv_5 = vec2<f32>(((atan2(_e910, _e912) / 6.2831855f) + 0.5f), ((asin(clamp(_e917, -1f, 1f)) / 3.1415927f) + 0.5f));
        let _e923 = uv_5;
        let _e924 = textureSample(physicalImage, physicalSampler, _e923);
        let _e927 = productDraw.atmosphereHeightSky[2u];
        let _e929 = accumulated;
        param_52 = _e929.xyz;
        param_53 = _e924.xyz;
        param_54 = clamp(_e927, 0f, 1f);
        let _e932 = wired_compose_authored_physical_sky_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_52), (&param_53), (&param_54));
        accumulated[0u] = _e932.x;
        accumulated[1u] = _e932.y;
        accumulated[2u] = _e932.z;
    }
    let _e939 = accumulated;
    param_55 = _e939.xyz;
    let _e941 = wired_display_visibility_u0028_vf3_u003b((&param_55));
    outColor = vec4<f32>(_e941.x, _e941.y, _e941.z, 1f);
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
