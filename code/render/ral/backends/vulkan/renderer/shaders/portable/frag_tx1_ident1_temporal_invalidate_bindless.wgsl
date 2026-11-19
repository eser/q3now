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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
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

    let _e67 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e67 + 0.5f));
    let _e72 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e74 = fogType;
    let _e77 = fogType;
    return (((_e72 > 0.5f) && (_e74 >= 1i)) && (_e77 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e67 = wired_advanced_fog_enabled_u0028_();
    if !(_e67) {
        return 0f;
    }
    let _e70 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e70, 0.000001f));
    let _e75 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e75 + 0.5f));
    let _e78 = fogType_1;
    if (_e78 == 1i) {
        let _e82 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e82 <= 0f) {
            return 0f;
        }
        let _e84 = viewDepth;
        let _e87 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e84 / _e87), 0f, 1f);
    }
    let _e92 = unnamed.advancedFogColorDensity[3u];
    let _e94 = viewDepth;
    opticalDepth = (max(_e92, 0f) * _e94);
    let _e96 = fogType_1;
    if (_e96 == 2i) {
        let _e98 = opticalDepth;
        return clamp((1f - exp(-(_e98))), 0f, 1f);
    }
    let _e103 = opticalDepth;
    let _e104 = opticalDepth;
    return clamp((1f - exp(-((_e103 * _e104)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e67 = (*rgb);
    let _e70 = unnamed.worldLightParams[0u];
    boosted = (_e67 * _e70);
    let _e73 = boosted[0u];
    let _e75 = boosted[1u];
    let _e77 = boosted[2u];
    peak = max(_e73, max(_e75, _e77));
    let _e80 = peak;
    if (_e80 > 1f) {
        let _e82 = peak;
        let _e83 = boosted;
        boosted = (_e83 / vec3(_e82));
    }
    let _e86 = boosted;
    return _e86;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e68 = (*c);
    (*c) = max(_e68, vec3<f32>(0f, 0f, 0f));
    let _e70 = (*c);
    cutoff = (_e70 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e72 = (*c);
    lo = (_e72 / vec3(12.92f));
    let _e75 = (*c);
    hi = pow(((_e75 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e80 = hi;
    let _e81 = lo;
    let _e82 = cutoff;
    return mix(_e80, _e81, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e82));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e70 = (*role);
    let _e72 = (*role);
    let _e77 = unnamed.packed_indices[(_e70 / 4u)][(_e72 % 4u)];
    let _e80 = (*role);
    let _e82 = (*role);
    let _e87 = unnamed.packed_indices[(_e80 / 4u)][(_e82 % 4u)];
    let _e92 = (*uv);
    let _e93 = textureSample(wired_bindless_images[(_e77 & 4095u)], wired_bindless_samplers[((_e87 >> bitcast<u32>(12i)) & 255u)], _e92);
    c_1 = _e93;
    let _e94 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e94))) == 0i) {
        let _e99 = c_1;
        param = _e99.xyz;
        let _e101 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e101.x;
        c_1[1u] = _e101.y;
        c_1[2u] = _e101.z;
    }
    let _e108 = (*slot);
    if (lightmap_slot == (_e108 + 1i)) {
        let _e111 = c_1;
        param_1 = _e111.xyz;
        let _e113 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e113.x;
        c_1[1u] = _e113.y;
        c_1[2u] = _e113.z;
    }
    let _e120 = c_1;
    return _e120;
}

fn main_1() {
    var sampledColor0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var sampledColor1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_8: f32;

    param_2 = 0u;
    let _e82 = frag_tex_coord0_1;
    param_3 = _e82;
    param_4 = 0i;
    let _e83 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e83;
    param_5 = 1u;
    let _e84 = frag_tex_coord1_1;
    param_6 = _e84;
    param_7 = 1i;
    let _e85 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor1_ = _e85;
    let _e86 = sampledColor0_;
    color0_ = _e86;
    if override_type_4_ {
        let _e87 = sampledColor1_;
        color1_ = _e87;
        let _e88 = color0_;
        let _e90 = color1_;
        let _e92 = (_e88.xyz + _e90.xyz);
        let _e94 = color0_[3u];
        let _e96 = color1_[3u];
        base = vec4<f32>(_e92.x, _e92.y, _e92.z, (_e94 * _e96));
    } else {
        if override_type_4_1 {
            let _e102 = sampledColor1_;
            color1_1 = _e102;
            let _e103 = color0_;
            let _e105 = color1_1;
            let _e107 = (_e103.xyz + _e105.xyz);
            let _e109 = color0_[3u];
            let _e111 = color1_1[3u];
            base = vec4<f32>(_e107.x, _e107.y, _e107.z, (_e109 * _e111));
        } else {
            let _e117 = sampledColor1_;
            color1_2 = _e117;
            let _e118 = color0_;
            let _e120 = color1_2;
            let _e122 = (_e118.xyz * _e120.xyz);
            base[0u] = _e122.x;
            base[1u] = _e122.y;
            base[2u] = _e122.z;
            let _e130 = color0_[3u];
            let _e132 = color1_2[3u];
            base[3u] = (_e130 * _e132);
        }
    }
    if override_type_4_2 {
        let _e137 = unnamed.worldLightParams[1u];
        wetness = clamp(_e137, 0f, 1f);
        let _e141 = unnamed.worldLightParams[2u];
        frost = clamp(_e141, 0f, 1f);
        let _e143 = base;
        luminance = dot(_e143.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e146 = wetness;
        let _e148 = base;
        let _e150 = (_e148.xyz * mix(1f, 0.82f, _e146));
        base[0u] = _e150.x;
        base[1u] = _e150.y;
        base[2u] = _e150.z;
        let _e157 = base;
        let _e159 = luminance;
        let _e161 = luminance;
        let _e163 = luminance;
        let _e165 = frost;
        let _e168 = mix(_e157.xyz, vec3<f32>((_e159 * 0.88f), (_e161 * 0.94f), _e163), vec3((_e165 * 0.55f)));
        base[0u] = _e168.x;
        base[1u] = _e168.y;
        base[2u] = _e168.z;
    }
    let _e177 = unnamed.materialFactors[0u];
    if (_e177 >= 0f) {
        let _e180 = unnamed.materialBaseColor;
        let _e181 = base;
        base = (_e181 * _e180);
    }
    let _e183 = color0_;
    let _e186 = unnamed.emissionRadiance;
    let _e189 = base;
    let _e191 = (_e189.xyz + (_e183.xyz * _e186.xyz));
    base[0u] = _e191.x;
    base[1u] = _e191.y;
    base[2u] = _e191.z;
    let _e198 = wired_advanced_fog_enabled_u0028_();
    if _e198 {
        let _e199 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e199;
        let _e200 = base;
        let _e203 = unnamed.advancedFogColorDensity;
        let _e205 = fogAmount;
        let _e207 = mix(_e200.xyz, _e203.xyz, vec3(_e205));
        base[0u] = _e207.x;
        base[1u] = _e207.y;
        base[2u] = _e207.z;
    }
    if override_type_4_3 {
        let _e215 = base[3u];
        if (_e215 == 0f) {
            discard;
        }
    } else {
        if override_type_4_4 {
            let _e217 = base;
            let _e219 = base;
            if (dot(_e217.xyz, _e219.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e223 = base;
    out_color = _e223;
    param_8 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_8));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
