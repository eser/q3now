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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
@id(6) override tex_mode: i32 = 0i;
override override_type_6_: bool = (tex_mode == 1i);
override override_type_6_1: bool = (tex_mode == 2i);
override override_type_6_2: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_6_3: bool = (acff == 1i);
override override_type_6_4: bool = (acff == 2i);
override override_type_6_5: bool = (acff == 3i);
override override_type_6_6: bool = (acff == 1i);
override override_type_6_7: bool = (acff == 2i);
override override_type_6_8: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_6_9: bool = (discard_mode == 1i);
override override_type_6_10: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e79 = (*sceneColor)[0u];
    let _e81 = (*sceneColor)[1u];
    let _e83 = (*sceneColor)[2u];
    let _e88 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e79, max(_e81, _e83)), 0f, 1f), 0f, 0f, clamp(_e88, 0f, 1f));
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e79 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e79 + 0.5f));
    let _e84 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e86 = fogType;
    let _e89 = fogType;
    return (((_e84 > 0.5f) && (_e86 >= 1i)) && (_e89 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e79 = wired_advanced_fog_enabled_u0028_();
    if !(_e79) {
        return 0f;
    }
    let _e82 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e82, 0.000001f));
    let _e87 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e87 + 0.5f));
    let _e90 = fogType_1;
    if (_e90 == 1i) {
        let _e94 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e94 <= 0f) {
            return 0f;
        }
        let _e96 = viewDepth;
        let _e99 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e96 / _e99), 0f, 1f);
    }
    let _e104 = unnamed.advancedFogColorDensity[3u];
    let _e106 = viewDepth;
    opticalDepth = (max(_e104, 0f) * _e106);
    let _e108 = fogType_1;
    if (_e108 == 2i) {
        let _e110 = opticalDepth;
        return clamp((1f - exp(-(_e110))), 0f, 1f);
    }
    let _e115 = opticalDepth;
    let _e116 = opticalDepth;
    return clamp((1f - exp(-((_e115 * _e116)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e79 = (*rgb);
    let _e82 = unnamed.worldLightParams[0u];
    boosted = (_e79 * _e82);
    let _e85 = boosted[0u];
    let _e87 = boosted[1u];
    let _e89 = boosted[2u];
    peak = max(_e85, max(_e87, _e89));
    let _e92 = peak;
    if (_e92 > 1f) {
        let _e94 = peak;
        let _e95 = boosted;
        boosted = (_e95 / vec3(_e94));
    }
    let _e98 = boosted;
    return _e98;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e80 = (*c);
    (*c) = max(_e80, vec3<f32>(0f, 0f, 0f));
    let _e82 = (*c);
    cutoff = (_e82 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e84 = (*c);
    lo = (_e84 / vec3(12.92f));
    let _e87 = (*c);
    hi = pow(((_e87 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e92 = hi;
    let _e93 = lo;
    let _e94 = cutoff;
    return mix(_e92, _e93, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e94));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e82 = (*role);
    let _e84 = (*role);
    let _e89 = unnamed.packed_indices[(_e82 / 4u)][(_e84 % 4u)];
    let _e92 = (*role);
    let _e94 = (*role);
    let _e99 = unnamed.packed_indices[(_e92 / 4u)][(_e94 % 4u)];
    let _e104 = (*uv);
    let _e105 = textureSample(wired_bindless_images[(_e89 & 4095u)], wired_bindless_samplers[((_e99 >> bitcast<u32>(12i)) & 255u)], _e104);
    c_1 = _e105;
    let _e106 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e106))) == 0i) {
        let _e111 = c_1;
        param = _e111.xyz;
        let _e113 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e113.x;
        c_1[1u] = _e113.y;
        c_1[2u] = _e113.z;
    }
    let _e120 = (*slot);
    if (lightmap_slot == (_e120 + 1i)) {
        let _e123 = c_1;
        param_1 = _e123.xyz;
        let _e125 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e125.x;
        c_1[1u] = _e125.y;
        c_1[2u] = _e125.z;
    }
    let _e132 = c_1;
    return _e132;
}

fn main_1() {
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var sampledColor1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var frag_color: vec4<f32>;
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
    var param_9: vec4<f32>;

    let _e100 = unnamed.packed_indices[0i][3u];
    let _e106 = unnamed.packed_indices[0i][3u];
    let _e111 = fog_tex_coord_1;
    let _e112 = textureSample(wired_bindless_images[(_e100 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    fog = _e112;
    param_2 = 0u;
    let _e113 = frag_tex_coord0_1;
    param_3 = _e113;
    param_4 = 0i;
    let _e114 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e114;
    param_5 = 1u;
    let _e115 = frag_tex_coord1_1;
    param_6 = _e115;
    param_7 = 1i;
    let _e116 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor1_ = _e116;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e121 = sampledColor0_;
    let _e122 = frag_color;
    color0_ = (_e121 * _e122);
    if override_type_6_ {
        let _e124 = sampledColor1_;
        color1_ = _e124;
        let _e125 = color0_;
        let _e127 = color1_;
        let _e129 = (_e125.xyz + _e127.xyz);
        let _e131 = color0_[3u];
        let _e133 = color1_[3u];
        base = vec4<f32>(_e129.x, _e129.y, _e129.z, (_e131 * _e133));
    } else {
        if override_type_6_1 {
            let _e139 = sampledColor1_;
            let _e140 = frag_color;
            color1_1 = (_e139 * _e140);
            let _e142 = color0_;
            let _e144 = color1_1;
            let _e146 = (_e142.xyz + _e144.xyz);
            let _e148 = color0_[3u];
            let _e150 = color1_1[3u];
            base = vec4<f32>(_e146.x, _e146.y, _e146.z, (_e148 * _e150));
        } else {
            let _e156 = sampledColor1_;
            let _e157 = frag_color;
            color1_2 = (_e156 * _e157);
            let _e159 = color0_;
            let _e161 = color1_2;
            let _e163 = (_e159.xyz * _e161.xyz);
            base[0u] = _e163.x;
            base[1u] = _e163.y;
            base[2u] = _e163.z;
            let _e171 = color0_[3u];
            let _e173 = color1_2[3u];
            base[3u] = (_e171 * _e173);
        }
    }
    if override_type_6_2 {
        let _e178 = unnamed.worldLightParams[1u];
        wetness = clamp(_e178, 0f, 1f);
        let _e182 = unnamed.worldLightParams[2u];
        frost = clamp(_e182, 0f, 1f);
        let _e184 = base;
        luminance = dot(_e184.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e187 = wetness;
        let _e189 = base;
        let _e191 = (_e189.xyz * mix(1f, 0.82f, _e187));
        base[0u] = _e191.x;
        base[1u] = _e191.y;
        base[2u] = _e191.z;
        let _e198 = base;
        let _e200 = luminance;
        let _e202 = luminance;
        let _e204 = luminance;
        let _e206 = frost;
        let _e209 = mix(_e198.xyz, vec3<f32>((_e200 * 0.88f), (_e202 * 0.94f), _e204), vec3((_e206 * 0.55f)));
        base[0u] = _e209.x;
        base[1u] = _e209.y;
        base[2u] = _e209.z;
    }
    let _e218 = unnamed.materialFactors[0u];
    if (_e218 >= 0f) {
        let _e221 = unnamed.materialBaseColor;
        let _e222 = base;
        base = (_e222 * _e221);
    }
    let _e224 = color0_;
    let _e227 = unnamed.emissionRadiance;
    let _e230 = base;
    let _e232 = (_e230.xyz + (_e224.xyz * _e227.xyz));
    base[0u] = _e232.x;
    base[1u] = _e232.y;
    base[2u] = _e232.z;
    let _e239 = wired_advanced_fog_enabled_u0028_();
    if _e239 {
        let _e240 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e240;
        if override_type_6_3 {
            let _e241 = fogAmount;
            let _e243 = base;
            let _e245 = (_e243.xyz * (1f - _e241));
            base[0u] = _e245.x;
            base[1u] = _e245.y;
            base[2u] = _e245.z;
        } else {
            if override_type_6_4 {
                let _e252 = fogAmount;
                let _e254 = base;
                base = (_e254 * (1f - _e252));
            } else {
                if override_type_6_5 {
                    let _e256 = fogAmount;
                    let _e259 = base[3u];
                    base[3u] = (_e259 * (1f - _e256));
                } else {
                    let _e262 = base;
                    let _e265 = unnamed.advancedFogColorDensity;
                    let _e267 = fogAmount;
                    let _e269 = mix(_e262.xyz, _e265.xyz, vec3(_e267));
                    base[0u] = _e269.x;
                    base[1u] = _e269.y;
                    base[2u] = _e269.z;
                }
            }
        }
    } else {
        if override_type_6_6 {
            let _e276 = base;
            let _e279 = fog[3u];
            let _e281 = (_e276.xyz * (1f - _e279));
            base[0u] = _e281.x;
            base[1u] = _e281.y;
            base[2u] = _e281.z;
        } else {
            if override_type_6_7 {
                let _e288 = base;
                let _e290 = fog[3u];
                base = (_e288 * (1f - _e290));
            } else {
                if override_type_6_8 {
                    let _e294 = base[3u];
                    let _e296 = fog[3u];
                    base[3u] = (_e294 * (1f - _e296));
                } else {
                    let _e300 = base;
                    let _e301 = fog;
                    let _e303 = unnamed.fogColor;
                    let _e306 = fog[3u];
                    base = mix(_e300, (_e301 * _e303), vec4(_e306));
                }
            }
        }
    }
    if override_type_6_9 {
        let _e310 = base[3u];
        if (_e310 == 0f) {
            discard;
        }
    } else {
        if override_type_6_10 {
            let _e312 = base;
            let _e314 = base;
            if (dot(_e312.xyz, _e314.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e318 = base;
    out_color = _e318;
    param_8 = 1f;
    let _e319 = base;
    param_9 = _e319;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_8), (&param_9));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
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
