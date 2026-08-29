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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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

    let _e76 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e76 + 0.5f));
    let _e81 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e83 = fogType;
    let _e86 = fogType;
    return (((_e81 > 0.5f) && (_e83 >= 1i)) && (_e86 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e76 = wired_advanced_fog_enabled_u0028_();
    if !(_e76) {
        return 0f;
    }
    let _e79 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e79, 0.000001f));
    let _e84 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e84 + 0.5f));
    let _e87 = fogType_1;
    if (_e87 == 1i) {
        let _e91 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e91 <= 0f) {
            return 0f;
        }
        let _e93 = viewDepth;
        let _e96 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e93 / _e96), 0f, 1f);
    }
    let _e101 = unnamed.advancedFogColorDensity[3u];
    let _e103 = viewDepth;
    opticalDepth = (max(_e101, 0f) * _e103);
    let _e105 = fogType_1;
    if (_e105 == 2i) {
        let _e107 = opticalDepth;
        return clamp((1f - exp(-(_e107))), 0f, 1f);
    }
    let _e112 = opticalDepth;
    let _e113 = opticalDepth;
    return clamp((1f - exp(-((_e112 * _e113)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e77 = (*c);
    (*c) = max(_e77, vec3<f32>(0f, 0f, 0f));
    let _e79 = (*c);
    cutoff = (_e79 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e81 = (*c);
    lo = (_e81 / vec3(12.92f));
    let _e84 = (*c);
    hi = pow(((_e84 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e89 = hi;
    let _e90 = lo;
    let _e91 = cutoff;
    return mix(_e89, _e90, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e91));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e78 = (*role);
    let _e80 = (*role);
    let _e85 = unnamed.packed_indices[(_e78 / 4u)][(_e80 % 4u)];
    let _e88 = (*role);
    let _e90 = (*role);
    let _e95 = unnamed.packed_indices[(_e88 / 4u)][(_e90 % 4u)];
    let _e100 = (*uv);
    let _e101 = textureSample(wired_bindless_images[(_e85 & 4095u)], wired_bindless_samplers[((_e95 >> bitcast<u32>(12i)) & 255u)], _e100);
    c_1 = _e101;
    let _e102 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e102))) == 0i) {
        let _e107 = c_1;
        param = _e107.xyz;
        let _e109 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e109.x;
        c_1[1u] = _e109.y;
        c_1[2u] = _e109.z;
    }
    let _e116 = (*slot);
    if (lightmap_slot == (_e116 + 1i)) {
        let _e121 = unnamed.worldLightParams[0u];
        let _e122 = c_1;
        let _e124 = (_e122.xyz * _e121);
        c_1[0u] = _e124.x;
        c_1[1u] = _e124.y;
        c_1[2u] = _e124.z;
    }
    let _e131 = c_1;
    return _e131;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var color1_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color1_2: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_13: f32;

    let _e100 = unnamed.packed_indices[0i][3u];
    let _e106 = unnamed.packed_indices[0i][3u];
    let _e111 = fog_tex_coord_1;
    let _e112 = textureSample(wired_bindless_images[(_e100 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    fog = _e112;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e117 = frag_tex_coord0_1;
    param_2 = _e117;
    param_3 = 0i;
    let _e118 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e119 = frag_color;
    color0_ = (_e118 * _e119);
    if override_type_4_ {
        param_4 = 1u;
        let _e121 = frag_tex_coord1_1;
        param_5 = _e121;
        param_6 = 1i;
        let _e122 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e122;
        let _e123 = color0_;
        let _e125 = color1_;
        let _e127 = (_e123.xyz + _e125.xyz);
        let _e129 = color0_[3u];
        let _e131 = color1_[3u];
        base = vec4<f32>(_e127.x, _e127.y, _e127.z, (_e129 * _e131));
    } else {
        if override_type_4_1 {
            param_7 = 1u;
            let _e137 = frag_tex_coord1_1;
            param_8 = _e137;
            param_9 = 1i;
            let _e138 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            let _e139 = frag_color;
            color1_1 = (_e138 * _e139);
            let _e141 = color0_;
            let _e143 = color1_1;
            let _e145 = (_e141.xyz + _e143.xyz);
            let _e147 = color0_[3u];
            let _e149 = color1_1[3u];
            base = vec4<f32>(_e145.x, _e145.y, _e145.z, (_e147 * _e149));
        } else {
            param_10 = 1u;
            let _e155 = frag_tex_coord1_1;
            param_11 = _e155;
            param_12 = 1i;
            let _e156 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
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
    if override_type_4_2 {
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
    let _e216 = color0_;
    let _e219 = unnamed.emissionRadiance;
    let _e222 = base;
    let _e224 = (_e222.xyz + (_e216.xyz * _e219.xyz));
    base[0u] = _e224.x;
    base[1u] = _e224.y;
    base[2u] = _e224.z;
    let _e231 = wired_advanced_fog_enabled_u0028_();
    if _e231 {
        let _e232 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e232;
        if override_type_4_3 {
            let _e233 = fogAmount;
            let _e235 = base;
            let _e237 = (_e235.xyz * (1f - _e233));
            base[0u] = _e237.x;
            base[1u] = _e237.y;
            base[2u] = _e237.z;
        } else {
            if override_type_4_4 {
                let _e244 = fogAmount;
                let _e246 = base;
                base = (_e246 * (1f - _e244));
            } else {
                if override_type_4_5 {
                    let _e248 = fogAmount;
                    let _e251 = base[3u];
                    base[3u] = (_e251 * (1f - _e248));
                } else {
                    let _e254 = base;
                    let _e257 = unnamed.advancedFogColorDensity;
                    let _e259 = fogAmount;
                    let _e261 = mix(_e254.xyz, _e257.xyz, vec3(_e259));
                    base[0u] = _e261.x;
                    base[1u] = _e261.y;
                    base[2u] = _e261.z;
                }
            }
        }
    } else {
        if override_type_4_6 {
            let _e268 = base;
            let _e271 = fog[3u];
            let _e273 = (_e268.xyz * (1f - _e271));
            base[0u] = _e273.x;
            base[1u] = _e273.y;
            base[2u] = _e273.z;
        } else {
            if override_type_4_7 {
                let _e280 = base;
                let _e282 = fog[3u];
                base = (_e280 * (1f - _e282));
            } else {
                if override_type_4_8 {
                    let _e286 = base[3u];
                    let _e288 = fog[3u];
                    base[3u] = (_e286 * (1f - _e288));
                } else {
                    let _e292 = base;
                    let _e293 = fog;
                    let _e295 = unnamed.fogColor;
                    let _e298 = fog[3u];
                    base = mix(_e292, (_e293 * _e295), vec4(_e298));
                }
            }
        }
    }
    if override_type_4_9 {
        let _e302 = base[3u];
        if (_e302 == 0f) {
            discard;
        }
    } else {
        if override_type_4_10 {
            let _e304 = base;
            let _e306 = base;
            if (dot(_e304.xyz, _e306.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e310 = base;
    out_color = _e310;
    param_13 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_13));
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
