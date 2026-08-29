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
var<private> frag_color0In_1: vec4<f32>;
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

    let _e66 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e66 + 0.5f));
    let _e71 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e73 = fogType;
    let _e76 = fogType;
    return (((_e71 > 0.5f) && (_e73 >= 1i)) && (_e76 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e66 = wired_advanced_fog_enabled_u0028_();
    if !(_e66) {
        return 0f;
    }
    let _e69 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e69, 0.000001f));
    let _e74 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e74 + 0.5f));
    let _e77 = fogType_1;
    if (_e77 == 1i) {
        let _e81 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e81 <= 0f) {
            return 0f;
        }
        let _e83 = viewDepth;
        let _e86 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e83 / _e86), 0f, 1f);
    }
    let _e91 = unnamed.advancedFogColorDensity[3u];
    let _e93 = viewDepth;
    opticalDepth = (max(_e91, 0f) * _e93);
    let _e95 = fogType_1;
    if (_e95 == 2i) {
        let _e97 = opticalDepth;
        return clamp((1f - exp(-(_e97))), 0f, 1f);
    }
    let _e102 = opticalDepth;
    let _e103 = opticalDepth;
    return clamp((1f - exp(-((_e102 * _e103)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e67 = (*c);
    (*c) = max(_e67, vec3<f32>(0f, 0f, 0f));
    let _e69 = (*c);
    cutoff = (_e69 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e71 = (*c);
    lo = (_e71 / vec3(12.92f));
    let _e74 = (*c);
    hi = pow(((_e74 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e79 = hi;
    let _e80 = lo;
    let _e81 = cutoff;
    return mix(_e79, _e80, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e81));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

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
        let _e111 = unnamed.worldLightParams[0u];
        let _e112 = c_1;
        let _e114 = (_e112.xyz * _e111);
        c_1[0u] = _e114.x;
        c_1[1u] = _e114.y;
        c_1[2u] = _e114.z;
    }
    let _e121 = c_1;
    return _e121;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
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

    let _e87 = frag_color0In_1;
    param_1 = _e87.xyz;
    let _e89 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e91 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e89.x, _e89.y, _e89.z, _e91);
    param_2 = 0u;
    let _e96 = frag_tex_coord0_1;
    param_3 = _e96;
    param_4 = 0i;
    let _e97 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e98 = frag_color0_;
    color0_ = (_e97 * _e98);
    if override_type_4_ {
        param_5 = 1u;
        let _e100 = frag_tex_coord1_1;
        param_6 = _e100;
        param_7 = 1i;
        let _e101 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e101;
        let _e102 = color0_;
        let _e104 = color1_;
        let _e106 = (_e102.xyz + _e104.xyz);
        let _e108 = color0_[3u];
        let _e110 = color1_[3u];
        base = vec4<f32>(_e106.x, _e106.y, _e106.z, (_e108 * _e110));
    } else {
        if override_type_4_1 {
            param_8 = 1u;
            let _e116 = frag_tex_coord1_1;
            param_9 = _e116;
            param_10 = 1i;
            let _e117 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            let _e118 = frag_color0_;
            color1_1 = (_e117 * _e118);
            let _e120 = color0_;
            let _e122 = color1_1;
            let _e124 = (_e120.xyz + _e122.xyz);
            let _e126 = color0_[3u];
            let _e128 = color1_1[3u];
            base = vec4<f32>(_e124.x, _e124.y, _e124.z, (_e126 * _e128));
        } else {
            param_11 = 1u;
            let _e134 = frag_tex_coord1_1;
            param_12 = _e134;
            param_13 = 1i;
            let _e135 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e135;
            let _e136 = color0_;
            let _e138 = color1_2;
            let _e140 = (_e136.xyz * _e138.xyz);
            base[0u] = _e140.x;
            base[1u] = _e140.y;
            base[2u] = _e140.z;
            let _e148 = color0_[3u];
            let _e150 = color1_2[3u];
            base[3u] = (_e148 * _e150);
        }
    }
    if override_type_4_2 {
        let _e155 = unnamed.worldLightParams[1u];
        wetness = clamp(_e155, 0f, 1f);
        let _e159 = unnamed.worldLightParams[2u];
        frost = clamp(_e159, 0f, 1f);
        let _e161 = base;
        luminance = dot(_e161.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e164 = wetness;
        let _e166 = base;
        let _e168 = (_e166.xyz * mix(1f, 0.82f, _e164));
        base[0u] = _e168.x;
        base[1u] = _e168.y;
        base[2u] = _e168.z;
        let _e175 = base;
        let _e177 = luminance;
        let _e179 = luminance;
        let _e181 = luminance;
        let _e183 = frost;
        let _e186 = mix(_e175.xyz, vec3<f32>((_e177 * 0.88f), (_e179 * 0.94f), _e181), vec3((_e183 * 0.55f)));
        base[0u] = _e186.x;
        base[1u] = _e186.y;
        base[2u] = _e186.z;
    }
    let _e193 = color0_;
    let _e196 = unnamed.emissionRadiance;
    let _e199 = base;
    let _e201 = (_e199.xyz + (_e193.xyz * _e196.xyz));
    base[0u] = _e201.x;
    base[1u] = _e201.y;
    base[2u] = _e201.z;
    let _e208 = wired_advanced_fog_enabled_u0028_();
    if _e208 {
        let _e209 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e209;
        let _e210 = base;
        let _e213 = unnamed.advancedFogColorDensity;
        let _e215 = fogAmount;
        let _e217 = mix(_e210.xyz, _e213.xyz, vec3(_e215));
        base[0u] = _e217.x;
        base[1u] = _e217.y;
        base[2u] = _e217.z;
    }
    if override_type_4_3 {
        let _e225 = base[3u];
        if (_e225 == 0f) {
            discard;
        }
    } else {
        if override_type_4_4 {
            let _e227 = base;
            let _e229 = base;
            if (dot(_e227.xyz, _e229.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e233 = base;
    out_color = _e233;
    param_14 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_14));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
