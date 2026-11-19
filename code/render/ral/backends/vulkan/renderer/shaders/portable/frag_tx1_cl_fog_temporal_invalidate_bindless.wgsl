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
override override_type_4_2: bool = (override_type_4_ || override_type_4_1);
override override_type_4_3: bool = (tex_mode == 3i);
override override_type_4_4: bool = (tex_mode == 4i);
override override_type_4_5: bool = (tex_mode == 5i);
override override_type_4_6: bool = (tex_mode == 6i);
override override_type_4_7: bool = (tex_mode == 7i);
override override_type_4_8: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_4_9: bool = (acff == 1i);
override override_type_4_10: bool = (acff == 2i);
override override_type_4_11: bool = (acff == 3i);
override override_type_4_12: bool = (acff == 1i);
override override_type_4_13: bool = (acff == 2i);
override override_type_4_14: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_15: bool = (discard_mode == 1i);
override override_type_4_16: bool = (discard_mode == 2i);
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
var<private> frag_color1In_1: vec4<f32>;
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

    let _e87 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e87 + 0.5f));
    let _e92 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e94 = fogType;
    let _e97 = fogType;
    return (((_e92 > 0.5f) && (_e94 >= 1i)) && (_e97 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e87 = wired_advanced_fog_enabled_u0028_();
    if !(_e87) {
        return 0f;
    }
    let _e90 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e90, 0.000001f));
    let _e95 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e95 + 0.5f));
    let _e98 = fogType_1;
    if (_e98 == 1i) {
        let _e102 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e102 <= 0f) {
            return 0f;
        }
        let _e104 = viewDepth;
        let _e107 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e104 / _e107), 0f, 1f);
    }
    let _e112 = unnamed.advancedFogColorDensity[3u];
    let _e114 = viewDepth;
    opticalDepth = (max(_e112, 0f) * _e114);
    let _e116 = fogType_1;
    if (_e116 == 2i) {
        let _e118 = opticalDepth;
        return clamp((1f - exp(-(_e118))), 0f, 1f);
    }
    let _e123 = opticalDepth;
    let _e124 = opticalDepth;
    return clamp((1f - exp(-((_e123 * _e124)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e87 = (*rgb);
    let _e90 = unnamed.worldLightParams[0u];
    boosted = (_e87 * _e90);
    let _e93 = boosted[0u];
    let _e95 = boosted[1u];
    let _e97 = boosted[2u];
    peak = max(_e93, max(_e95, _e97));
    let _e100 = peak;
    if (_e100 > 1f) {
        let _e102 = peak;
        let _e103 = boosted;
        boosted = (_e103 / vec3(_e102));
    }
    let _e106 = boosted;
    return _e106;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e88 = (*c);
    (*c) = max(_e88, vec3<f32>(0f, 0f, 0f));
    let _e90 = (*c);
    cutoff = (_e90 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e92 = (*c);
    lo = (_e92 / vec3(12.92f));
    let _e95 = (*c);
    hi = pow(((_e95 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e100 = hi;
    let _e101 = lo;
    let _e102 = cutoff;
    return mix(_e100, _e101, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e102));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e90 = (*role);
    let _e92 = (*role);
    let _e97 = unnamed.packed_indices[(_e90 / 4u)][(_e92 % 4u)];
    let _e100 = (*role);
    let _e102 = (*role);
    let _e107 = unnamed.packed_indices[(_e100 / 4u)][(_e102 % 4u)];
    let _e112 = (*uv);
    let _e113 = textureSample(wired_bindless_images[(_e97 & 4095u)], wired_bindless_samplers[((_e107 >> bitcast<u32>(12i)) & 255u)], _e112);
    c_1 = _e113;
    let _e114 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e114))) == 0i) {
        let _e119 = c_1;
        param = _e119.xyz;
        let _e121 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e121.x;
        c_1[1u] = _e121.y;
        c_1[2u] = _e121.z;
    }
    let _e128 = (*slot);
    if (lightmap_slot == (_e128 + 1i)) {
        let _e131 = c_1;
        param_1 = _e131.xyz;
        let _e133 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e133.x;
        c_1[1u] = _e133.y;
        c_1[2u] = _e133.z;
    }
    let _e140 = c_1;
    return _e140;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_3: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var sampledColor1_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_10: f32;

    let _e114 = unnamed.packed_indices[0i][3u];
    let _e120 = unnamed.packed_indices[0i][3u];
    let _e125 = fog_tex_coord_1;
    let _e126 = textureSample(wired_bindless_images[(_e114 & 4095u)], wired_bindless_samplers[((_e120 >> bitcast<u32>(12i)) & 255u)], _e125);
    fog = _e126;
    let _e127 = frag_color0In_1;
    param_2 = _e127.xyz;
    let _e129 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e131 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e129.x, _e129.y, _e129.z, _e131);
    let _e136 = frag_color1In_1;
    param_3 = _e136.xyz;
    let _e138 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e140 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e138.x, _e138.y, _e138.z, _e140);
    param_4 = 0u;
    let _e145 = frag_tex_coord0_1;
    param_5 = _e145;
    param_6 = 0i;
    let _e146 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    sampledColor0_ = _e146;
    param_7 = 1u;
    let _e147 = frag_tex_coord1_1;
    param_8 = _e147;
    param_9 = 1i;
    let _e148 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor1_ = _e148;
    let _e149 = sampledColor0_;
    let _e150 = frag_color0_;
    color0_ = (_e149 * _e150);
    if override_type_4_2 {
        let _e152 = sampledColor1_;
        let _e153 = frag_color1_;
        color1_ = (_e152 * _e153);
        let _e155 = color0_;
        let _e157 = color1_;
        let _e159 = (_e155.xyz + _e157.xyz);
        let _e161 = color0_[3u];
        let _e163 = color1_[3u];
        base = vec4<f32>(_e159.x, _e159.y, _e159.z, (_e161 * _e163));
    } else {
        if override_type_4_3 {
            let _e169 = sampledColor1_;
            let _e170 = frag_color1_;
            color1_1 = (_e169 * _e170);
            let _e173 = color0_[3u];
            let _e174 = color0_;
            color0_ = (_e174 * _e173);
            let _e177 = color1_1[3u];
            let _e178 = color1_1;
            color1_1 = (_e178 * _e177);
            let _e180 = color0_;
            let _e182 = color1_1;
            let _e184 = (_e180.xyz + _e182.xyz);
            let _e186 = color0_[3u];
            let _e188 = color1_1[3u];
            base = vec4<f32>(_e184.x, _e184.y, _e184.z, (_e186 * _e188));
        } else {
            if override_type_4_4 {
                let _e194 = sampledColor1_;
                let _e195 = frag_color1_;
                color1_2 = (_e194 * _e195);
                let _e198 = color0_[3u];
                let _e200 = color0_;
                color0_ = (_e200 * (1f - _e198));
                let _e203 = color1_2[3u];
                let _e205 = color1_2;
                color1_2 = (_e205 * (1f - _e203));
                let _e207 = color0_;
                let _e209 = color1_2;
                let _e211 = (_e207.xyz + _e209.xyz);
                let _e213 = color0_[3u];
                let _e215 = color1_2[3u];
                base = vec4<f32>(_e211.x, _e211.y, _e211.z, (_e213 * _e215));
            } else {
                if override_type_4_5 {
                    let _e221 = sampledColor1_;
                    let _e222 = frag_color1_;
                    color1_3 = (_e221 * _e222);
                    let _e224 = color0_;
                    let _e225 = color1_3;
                    let _e227 = color1_3[3u];
                    base = mix(_e224, _e225, vec4(_e227));
                } else {
                    if override_type_4_6 {
                        let _e230 = sampledColor1_;
                        let _e231 = frag_color1_;
                        color1_4 = (_e230 * _e231);
                        let _e233 = color1_4;
                        let _e234 = color0_;
                        let _e236 = color1_4[3u];
                        base = mix(_e233, _e234, vec4(_e236));
                    } else {
                        if override_type_4_7 {
                            let _e239 = sampledColor1_;
                            let _e240 = frag_color1_;
                            color1_5 = (_e239 * _e240);
                            let _e242 = color1_5;
                            let _e244 = color1_5[3u];
                            let _e247 = color0_;
                            base = ((_e242 + vec4(_e244)) * _e247);
                        } else {
                            let _e249 = sampledColor1_;
                            let _e250 = frag_color1_;
                            color1_6 = (_e249 * _e250);
                            let _e252 = color0_;
                            let _e254 = color1_6;
                            let _e256 = (_e252.xyz * _e254.xyz);
                            base[0u] = _e256.x;
                            base[1u] = _e256.y;
                            base[2u] = _e256.z;
                            let _e264 = color0_[3u];
                            let _e266 = color1_6[3u];
                            base[3u] = (_e264 * _e266);
                        }
                    }
                }
            }
        }
    }
    if override_type_4_8 {
        let _e271 = unnamed.worldLightParams[1u];
        wetness = clamp(_e271, 0f, 1f);
        let _e275 = unnamed.worldLightParams[2u];
        frost = clamp(_e275, 0f, 1f);
        let _e277 = base;
        luminance = dot(_e277.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e280 = wetness;
        let _e282 = base;
        let _e284 = (_e282.xyz * mix(1f, 0.82f, _e280));
        base[0u] = _e284.x;
        base[1u] = _e284.y;
        base[2u] = _e284.z;
        let _e291 = base;
        let _e293 = luminance;
        let _e295 = luminance;
        let _e297 = luminance;
        let _e299 = frost;
        let _e302 = mix(_e291.xyz, vec3<f32>((_e293 * 0.88f), (_e295 * 0.94f), _e297), vec3((_e299 * 0.55f)));
        base[0u] = _e302.x;
        base[1u] = _e302.y;
        base[2u] = _e302.z;
    }
    let _e311 = unnamed.materialFactors[0u];
    if (_e311 >= 0f) {
        let _e314 = unnamed.materialBaseColor;
        let _e315 = base;
        base = (_e315 * _e314);
    }
    let _e317 = color0_;
    let _e320 = unnamed.emissionRadiance;
    let _e323 = base;
    let _e325 = (_e323.xyz + (_e317.xyz * _e320.xyz));
    base[0u] = _e325.x;
    base[1u] = _e325.y;
    base[2u] = _e325.z;
    let _e332 = wired_advanced_fog_enabled_u0028_();
    if _e332 {
        let _e333 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e333;
        if override_type_4_9 {
            let _e334 = fogAmount;
            let _e336 = base;
            let _e338 = (_e336.xyz * (1f - _e334));
            base[0u] = _e338.x;
            base[1u] = _e338.y;
            base[2u] = _e338.z;
        } else {
            if override_type_4_10 {
                let _e345 = fogAmount;
                let _e347 = base;
                base = (_e347 * (1f - _e345));
            } else {
                if override_type_4_11 {
                    let _e349 = fogAmount;
                    let _e352 = base[3u];
                    base[3u] = (_e352 * (1f - _e349));
                } else {
                    let _e355 = base;
                    let _e358 = unnamed.advancedFogColorDensity;
                    let _e360 = fogAmount;
                    let _e362 = mix(_e355.xyz, _e358.xyz, vec3(_e360));
                    base[0u] = _e362.x;
                    base[1u] = _e362.y;
                    base[2u] = _e362.z;
                }
            }
        }
    } else {
        if override_type_4_12 {
            let _e369 = base;
            let _e372 = fog[3u];
            let _e374 = (_e369.xyz * (1f - _e372));
            base[0u] = _e374.x;
            base[1u] = _e374.y;
            base[2u] = _e374.z;
        } else {
            if override_type_4_13 {
                let _e381 = base;
                let _e383 = fog[3u];
                base = (_e381 * (1f - _e383));
            } else {
                if override_type_4_14 {
                    let _e387 = base[3u];
                    let _e389 = fog[3u];
                    base[3u] = (_e387 * (1f - _e389));
                } else {
                    let _e393 = base;
                    let _e394 = fog;
                    let _e396 = unnamed.fogColor;
                    let _e399 = fog[3u];
                    base = mix(_e393, (_e394 * _e396), vec4(_e399));
                }
            }
        }
    }
    if override_type_4_15 {
        let _e403 = base[3u];
        if (_e403 == 0f) {
            discard;
        }
    } else {
        if override_type_4_16 {
            let _e405 = base;
            let _e407 = base;
            if (dot(_e405.xyz, _e407.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e411 = base;
    out_color = _e411;
    param_10 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_10));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
