enable wgpu_binding_array;

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    fogDistanceVector: vec4<f32>,
    fogDepthVector: vec4<f32>,
    fogEyeT: vec4<f32>,
    fogColor: vec4<f32>,
    _pad_to_packed_indices: array<vec4<f32>, 26>,
    packed_indices: array<vec4<u32>, 3>,
    worldLightParams: vec4<f32>,
    advancedFogColorDensity: vec4<f32>,
    advancedFogTypeFarEnabled: vec4<f32>,
    emissionRadiance: vec4<f32>,
    materialBaseColor: vec4<f32>,
    materialFactors: vec4<f32>,
}

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_4_: bool = (tex_mode == 1i);
override override_type_4_1: bool = (tex_mode == 2i);
override override_type_4_2: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_3: bool = (discard_mode == 1i);
override override_type_4_4: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e69 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e69 + 0.5f));
    let _e74 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e76 = fogType;
    let _e79 = fogType;
    return (((_e74 > 0.5f) && (_e76 >= 1i)) && (_e79 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e69 = wired_advanced_fog_enabled_u0028_();
    if !(_e69) {
        return 0f;
    }
    let _e72 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e72, 0.000001f));
    let _e77 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e77 + 0.5f));
    let _e80 = fogType_1;
    if (_e80 == 1i) {
        let _e84 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e84 <= 0f) {
            return 0f;
        }
        let _e86 = viewDepth;
        let _e89 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e86 / _e89), 0f, 1f);
    }
    let _e94 = unnamed.advancedFogColorDensity[3u];
    let _e96 = viewDepth;
    opticalDepth = (max(_e94, 0f) * _e96);
    let _e98 = fogType_1;
    if (_e98 == 2i) {
        let _e100 = opticalDepth;
        return clamp((1f - exp(-(_e100))), 0f, 1f);
    }
    let _e105 = opticalDepth;
    let _e106 = opticalDepth;
    return clamp((1f - exp(-((_e105 * _e106)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e69 = (*rgb);
    let _e72 = unnamed.worldLightParams[0u];
    boosted = (_e69 * _e72);
    let _e75 = boosted[0u];
    let _e77 = boosted[1u];
    let _e79 = boosted[2u];
    peak = max(_e75, max(_e77, _e79));
    let _e82 = peak;
    if (_e82 > 1f) {
        let _e84 = peak;
        let _e85 = boosted;
        boosted = (_e85 / vec3(_e84));
    }
    let _e88 = boosted;
    return _e88;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e70 = (*c);
    (*c) = max(_e70, vec3<f32>(0f, 0f, 0f));
    let _e72 = (*c);
    cutoff = (_e72 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e74 = (*c);
    lo = (_e74 / vec3(12.92f));
    let _e77 = (*c);
    hi = pow(((_e77 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e82 = hi;
    let _e83 = lo;
    let _e84 = cutoff;
    return mix(_e82, _e83, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e84));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e72 = (*role);
    let _e74 = (*role);
    let _e79 = unnamed.packed_indices[(_e72 / 4u)][(_e74 % 4u)];
    let _e82 = (*role);
    let _e84 = (*role);
    let _e89 = unnamed.packed_indices[(_e82 / 4u)][(_e84 % 4u)];
    let _e94 = (*uv);
    let _e95 = textureSample(wired_bindless_images[(_e79 & 4095u)], wired_bindless_samplers[((_e89 >> bitcast<u32>(12i)) & 255u)], _e94);
    c_1 = _e95;
    let _e96 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e96))) == 0i) {
        let _e101 = c_1;
        param = _e101.xyz;
        let _e103 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e103.x;
        c_1[1u] = _e103.y;
        c_1[2u] = _e103.z;
    }
    let _e110 = (*slot);
    if (lightmap_slot == (_e110 + 1i)) {
        let _e113 = c_1;
        param_1 = _e113.xyz;
        let _e115 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e115.x;
        c_1[1u] = _e115.y;
        c_1[2u] = _e115.z;
    }
    let _e122 = c_1;
    return _e122;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var sampledColor1_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var sampledColor2_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var color2_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color2_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color2_2: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_12: f32;

    let _e93 = frag_color0In_1;
    param_2 = _e93.xyz;
    let _e95 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e97 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e95.x, _e95.y, _e95.z, _e97);
    param_3 = 0u;
    let _e102 = frag_tex_coord0_1;
    param_4 = _e102;
    param_5 = 0i;
    let _e103 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e103;
    param_6 = 1u;
    let _e104 = frag_tex_coord1_1;
    param_7 = _e104;
    param_8 = 1i;
    let _e105 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    sampledColor1_ = _e105;
    param_9 = 2u;
    let _e106 = frag_tex_coord2_1;
    param_10 = _e106;
    param_11 = 2i;
    let _e107 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    sampledColor2_ = _e107;
    let _e108 = sampledColor0_;
    let _e109 = frag_color0_;
    color0_ = (_e108 * _e109);
    if override_type_4_ {
        let _e111 = sampledColor1_;
        color1_ = _e111;
        let _e112 = sampledColor2_;
        color2_ = _e112;
        let _e113 = color0_;
        let _e115 = color1_;
        let _e118 = color2_;
        let _e120 = ((_e113.xyz + _e115.xyz) + _e118.xyz);
        let _e122 = color0_[3u];
        let _e124 = color1_[3u];
        let _e127 = color2_[3u];
        base = vec4<f32>(_e120.x, _e120.y, _e120.z, ((_e122 * _e124) * _e127));
    } else {
        if override_type_4_1 {
            let _e133 = sampledColor1_;
            let _e134 = frag_color0_;
            color1_1 = (_e133 * _e134);
            let _e136 = sampledColor2_;
            let _e137 = frag_color0_;
            color2_1 = (_e136 * _e137);
            let _e139 = color0_;
            let _e141 = color1_1;
            let _e144 = color2_1;
            let _e146 = ((_e139.xyz + _e141.xyz) + _e144.xyz);
            let _e148 = color0_[3u];
            let _e150 = color1_1[3u];
            let _e153 = color2_1[3u];
            base = vec4<f32>(_e146.x, _e146.y, _e146.z, ((_e148 * _e150) * _e153));
        } else {
            let _e159 = sampledColor1_;
            color1_2 = _e159;
            let _e160 = sampledColor2_;
            color2_2 = _e160;
            let _e161 = color0_;
            let _e163 = color1_2;
            let _e166 = color2_2;
            let _e168 = ((_e161.xyz * _e163.xyz) * _e166.xyz);
            base[0u] = _e168.x;
            base[1u] = _e168.y;
            base[2u] = _e168.z;
            let _e176 = color0_[3u];
            let _e178 = color1_2[3u];
            let _e181 = color2_2[3u];
            base[3u] = ((_e176 * _e178) * _e181);
        }
    }
    if override_type_4_2 {
        let _e186 = unnamed.worldLightParams[1u];
        wetness = clamp(_e186, 0f, 1f);
        let _e190 = unnamed.worldLightParams[2u];
        frost = clamp(_e190, 0f, 1f);
        let _e192 = base;
        luminance = dot(_e192.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e195 = wetness;
        let _e197 = base;
        let _e199 = (_e197.xyz * mix(1f, 0.82f, _e195));
        base[0u] = _e199.x;
        base[1u] = _e199.y;
        base[2u] = _e199.z;
        let _e206 = base;
        let _e208 = luminance;
        let _e210 = luminance;
        let _e212 = luminance;
        let _e214 = frost;
        let _e217 = mix(_e206.xyz, vec3<f32>((_e208 * 0.88f), (_e210 * 0.94f), _e212), vec3((_e214 * 0.55f)));
        base[0u] = _e217.x;
        base[1u] = _e217.y;
        base[2u] = _e217.z;
    }
    let _e226 = unnamed.materialFactors[0u];
    if (_e226 >= 0f) {
        let _e229 = unnamed.materialBaseColor;
        let _e230 = base;
        base = (_e230 * _e229);
    }
    let _e232 = color0_;
    let _e235 = unnamed.emissionRadiance;
    let _e238 = base;
    let _e240 = (_e238.xyz + (_e232.xyz * _e235.xyz));
    base[0u] = _e240.x;
    base[1u] = _e240.y;
    base[2u] = _e240.z;
    let _e247 = wired_advanced_fog_enabled_u0028_();
    if _e247 {
        let _e248 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e248;
        let _e249 = base;
        let _e252 = unnamed.advancedFogColorDensity;
        let _e254 = fogAmount;
        let _e256 = mix(_e249.xyz, _e252.xyz, vec3(_e254));
        base[0u] = _e256.x;
        base[1u] = _e256.y;
        base[2u] = _e256.z;
    }
    if override_type_4_3 {
        let _e264 = base[3u];
        if (_e264 == 0f) {
            discard;
        }
    } else {
        if override_type_4_4 {
            let _e266 = base;
            let _e268 = base;
            if (dot(_e266.xyz, _e268.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e272 = base;
    out_color = _e272;
    param_12 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_12));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
