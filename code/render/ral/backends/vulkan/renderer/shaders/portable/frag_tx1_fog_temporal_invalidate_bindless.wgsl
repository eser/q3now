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
@id(10) override acff: i32 = 0i;
override override_type_4_2: bool = (acff == 1i);
override override_type_4_3: bool = (acff == 2i);
override override_type_4_4: bool = (acff == 3i);
override override_type_4_5: bool = (acff == 1i);
override override_type_4_6: bool = (acff == 2i);
override override_type_4_7: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_8: bool = (discard_mode == 1i);
override override_type_4_9: bool = (discard_mode == 2i);
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

    let _e67 = (*role);
    let _e69 = (*role);
    let _e74 = unnamed.packed_indices[(_e67 / 4u)][(_e69 % 4u)];
    let _e77 = (*role);
    let _e79 = (*role);
    let _e84 = unnamed.packed_indices[(_e77 / 4u)][(_e79 % 4u)];
    let _e89 = (*uv);
    let _e90 = textureSample(wired_bindless_images[(_e74 & 4095u)], wired_bindless_samplers[((_e84 >> bitcast<u32>(12i)) & 255u)], _e89);
    c_1 = _e90;
    let _e91 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e91))) == 0i) {
        let _e96 = c_1;
        param = _e96.xyz;
        let _e98 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e98.x;
        c_1[1u] = _e98.y;
        c_1[2u] = _e98.z;
    }
    let _e105 = (*slot);
    if (lightmap_slot == (_e105 + 1i)) {
        let _e110 = unnamed.worldLightParams[0u];
        let _e111 = c_1;
        let _e113 = (_e111.xyz * _e110);
        c_1[0u] = _e113.x;
        c_1[1u] = _e113.y;
        c_1[2u] = _e113.z;
    }
    let _e120 = c_1;
    return _e120;
}

fn main_1() {
    var fog: vec4<f32>;
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
    var fogAmount: f32;
    var param_14: f32;

    let _e87 = unnamed.packed_indices[0i][3u];
    let _e93 = unnamed.packed_indices[0i][3u];
    let _e98 = fog_tex_coord_1;
    let _e99 = textureSample(wired_bindless_images[(_e87 & 4095u)], wired_bindless_samplers[((_e93 >> bitcast<u32>(12i)) & 255u)], _e98);
    fog = _e99;
    let _e100 = frag_color0In_1;
    param_1 = _e100.xyz;
    let _e102 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e104 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e102.x, _e102.y, _e102.z, _e104);
    param_2 = 0u;
    let _e109 = frag_tex_coord0_1;
    param_3 = _e109;
    param_4 = 0i;
    let _e110 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e111 = frag_color0_;
    color0_ = (_e110 * _e111);
    if override_type_4_ {
        param_5 = 1u;
        let _e113 = frag_tex_coord1_1;
        param_6 = _e113;
        param_7 = 1i;
        let _e114 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e114;
        let _e115 = color0_;
        let _e117 = color1_;
        let _e119 = (_e115.xyz + _e117.xyz);
        let _e121 = color0_[3u];
        let _e123 = color1_[3u];
        base = vec4<f32>(_e119.x, _e119.y, _e119.z, (_e121 * _e123));
    } else {
        if override_type_4_1 {
            param_8 = 1u;
            let _e129 = frag_tex_coord1_1;
            param_9 = _e129;
            param_10 = 1i;
            let _e130 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            let _e131 = frag_color0_;
            color1_1 = (_e130 * _e131);
            let _e133 = color0_;
            let _e135 = color1_1;
            let _e137 = (_e133.xyz + _e135.xyz);
            let _e139 = color0_[3u];
            let _e141 = color1_1[3u];
            base = vec4<f32>(_e137.x, _e137.y, _e137.z, (_e139 * _e141));
        } else {
            param_11 = 1u;
            let _e147 = frag_tex_coord1_1;
            param_12 = _e147;
            param_13 = 1i;
            let _e148 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e148;
            let _e149 = color0_;
            let _e151 = color1_2;
            let _e153 = (_e149.xyz * _e151.xyz);
            base[0u] = _e153.x;
            base[1u] = _e153.y;
            base[2u] = _e153.z;
            let _e161 = color0_[3u];
            let _e163 = color1_2[3u];
            base[3u] = (_e161 * _e163);
        }
    }
    let _e166 = wired_advanced_fog_enabled_u0028_();
    if _e166 {
        let _e167 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e167;
        if override_type_4_2 {
            let _e168 = fogAmount;
            let _e170 = base;
            let _e172 = (_e170.xyz * (1f - _e168));
            base[0u] = _e172.x;
            base[1u] = _e172.y;
            base[2u] = _e172.z;
        } else {
            if override_type_4_3 {
                let _e179 = fogAmount;
                let _e181 = base;
                base = (_e181 * (1f - _e179));
            } else {
                if override_type_4_4 {
                    let _e183 = fogAmount;
                    let _e186 = base[3u];
                    base[3u] = (_e186 * (1f - _e183));
                } else {
                    let _e189 = base;
                    let _e192 = unnamed.advancedFogColorDensity;
                    let _e194 = fogAmount;
                    let _e196 = mix(_e189.xyz, _e192.xyz, vec3(_e194));
                    base[0u] = _e196.x;
                    base[1u] = _e196.y;
                    base[2u] = _e196.z;
                }
            }
        }
    } else {
        if override_type_4_5 {
            let _e203 = base;
            let _e206 = fog[3u];
            let _e208 = (_e203.xyz * (1f - _e206));
            base[0u] = _e208.x;
            base[1u] = _e208.y;
            base[2u] = _e208.z;
        } else {
            if override_type_4_6 {
                let _e215 = base;
                let _e217 = fog[3u];
                base = (_e215 * (1f - _e217));
            } else {
                if override_type_4_7 {
                    let _e221 = base[3u];
                    let _e223 = fog[3u];
                    base[3u] = (_e221 * (1f - _e223));
                } else {
                    let _e227 = base;
                    let _e228 = fog;
                    let _e230 = unnamed.fogColor;
                    let _e233 = fog[3u];
                    base = mix(_e227, (_e228 * _e230), vec4(_e233));
                }
            }
        }
    }
    if override_type_4_8 {
        let _e237 = base[3u];
        if (_e237 == 0f) {
            discard;
        }
    } else {
        if override_type_4_9 {
            let _e239 = base;
            let _e241 = base;
            if (dot(_e239.xyz, _e241.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e245 = base;
    out_color = _e245;
    param_14 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_14));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
