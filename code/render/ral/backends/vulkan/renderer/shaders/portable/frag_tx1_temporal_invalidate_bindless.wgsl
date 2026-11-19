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

    let _e68 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e68 + 0.5f));
    let _e73 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e75 = fogType;
    let _e78 = fogType;
    return (((_e73 > 0.5f) && (_e75 >= 1i)) && (_e78 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e68 = wired_advanced_fog_enabled_u0028_();
    if !(_e68) {
        return 0f;
    }
    let _e71 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e71, 0.000001f));
    let _e76 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e76 + 0.5f));
    let _e79 = fogType_1;
    if (_e79 == 1i) {
        let _e83 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e83 <= 0f) {
            return 0f;
        }
        let _e85 = viewDepth;
        let _e88 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e85 / _e88), 0f, 1f);
    }
    let _e93 = unnamed.advancedFogColorDensity[3u];
    let _e95 = viewDepth;
    opticalDepth = (max(_e93, 0f) * _e95);
    let _e97 = fogType_1;
    if (_e97 == 2i) {
        let _e99 = opticalDepth;
        return clamp((1f - exp(-(_e99))), 0f, 1f);
    }
    let _e104 = opticalDepth;
    let _e105 = opticalDepth;
    return clamp((1f - exp(-((_e104 * _e105)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e68 = (*rgb);
    let _e71 = unnamed.worldLightParams[0u];
    boosted = (_e68 * _e71);
    let _e74 = boosted[0u];
    let _e76 = boosted[1u];
    let _e78 = boosted[2u];
    peak = max(_e74, max(_e76, _e78));
    let _e81 = peak;
    if (_e81 > 1f) {
        let _e83 = peak;
        let _e84 = boosted;
        boosted = (_e84 / vec3(_e83));
    }
    let _e87 = boosted;
    return _e87;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e69 = (*c);
    (*c) = max(_e69, vec3<f32>(0f, 0f, 0f));
    let _e71 = (*c);
    cutoff = (_e71 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e73 = (*c);
    lo = (_e73 / vec3(12.92f));
    let _e76 = (*c);
    hi = pow(((_e76 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e81 = hi;
    let _e82 = lo;
    let _e83 = cutoff;
    return mix(_e81, _e82, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e83));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e71 = (*role);
    let _e73 = (*role);
    let _e78 = unnamed.packed_indices[(_e71 / 4u)][(_e73 % 4u)];
    let _e81 = (*role);
    let _e83 = (*role);
    let _e88 = unnamed.packed_indices[(_e81 / 4u)][(_e83 % 4u)];
    let _e93 = (*uv);
    let _e94 = textureSample(wired_bindless_images[(_e78 & 4095u)], wired_bindless_samplers[((_e88 >> bitcast<u32>(12i)) & 255u)], _e93);
    c_1 = _e94;
    let _e95 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e95))) == 0i) {
        let _e100 = c_1;
        param = _e100.xyz;
        let _e102 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e102.x;
        c_1[1u] = _e102.y;
        c_1[2u] = _e102.z;
    }
    let _e109 = (*slot);
    if (lightmap_slot == (_e109 + 1i)) {
        let _e112 = c_1;
        param_1 = _e112.xyz;
        let _e114 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e114.x;
        c_1[1u] = _e114.y;
        c_1[2u] = _e114.z;
    }
    let _e121 = c_1;
    return _e121;
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
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_9: f32;

    let _e85 = frag_color0In_1;
    param_2 = _e85.xyz;
    let _e87 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e89 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e87.x, _e87.y, _e87.z, _e89);
    param_3 = 0u;
    let _e94 = frag_tex_coord0_1;
    param_4 = _e94;
    param_5 = 0i;
    let _e95 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e95;
    param_6 = 1u;
    let _e96 = frag_tex_coord1_1;
    param_7 = _e96;
    param_8 = 1i;
    let _e97 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    sampledColor1_ = _e97;
    let _e98 = sampledColor0_;
    let _e99 = frag_color0_;
    color0_ = (_e98 * _e99);
    if override_type_4_ {
        let _e101 = sampledColor1_;
        color1_ = _e101;
        let _e102 = color0_;
        let _e104 = color1_;
        let _e106 = (_e102.xyz + _e104.xyz);
        let _e108 = color0_[3u];
        let _e110 = color1_[3u];
        base = vec4<f32>(_e106.x, _e106.y, _e106.z, (_e108 * _e110));
    } else {
        if override_type_4_1 {
            let _e116 = sampledColor1_;
            let _e117 = frag_color0_;
            color1_1 = (_e116 * _e117);
            let _e119 = color0_;
            let _e121 = color1_1;
            let _e123 = (_e119.xyz + _e121.xyz);
            let _e125 = color0_[3u];
            let _e127 = color1_1[3u];
            base = vec4<f32>(_e123.x, _e123.y, _e123.z, (_e125 * _e127));
        } else {
            let _e133 = sampledColor1_;
            color1_2 = _e133;
            let _e134 = color0_;
            let _e136 = color1_2;
            let _e138 = (_e134.xyz * _e136.xyz);
            base[0u] = _e138.x;
            base[1u] = _e138.y;
            base[2u] = _e138.z;
            let _e146 = color0_[3u];
            let _e148 = color1_2[3u];
            base[3u] = (_e146 * _e148);
        }
    }
    if override_type_4_2 {
        let _e153 = unnamed.worldLightParams[1u];
        wetness = clamp(_e153, 0f, 1f);
        let _e157 = unnamed.worldLightParams[2u];
        frost = clamp(_e157, 0f, 1f);
        let _e159 = base;
        luminance = dot(_e159.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e162 = wetness;
        let _e164 = base;
        let _e166 = (_e164.xyz * mix(1f, 0.82f, _e162));
        base[0u] = _e166.x;
        base[1u] = _e166.y;
        base[2u] = _e166.z;
        let _e173 = base;
        let _e175 = luminance;
        let _e177 = luminance;
        let _e179 = luminance;
        let _e181 = frost;
        let _e184 = mix(_e173.xyz, vec3<f32>((_e175 * 0.88f), (_e177 * 0.94f), _e179), vec3((_e181 * 0.55f)));
        base[0u] = _e184.x;
        base[1u] = _e184.y;
        base[2u] = _e184.z;
    }
    let _e193 = unnamed.materialFactors[0u];
    if (_e193 >= 0f) {
        let _e196 = unnamed.materialBaseColor;
        let _e197 = base;
        base = (_e197 * _e196);
    }
    let _e199 = color0_;
    let _e202 = unnamed.emissionRadiance;
    let _e205 = base;
    let _e207 = (_e205.xyz + (_e199.xyz * _e202.xyz));
    base[0u] = _e207.x;
    base[1u] = _e207.y;
    base[2u] = _e207.z;
    let _e214 = wired_advanced_fog_enabled_u0028_();
    if _e214 {
        let _e215 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e215;
        let _e216 = base;
        let _e219 = unnamed.advancedFogColorDensity;
        let _e221 = fogAmount;
        let _e223 = mix(_e216.xyz, _e219.xyz, vec3(_e221));
        base[0u] = _e223.x;
        base[1u] = _e223.y;
        base[2u] = _e223.z;
    }
    if override_type_4_3 {
        let _e231 = base[3u];
        if (_e231 == 0f) {
            discard;
        }
    } else {
        if override_type_4_4 {
            let _e233 = base;
            let _e235 = base;
            if (dot(_e233.xyz, _e235.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e239 = base;
    out_color = _e239;
    param_9 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_9));
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
