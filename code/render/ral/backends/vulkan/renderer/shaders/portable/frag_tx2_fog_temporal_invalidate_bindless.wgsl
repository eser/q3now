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
@id(10) override acff: i32 = 0i;
override override_type_4_3: bool = (acff == 1i);
override override_type_4_4: bool = (acff == 2i);
override override_type_4_5: bool = (acff == 3i);
override override_type_4_6: bool = (acff == 1i);
override override_type_4_7: bool = (acff == 2i);
override override_type_4_8: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_9: bool = (discard_mode == 1i);
override override_type_4_10: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
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

    let _e78 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e78 + 0.5f));
    let _e83 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e85 = fogType;
    let _e88 = fogType;
    return (((_e83 > 0.5f) && (_e85 >= 1i)) && (_e88 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e78 = wired_advanced_fog_enabled_u0028_();
    if !(_e78) {
        return 0f;
    }
    let _e81 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e81, 0.000001f));
    let _e86 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e86 + 0.5f));
    let _e89 = fogType_1;
    if (_e89 == 1i) {
        let _e93 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e93 <= 0f) {
            return 0f;
        }
        let _e95 = viewDepth;
        let _e98 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e95 / _e98), 0f, 1f);
    }
    let _e103 = unnamed.advancedFogColorDensity[3u];
    let _e105 = viewDepth;
    opticalDepth = (max(_e103, 0f) * _e105);
    let _e107 = fogType_1;
    if (_e107 == 2i) {
        let _e109 = opticalDepth;
        return clamp((1f - exp(-(_e109))), 0f, 1f);
    }
    let _e114 = opticalDepth;
    let _e115 = opticalDepth;
    return clamp((1f - exp(-((_e114 * _e115)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e78 = (*rgb);
    let _e81 = unnamed.worldLightParams[0u];
    boosted = (_e78 * _e81);
    let _e84 = boosted[0u];
    let _e86 = boosted[1u];
    let _e88 = boosted[2u];
    peak = max(_e84, max(_e86, _e88));
    let _e91 = peak;
    if (_e91 > 1f) {
        let _e93 = peak;
        let _e94 = boosted;
        boosted = (_e94 / vec3(_e93));
    }
    let _e97 = boosted;
    return _e97;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e79 = (*c);
    (*c) = max(_e79, vec3<f32>(0f, 0f, 0f));
    let _e81 = (*c);
    cutoff = (_e81 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e83 = (*c);
    lo = (_e83 / vec3(12.92f));
    let _e86 = (*c);
    hi = pow(((_e86 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e91 = hi;
    let _e92 = lo;
    let _e93 = cutoff;
    return mix(_e91, _e92, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e93));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e81 = (*role);
    let _e83 = (*role);
    let _e88 = unnamed.packed_indices[(_e81 / 4u)][(_e83 % 4u)];
    let _e91 = (*role);
    let _e93 = (*role);
    let _e98 = unnamed.packed_indices[(_e91 / 4u)][(_e93 % 4u)];
    let _e103 = (*uv);
    let _e104 = textureSample(wired_bindless_images[(_e88 & 4095u)], wired_bindless_samplers[((_e98 >> bitcast<u32>(12i)) & 255u)], _e103);
    c_1 = _e104;
    let _e105 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e105))) == 0i) {
        let _e110 = c_1;
        param = _e110.xyz;
        let _e112 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e112.x;
        c_1[1u] = _e112.y;
        c_1[2u] = _e112.z;
    }
    let _e119 = (*slot);
    if (lightmap_slot == (_e119 + 1i)) {
        let _e122 = c_1;
        param_1 = _e122.xyz;
        let _e124 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e124.x;
        c_1[1u] = _e124.y;
        c_1[2u] = _e124.z;
    }
    let _e131 = c_1;
    return _e131;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e106 = unnamed.packed_indices[0i][3u];
    let _e112 = unnamed.packed_indices[0i][3u];
    let _e117 = fog_tex_coord_1;
    let _e118 = textureSample(wired_bindless_images[(_e106 & 4095u)], wired_bindless_samplers[((_e112 >> bitcast<u32>(12i)) & 255u)], _e117);
    fog = _e118;
    let _e119 = frag_color0In_1;
    param_2 = _e119.xyz;
    let _e121 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e123 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e121.x, _e121.y, _e121.z, _e123);
    param_3 = 0u;
    let _e128 = frag_tex_coord0_1;
    param_4 = _e128;
    param_5 = 0i;
    let _e129 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e129;
    param_6 = 1u;
    let _e130 = frag_tex_coord1_1;
    param_7 = _e130;
    param_8 = 1i;
    let _e131 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    sampledColor1_ = _e131;
    param_9 = 2u;
    let _e132 = frag_tex_coord2_1;
    param_10 = _e132;
    param_11 = 2i;
    let _e133 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    sampledColor2_ = _e133;
    let _e134 = sampledColor0_;
    let _e135 = frag_color0_;
    color0_ = (_e134 * _e135);
    if override_type_4_ {
        let _e137 = sampledColor1_;
        color1_ = _e137;
        let _e138 = sampledColor2_;
        color2_ = _e138;
        let _e139 = color0_;
        let _e141 = color1_;
        let _e144 = color2_;
        let _e146 = ((_e139.xyz + _e141.xyz) + _e144.xyz);
        let _e148 = color0_[3u];
        let _e150 = color1_[3u];
        let _e153 = color2_[3u];
        base = vec4<f32>(_e146.x, _e146.y, _e146.z, ((_e148 * _e150) * _e153));
    } else {
        if override_type_4_1 {
            let _e159 = sampledColor1_;
            let _e160 = frag_color0_;
            color1_1 = (_e159 * _e160);
            let _e162 = sampledColor2_;
            let _e163 = frag_color0_;
            color2_1 = (_e162 * _e163);
            let _e165 = color0_;
            let _e167 = color1_1;
            let _e170 = color2_1;
            let _e172 = ((_e165.xyz + _e167.xyz) + _e170.xyz);
            let _e174 = color0_[3u];
            let _e176 = color1_1[3u];
            let _e179 = color2_1[3u];
            base = vec4<f32>(_e172.x, _e172.y, _e172.z, ((_e174 * _e176) * _e179));
        } else {
            let _e185 = sampledColor1_;
            color1_2 = _e185;
            let _e186 = sampledColor2_;
            color2_2 = _e186;
            let _e187 = color0_;
            let _e189 = color1_2;
            let _e192 = color2_2;
            let _e194 = ((_e187.xyz * _e189.xyz) * _e192.xyz);
            base[0u] = _e194.x;
            base[1u] = _e194.y;
            base[2u] = _e194.z;
            let _e202 = color0_[3u];
            let _e204 = color1_2[3u];
            let _e207 = color2_2[3u];
            base[3u] = ((_e202 * _e204) * _e207);
        }
    }
    if override_type_4_2 {
        let _e212 = unnamed.worldLightParams[1u];
        wetness = clamp(_e212, 0f, 1f);
        let _e216 = unnamed.worldLightParams[2u];
        frost = clamp(_e216, 0f, 1f);
        let _e218 = base;
        luminance = dot(_e218.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e221 = wetness;
        let _e223 = base;
        let _e225 = (_e223.xyz * mix(1f, 0.82f, _e221));
        base[0u] = _e225.x;
        base[1u] = _e225.y;
        base[2u] = _e225.z;
        let _e232 = base;
        let _e234 = luminance;
        let _e236 = luminance;
        let _e238 = luminance;
        let _e240 = frost;
        let _e243 = mix(_e232.xyz, vec3<f32>((_e234 * 0.88f), (_e236 * 0.94f), _e238), vec3((_e240 * 0.55f)));
        base[0u] = _e243.x;
        base[1u] = _e243.y;
        base[2u] = _e243.z;
    }
    let _e252 = unnamed.materialFactors[0u];
    if (_e252 >= 0f) {
        let _e255 = unnamed.materialBaseColor;
        let _e256 = base;
        base = (_e256 * _e255);
    }
    let _e258 = color0_;
    let _e261 = unnamed.emissionRadiance;
    let _e264 = base;
    let _e266 = (_e264.xyz + (_e258.xyz * _e261.xyz));
    base[0u] = _e266.x;
    base[1u] = _e266.y;
    base[2u] = _e266.z;
    let _e273 = wired_advanced_fog_enabled_u0028_();
    if _e273 {
        let _e274 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e274;
        if override_type_4_3 {
            let _e275 = fogAmount;
            let _e277 = base;
            let _e279 = (_e277.xyz * (1f - _e275));
            base[0u] = _e279.x;
            base[1u] = _e279.y;
            base[2u] = _e279.z;
        } else {
            if override_type_4_4 {
                let _e286 = fogAmount;
                let _e288 = base;
                base = (_e288 * (1f - _e286));
            } else {
                if override_type_4_5 {
                    let _e290 = fogAmount;
                    let _e293 = base[3u];
                    base[3u] = (_e293 * (1f - _e290));
                } else {
                    let _e296 = base;
                    let _e299 = unnamed.advancedFogColorDensity;
                    let _e301 = fogAmount;
                    let _e303 = mix(_e296.xyz, _e299.xyz, vec3(_e301));
                    base[0u] = _e303.x;
                    base[1u] = _e303.y;
                    base[2u] = _e303.z;
                }
            }
        }
    } else {
        if override_type_4_6 {
            let _e310 = base;
            let _e313 = fog[3u];
            let _e315 = (_e310.xyz * (1f - _e313));
            base[0u] = _e315.x;
            base[1u] = _e315.y;
            base[2u] = _e315.z;
        } else {
            if override_type_4_7 {
                let _e322 = base;
                let _e324 = fog[3u];
                base = (_e322 * (1f - _e324));
            } else {
                if override_type_4_8 {
                    let _e328 = base[3u];
                    let _e330 = fog[3u];
                    base[3u] = (_e328 * (1f - _e330));
                } else {
                    let _e334 = base;
                    let _e335 = fog;
                    let _e337 = unnamed.fogColor;
                    let _e340 = fog[3u];
                    base = mix(_e334, (_e335 * _e337), vec4(_e340));
                }
            }
        }
    }
    if override_type_4_9 {
        let _e344 = base[3u];
        if (_e344 == 0f) {
            discard;
        }
    } else {
        if override_type_4_10 {
            let _e346 = base;
            let _e348 = base;
            if (dot(_e346.xyz, _e348.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e352 = base;
    out_color = _e352;
    param_12 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_12));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
