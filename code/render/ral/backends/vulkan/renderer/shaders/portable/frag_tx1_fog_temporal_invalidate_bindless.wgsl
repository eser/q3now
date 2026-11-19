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

    let _e77 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e77 + 0.5f));
    let _e82 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e84 = fogType;
    let _e87 = fogType;
    return (((_e82 > 0.5f) && (_e84 >= 1i)) && (_e87 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e77 = wired_advanced_fog_enabled_u0028_();
    if !(_e77) {
        return 0f;
    }
    let _e80 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e80, 0.000001f));
    let _e85 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e85 + 0.5f));
    let _e88 = fogType_1;
    if (_e88 == 1i) {
        let _e92 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e92 <= 0f) {
            return 0f;
        }
        let _e94 = viewDepth;
        let _e97 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e94 / _e97), 0f, 1f);
    }
    let _e102 = unnamed.advancedFogColorDensity[3u];
    let _e104 = viewDepth;
    opticalDepth = (max(_e102, 0f) * _e104);
    let _e106 = fogType_1;
    if (_e106 == 2i) {
        let _e108 = opticalDepth;
        return clamp((1f - exp(-(_e108))), 0f, 1f);
    }
    let _e113 = opticalDepth;
    let _e114 = opticalDepth;
    return clamp((1f - exp(-((_e113 * _e114)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e77 = (*rgb);
    let _e80 = unnamed.worldLightParams[0u];
    boosted = (_e77 * _e80);
    let _e83 = boosted[0u];
    let _e85 = boosted[1u];
    let _e87 = boosted[2u];
    peak = max(_e83, max(_e85, _e87));
    let _e90 = peak;
    if (_e90 > 1f) {
        let _e92 = peak;
        let _e93 = boosted;
        boosted = (_e93 / vec3(_e92));
    }
    let _e96 = boosted;
    return _e96;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e78 = (*c);
    (*c) = max(_e78, vec3<f32>(0f, 0f, 0f));
    let _e80 = (*c);
    cutoff = (_e80 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e82 = (*c);
    lo = (_e82 / vec3(12.92f));
    let _e85 = (*c);
    hi = pow(((_e85 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e90 = hi;
    let _e91 = lo;
    let _e92 = cutoff;
    return mix(_e90, _e91, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e92));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e80 = (*role);
    let _e82 = (*role);
    let _e87 = unnamed.packed_indices[(_e80 / 4u)][(_e82 % 4u)];
    let _e90 = (*role);
    let _e92 = (*role);
    let _e97 = unnamed.packed_indices[(_e90 / 4u)][(_e92 % 4u)];
    let _e102 = (*uv);
    let _e103 = textureSample(wired_bindless_images[(_e87 & 4095u)], wired_bindless_samplers[((_e97 >> bitcast<u32>(12i)) & 255u)], _e102);
    c_1 = _e103;
    let _e104 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e104))) == 0i) {
        let _e109 = c_1;
        param = _e109.xyz;
        let _e111 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e111.x;
        c_1[1u] = _e111.y;
        c_1[2u] = _e111.z;
    }
    let _e118 = (*slot);
    if (lightmap_slot == (_e118 + 1i)) {
        let _e121 = c_1;
        param_1 = _e121.xyz;
        let _e123 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e123.x;
        c_1[1u] = _e123.y;
        c_1[2u] = _e123.z;
    }
    let _e130 = c_1;
    return _e130;
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

    let _e98 = unnamed.packed_indices[0i][3u];
    let _e104 = unnamed.packed_indices[0i][3u];
    let _e109 = fog_tex_coord_1;
    let _e110 = textureSample(wired_bindless_images[(_e98 & 4095u)], wired_bindless_samplers[((_e104 >> bitcast<u32>(12i)) & 255u)], _e109);
    fog = _e110;
    let _e111 = frag_color0In_1;
    param_2 = _e111.xyz;
    let _e113 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e115 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e113.x, _e113.y, _e113.z, _e115);
    param_3 = 0u;
    let _e120 = frag_tex_coord0_1;
    param_4 = _e120;
    param_5 = 0i;
    let _e121 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e121;
    param_6 = 1u;
    let _e122 = frag_tex_coord1_1;
    param_7 = _e122;
    param_8 = 1i;
    let _e123 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    sampledColor1_ = _e123;
    let _e124 = sampledColor0_;
    let _e125 = frag_color0_;
    color0_ = (_e124 * _e125);
    if override_type_4_ {
        let _e127 = sampledColor1_;
        color1_ = _e127;
        let _e128 = color0_;
        let _e130 = color1_;
        let _e132 = (_e128.xyz + _e130.xyz);
        let _e134 = color0_[3u];
        let _e136 = color1_[3u];
        base = vec4<f32>(_e132.x, _e132.y, _e132.z, (_e134 * _e136));
    } else {
        if override_type_4_1 {
            let _e142 = sampledColor1_;
            let _e143 = frag_color0_;
            color1_1 = (_e142 * _e143);
            let _e145 = color0_;
            let _e147 = color1_1;
            let _e149 = (_e145.xyz + _e147.xyz);
            let _e151 = color0_[3u];
            let _e153 = color1_1[3u];
            base = vec4<f32>(_e149.x, _e149.y, _e149.z, (_e151 * _e153));
        } else {
            let _e159 = sampledColor1_;
            color1_2 = _e159;
            let _e160 = color0_;
            let _e162 = color1_2;
            let _e164 = (_e160.xyz * _e162.xyz);
            base[0u] = _e164.x;
            base[1u] = _e164.y;
            base[2u] = _e164.z;
            let _e172 = color0_[3u];
            let _e174 = color1_2[3u];
            base[3u] = (_e172 * _e174);
        }
    }
    if override_type_4_2 {
        let _e179 = unnamed.worldLightParams[1u];
        wetness = clamp(_e179, 0f, 1f);
        let _e183 = unnamed.worldLightParams[2u];
        frost = clamp(_e183, 0f, 1f);
        let _e185 = base;
        luminance = dot(_e185.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e188 = wetness;
        let _e190 = base;
        let _e192 = (_e190.xyz * mix(1f, 0.82f, _e188));
        base[0u] = _e192.x;
        base[1u] = _e192.y;
        base[2u] = _e192.z;
        let _e199 = base;
        let _e201 = luminance;
        let _e203 = luminance;
        let _e205 = luminance;
        let _e207 = frost;
        let _e210 = mix(_e199.xyz, vec3<f32>((_e201 * 0.88f), (_e203 * 0.94f), _e205), vec3((_e207 * 0.55f)));
        base[0u] = _e210.x;
        base[1u] = _e210.y;
        base[2u] = _e210.z;
    }
    let _e219 = unnamed.materialFactors[0u];
    if (_e219 >= 0f) {
        let _e222 = unnamed.materialBaseColor;
        let _e223 = base;
        base = (_e223 * _e222);
    }
    let _e225 = color0_;
    let _e228 = unnamed.emissionRadiance;
    let _e231 = base;
    let _e233 = (_e231.xyz + (_e225.xyz * _e228.xyz));
    base[0u] = _e233.x;
    base[1u] = _e233.y;
    base[2u] = _e233.z;
    let _e240 = wired_advanced_fog_enabled_u0028_();
    if _e240 {
        let _e241 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e241;
        if override_type_4_3 {
            let _e242 = fogAmount;
            let _e244 = base;
            let _e246 = (_e244.xyz * (1f - _e242));
            base[0u] = _e246.x;
            base[1u] = _e246.y;
            base[2u] = _e246.z;
        } else {
            if override_type_4_4 {
                let _e253 = fogAmount;
                let _e255 = base;
                base = (_e255 * (1f - _e253));
            } else {
                if override_type_4_5 {
                    let _e257 = fogAmount;
                    let _e260 = base[3u];
                    base[3u] = (_e260 * (1f - _e257));
                } else {
                    let _e263 = base;
                    let _e266 = unnamed.advancedFogColorDensity;
                    let _e268 = fogAmount;
                    let _e270 = mix(_e263.xyz, _e266.xyz, vec3(_e268));
                    base[0u] = _e270.x;
                    base[1u] = _e270.y;
                    base[2u] = _e270.z;
                }
            }
        }
    } else {
        if override_type_4_6 {
            let _e277 = base;
            let _e280 = fog[3u];
            let _e282 = (_e277.xyz * (1f - _e280));
            base[0u] = _e282.x;
            base[1u] = _e282.y;
            base[2u] = _e282.z;
        } else {
            if override_type_4_7 {
                let _e289 = base;
                let _e291 = fog[3u];
                base = (_e289 * (1f - _e291));
            } else {
                if override_type_4_8 {
                    let _e295 = base[3u];
                    let _e297 = fog[3u];
                    base[3u] = (_e295 * (1f - _e297));
                } else {
                    let _e301 = base;
                    let _e302 = fog;
                    let _e304 = unnamed.fogColor;
                    let _e307 = fog[3u];
                    base = mix(_e301, (_e302 * _e304), vec4(_e307));
                }
            }
        }
    }
    if override_type_4_9 {
        let _e311 = base[3u];
        if (_e311 == 0f) {
            discard;
        }
    } else {
        if override_type_4_10 {
            let _e313 = base;
            let _e315 = base;
            if (dot(_e313.xyz, _e315.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e319 = base;
    out_color = _e319;
    param_9 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_9));
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
