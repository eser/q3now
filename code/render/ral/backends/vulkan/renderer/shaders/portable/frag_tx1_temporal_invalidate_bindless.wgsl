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
    @location(2) member_1: vec4<f32>,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_6_: bool = (tex_mode == 1i);
override override_type_6_1: bool = (tex_mode == 2i);
override override_type_6_2: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_6_3: bool = (discard_mode == 1i);
override override_type_6_4: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: vec4<f32>;
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

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e69 = (*sceneColor)[0u];
    let _e71 = (*sceneColor)[1u];
    let _e73 = (*sceneColor)[2u];
    let _e78 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e69, max(_e71, _e73)), 0f, 1f), 0f, 0f, clamp(_e78, 0f, 1f));
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
    var param_10: vec4<f32>;

    let _e87 = frag_color0In_1;
    param_2 = _e87.xyz;
    let _e89 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e91 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e89.x, _e89.y, _e89.z, _e91);
    param_3 = 0u;
    let _e96 = frag_tex_coord0_1;
    param_4 = _e96;
    param_5 = 0i;
    let _e97 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e97;
    param_6 = 1u;
    let _e98 = frag_tex_coord1_1;
    param_7 = _e98;
    param_8 = 1i;
    let _e99 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    sampledColor1_ = _e99;
    let _e100 = sampledColor0_;
    let _e101 = frag_color0_;
    color0_ = (_e100 * _e101);
    if override_type_6_ {
        let _e103 = sampledColor1_;
        color1_ = _e103;
        let _e104 = color0_;
        let _e106 = color1_;
        let _e108 = (_e104.xyz + _e106.xyz);
        let _e110 = color0_[3u];
        let _e112 = color1_[3u];
        base = vec4<f32>(_e108.x, _e108.y, _e108.z, (_e110 * _e112));
    } else {
        if override_type_6_1 {
            let _e118 = sampledColor1_;
            let _e119 = frag_color0_;
            color1_1 = (_e118 * _e119);
            let _e121 = color0_;
            let _e123 = color1_1;
            let _e125 = (_e121.xyz + _e123.xyz);
            let _e127 = color0_[3u];
            let _e129 = color1_1[3u];
            base = vec4<f32>(_e125.x, _e125.y, _e125.z, (_e127 * _e129));
        } else {
            let _e135 = sampledColor1_;
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
    if override_type_6_2 {
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
    let _e195 = unnamed.materialFactors[0u];
    if (_e195 >= 0f) {
        let _e198 = unnamed.materialBaseColor;
        let _e199 = base;
        base = (_e199 * _e198);
    }
    let _e201 = color0_;
    let _e204 = unnamed.emissionRadiance;
    let _e207 = base;
    let _e209 = (_e207.xyz + (_e201.xyz * _e204.xyz));
    base[0u] = _e209.x;
    base[1u] = _e209.y;
    base[2u] = _e209.z;
    let _e216 = wired_advanced_fog_enabled_u0028_();
    if _e216 {
        let _e217 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e217;
        let _e218 = base;
        let _e221 = unnamed.advancedFogColorDensity;
        let _e223 = fogAmount;
        let _e225 = mix(_e218.xyz, _e221.xyz, vec3(_e223));
        base[0u] = _e225.x;
        base[1u] = _e225.y;
        base[2u] = _e225.z;
    }
    if override_type_6_3 {
        let _e233 = base[3u];
        if (_e233 == 0f) {
            discard;
        }
    } else {
        if override_type_6_4 {
            let _e235 = base;
            let _e237 = base;
            if (dot(_e235.xyz, _e237.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e241 = base;
    out_color = _e241;
    param_9 = 1f;
    let _e242 = base;
    param_10 = _e242;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_9), (&param_10));
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
