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

    let _e65 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e65 + 0.5f));
    let _e70 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e72 = fogType;
    let _e75 = fogType;
    return (((_e70 > 0.5f) && (_e72 >= 1i)) && (_e75 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e65 = wired_advanced_fog_enabled_u0028_();
    if !(_e65) {
        return 0f;
    }
    let _e68 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e68, 0.000001f));
    let _e73 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e73 + 0.5f));
    let _e76 = fogType_1;
    if (_e76 == 1i) {
        let _e80 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e80 <= 0f) {
            return 0f;
        }
        let _e82 = viewDepth;
        let _e85 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e82 / _e85), 0f, 1f);
    }
    let _e90 = unnamed.advancedFogColorDensity[3u];
    let _e92 = viewDepth;
    opticalDepth = (max(_e90, 0f) * _e92);
    let _e94 = fogType_1;
    if (_e94 == 2i) {
        let _e96 = opticalDepth;
        return clamp((1f - exp(-(_e96))), 0f, 1f);
    }
    let _e101 = opticalDepth;
    let _e102 = opticalDepth;
    return clamp((1f - exp(-((_e101 * _e102)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e65 = (*rgb);
    let _e68 = unnamed.worldLightParams[0u];
    boosted = (_e65 * _e68);
    let _e71 = boosted[0u];
    let _e73 = boosted[1u];
    let _e75 = boosted[2u];
    peak = max(_e71, max(_e73, _e75));
    let _e78 = peak;
    if (_e78 > 1f) {
        let _e80 = peak;
        let _e81 = boosted;
        boosted = (_e81 / vec3(_e80));
    }
    let _e84 = boosted;
    return _e84;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e66 = (*c);
    (*c) = max(_e66, vec3<f32>(0f, 0f, 0f));
    let _e68 = (*c);
    cutoff = (_e68 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e70 = (*c);
    lo = (_e70 / vec3(12.92f));
    let _e73 = (*c);
    hi = pow(((_e73 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e78 = hi;
    let _e79 = lo;
    let _e80 = cutoff;
    return mix(_e78, _e79, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e80));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e68 = (*role);
    let _e70 = (*role);
    let _e75 = unnamed.packed_indices[(_e68 / 4u)][(_e70 % 4u)];
    let _e78 = (*role);
    let _e80 = (*role);
    let _e85 = unnamed.packed_indices[(_e78 / 4u)][(_e80 % 4u)];
    let _e90 = (*uv);
    let _e91 = textureSample(wired_bindless_images[(_e75 & 4095u)], wired_bindless_samplers[((_e85 >> bitcast<u32>(12i)) & 255u)], _e90);
    c_1 = _e91;
    let _e92 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e92))) == 0i) {
        let _e97 = c_1;
        param = _e97.xyz;
        let _e99 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e99.x;
        c_1[1u] = _e99.y;
        c_1[2u] = _e99.z;
    }
    let _e106 = (*slot);
    if (lightmap_slot == (_e106 + 1i)) {
        let _e109 = c_1;
        param_1 = _e109.xyz;
        let _e111 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e111.x;
        c_1[1u] = _e111.y;
        c_1[2u] = _e111.z;
    }
    let _e118 = c_1;
    return _e118;
}

fn main_1() {
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var color1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color1_2: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_14: f32;

    param_2 = 0u;
    let _e84 = frag_tex_coord0_1;
    param_3 = _e84;
    param_4 = 0i;
    let _e85 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    color0_ = _e85;
    if override_type_4_ {
        param_5 = 1u;
        let _e86 = frag_tex_coord1_1;
        param_6 = _e86;
        param_7 = 1i;
        let _e87 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e87;
        let _e88 = color0_;
        let _e90 = color1_;
        let _e92 = (_e88.xyz + _e90.xyz);
        let _e94 = color0_[3u];
        let _e96 = color1_[3u];
        base = vec4<f32>(_e92.x, _e92.y, _e92.z, (_e94 * _e96));
    } else {
        if override_type_4_1 {
            param_8 = 1u;
            let _e102 = frag_tex_coord1_1;
            param_9 = _e102;
            param_10 = 1i;
            let _e103 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            color1_1 = _e103;
            let _e104 = color0_;
            let _e106 = color1_1;
            let _e108 = (_e104.xyz + _e106.xyz);
            let _e110 = color0_[3u];
            let _e112 = color1_1[3u];
            base = vec4<f32>(_e108.x, _e108.y, _e108.z, (_e110 * _e112));
        } else {
            param_11 = 1u;
            let _e118 = frag_tex_coord1_1;
            param_12 = _e118;
            param_13 = 1i;
            let _e119 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e119;
            let _e120 = color0_;
            let _e122 = color1_2;
            let _e124 = (_e120.xyz * _e122.xyz);
            base[0u] = _e124.x;
            base[1u] = _e124.y;
            base[2u] = _e124.z;
            let _e132 = color0_[3u];
            let _e134 = color1_2[3u];
            base[3u] = (_e132 * _e134);
        }
    }
    if override_type_4_2 {
        let _e139 = unnamed.worldLightParams[1u];
        wetness = clamp(_e139, 0f, 1f);
        let _e143 = unnamed.worldLightParams[2u];
        frost = clamp(_e143, 0f, 1f);
        let _e145 = base;
        luminance = dot(_e145.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e148 = wetness;
        let _e150 = base;
        let _e152 = (_e150.xyz * mix(1f, 0.82f, _e148));
        base[0u] = _e152.x;
        base[1u] = _e152.y;
        base[2u] = _e152.z;
        let _e159 = base;
        let _e161 = luminance;
        let _e163 = luminance;
        let _e165 = luminance;
        let _e167 = frost;
        let _e170 = mix(_e159.xyz, vec3<f32>((_e161 * 0.88f), (_e163 * 0.94f), _e165), vec3((_e167 * 0.55f)));
        base[0u] = _e170.x;
        base[1u] = _e170.y;
        base[2u] = _e170.z;
    }
    let _e177 = color0_;
    let _e180 = unnamed.emissionRadiance;
    let _e183 = base;
    let _e185 = (_e183.xyz + (_e177.xyz * _e180.xyz));
    base[0u] = _e185.x;
    base[1u] = _e185.y;
    base[2u] = _e185.z;
    let _e192 = wired_advanced_fog_enabled_u0028_();
    if _e192 {
        let _e193 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e193;
        let _e194 = base;
        let _e197 = unnamed.advancedFogColorDensity;
        let _e199 = fogAmount;
        let _e201 = mix(_e194.xyz, _e197.xyz, vec3(_e199));
        base[0u] = _e201.x;
        base[1u] = _e201.y;
        base[2u] = _e201.z;
    }
    if override_type_4_3 {
        let _e209 = base[3u];
        if (_e209 == 0f) {
            discard;
        }
    } else {
        if override_type_4_4 {
            let _e211 = base;
            let _e213 = base;
            if (dot(_e211.xyz, _e213.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e217 = base;
    out_color = _e217;
    param_14 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_14));
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
