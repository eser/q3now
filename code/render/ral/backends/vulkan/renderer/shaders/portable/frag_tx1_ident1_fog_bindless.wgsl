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

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_3_: bool = (tex_mode == 1i);
override override_type_3_1: bool = (tex_mode == 2i);
override override_type_3_2: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_3: bool = (acff == 1i);
override override_type_3_4: bool = (acff == 2i);
override override_type_3_5: bool = (acff == 3i);
override override_type_3_6: bool = (acff == 1i);
override override_type_3_7: bool = (acff == 2i);
override override_type_3_8: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_9: bool = (discard_mode == 1i);
override override_type_3_10: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

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

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e70 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e70 + 0.5f));
    let _e75 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e77 = fogType;
    let _e80 = fogType;
    return (((_e75 > 0.5f) && (_e77 >= 1i)) && (_e80 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e70 = wired_advanced_fog_enabled_u0028_();
    if !(_e70) {
        return 0f;
    }
    let _e73 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e73, 0.000001f));
    let _e78 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e78 + 0.5f));
    let _e81 = fogType_1;
    if (_e81 == 1i) {
        let _e85 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e85 <= 0f) {
            return 0f;
        }
        let _e87 = viewDepth;
        let _e90 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e87 / _e90), 0f, 1f);
    }
    let _e95 = unnamed.advancedFogColorDensity[3u];
    let _e97 = viewDepth;
    opticalDepth = (max(_e95, 0f) * _e97);
    let _e99 = fogType_1;
    if (_e99 == 2i) {
        let _e101 = opticalDepth;
        return clamp((1f - exp(-(_e101))), 0f, 1f);
    }
    let _e106 = opticalDepth;
    let _e107 = opticalDepth;
    return clamp((1f - exp(-((_e106 * _e107)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e70 = (*rgb);
    let _e73 = unnamed.worldLightParams[0u];
    boosted = (_e70 * _e73);
    let _e76 = boosted[0u];
    let _e78 = boosted[1u];
    let _e80 = boosted[2u];
    peak = max(_e76, max(_e78, _e80));
    let _e83 = peak;
    if (_e83 > 1f) {
        let _e85 = peak;
        let _e86 = boosted;
        boosted = (_e86 / vec3(_e85));
    }
    let _e89 = boosted;
    return _e89;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e71 = (*c);
    (*c) = max(_e71, vec3<f32>(0f, 0f, 0f));
    let _e73 = (*c);
    cutoff = (_e73 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e75 = (*c);
    lo = (_e75 / vec3(12.92f));
    let _e78 = (*c);
    hi = pow(((_e78 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e83 = hi;
    let _e84 = lo;
    let _e85 = cutoff;
    return mix(_e83, _e84, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e85));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e73 = (*role);
    let _e75 = (*role);
    let _e80 = unnamed.packed_indices[(_e73 / 4u)][(_e75 % 4u)];
    let _e83 = (*role);
    let _e85 = (*role);
    let _e90 = unnamed.packed_indices[(_e83 / 4u)][(_e85 % 4u)];
    let _e95 = (*uv);
    let _e96 = textureSample(wired_bindless_images[(_e80 & 4095u)], wired_bindless_samplers[((_e90 >> bitcast<u32>(12i)) & 255u)], _e95);
    c_1 = _e96;
    let _e97 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e97))) == 0i) {
        let _e102 = c_1;
        param = _e102.xyz;
        let _e104 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e104.x;
        c_1[1u] = _e104.y;
        c_1[2u] = _e104.z;
    }
    let _e111 = (*slot);
    if (lightmap_slot == (_e111 + 1i)) {
        let _e114 = c_1;
        param_1 = _e114.xyz;
        let _e116 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e116.x;
        c_1[1u] = _e116.y;
        c_1[2u] = _e116.z;
    }
    let _e123 = c_1;
    return _e123;
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
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e88 = unnamed.packed_indices[0i][3u];
    let _e94 = unnamed.packed_indices[0i][3u];
    let _e99 = fog_tex_coord_1;
    let _e100 = textureSample(wired_bindless_images[(_e88 & 4095u)], wired_bindless_samplers[((_e94 >> bitcast<u32>(12i)) & 255u)], _e99);
    fog = _e100;
    param_2 = 0u;
    let _e101 = frag_tex_coord0_1;
    param_3 = _e101;
    param_4 = 0i;
    let _e102 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e102;
    param_5 = 1u;
    let _e103 = frag_tex_coord1_1;
    param_6 = _e103;
    param_7 = 1i;
    let _e104 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor1_ = _e104;
    let _e105 = sampledColor0_;
    color0_ = _e105;
    if override_type_3_ {
        let _e106 = sampledColor1_;
        color1_ = _e106;
        let _e107 = color0_;
        let _e109 = color1_;
        let _e111 = (_e107.xyz + _e109.xyz);
        let _e113 = color0_[3u];
        let _e115 = color1_[3u];
        base = vec4<f32>(_e111.x, _e111.y, _e111.z, (_e113 * _e115));
    } else {
        if override_type_3_1 {
            let _e121 = sampledColor1_;
            color1_1 = _e121;
            let _e122 = color0_;
            let _e124 = color1_1;
            let _e126 = (_e122.xyz + _e124.xyz);
            let _e128 = color0_[3u];
            let _e130 = color1_1[3u];
            base = vec4<f32>(_e126.x, _e126.y, _e126.z, (_e128 * _e130));
        } else {
            let _e136 = sampledColor1_;
            color1_2 = _e136;
            let _e137 = color0_;
            let _e139 = color1_2;
            let _e141 = (_e137.xyz * _e139.xyz);
            base[0u] = _e141.x;
            base[1u] = _e141.y;
            base[2u] = _e141.z;
            let _e149 = color0_[3u];
            let _e151 = color1_2[3u];
            base[3u] = (_e149 * _e151);
        }
    }
    if override_type_3_2 {
        let _e156 = unnamed.worldLightParams[1u];
        wetness = clamp(_e156, 0f, 1f);
        let _e160 = unnamed.worldLightParams[2u];
        frost = clamp(_e160, 0f, 1f);
        let _e162 = base;
        luminance = dot(_e162.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e165 = wetness;
        let _e167 = base;
        let _e169 = (_e167.xyz * mix(1f, 0.82f, _e165));
        base[0u] = _e169.x;
        base[1u] = _e169.y;
        base[2u] = _e169.z;
        let _e176 = base;
        let _e178 = luminance;
        let _e180 = luminance;
        let _e182 = luminance;
        let _e184 = frost;
        let _e187 = mix(_e176.xyz, vec3<f32>((_e178 * 0.88f), (_e180 * 0.94f), _e182), vec3((_e184 * 0.55f)));
        base[0u] = _e187.x;
        base[1u] = _e187.y;
        base[2u] = _e187.z;
    }
    let _e196 = unnamed.materialFactors[0u];
    if (_e196 >= 0f) {
        let _e199 = unnamed.materialBaseColor;
        let _e200 = base;
        base = (_e200 * _e199);
    }
    let _e202 = color0_;
    let _e205 = unnamed.emissionRadiance;
    let _e208 = base;
    let _e210 = (_e208.xyz + (_e202.xyz * _e205.xyz));
    base[0u] = _e210.x;
    base[1u] = _e210.y;
    base[2u] = _e210.z;
    let _e217 = wired_advanced_fog_enabled_u0028_();
    if _e217 {
        let _e218 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e218;
        if override_type_3_3 {
            let _e219 = fogAmount;
            let _e221 = base;
            let _e223 = (_e221.xyz * (1f - _e219));
            base[0u] = _e223.x;
            base[1u] = _e223.y;
            base[2u] = _e223.z;
        } else {
            if override_type_3_4 {
                let _e230 = fogAmount;
                let _e232 = base;
                base = (_e232 * (1f - _e230));
            } else {
                if override_type_3_5 {
                    let _e234 = fogAmount;
                    let _e237 = base[3u];
                    base[3u] = (_e237 * (1f - _e234));
                } else {
                    let _e240 = base;
                    let _e243 = unnamed.advancedFogColorDensity;
                    let _e245 = fogAmount;
                    let _e247 = mix(_e240.xyz, _e243.xyz, vec3(_e245));
                    base[0u] = _e247.x;
                    base[1u] = _e247.y;
                    base[2u] = _e247.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e254 = base;
            let _e257 = fog[3u];
            let _e259 = (_e254.xyz * (1f - _e257));
            base[0u] = _e259.x;
            base[1u] = _e259.y;
            base[2u] = _e259.z;
        } else {
            if override_type_3_7 {
                let _e266 = base;
                let _e268 = fog[3u];
                base = (_e266 * (1f - _e268));
            } else {
                if override_type_3_8 {
                    let _e272 = base[3u];
                    let _e274 = fog[3u];
                    base[3u] = (_e272 * (1f - _e274));
                } else {
                    let _e278 = base;
                    let _e279 = fog;
                    let _e281 = unnamed.fogColor;
                    let _e284 = fog[3u];
                    base = mix(_e278, (_e279 * _e281), vec4(_e284));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e288 = base[3u];
        if (_e288 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e290 = base;
            let _e292 = base;
            if (dot(_e290.xyz, _e292.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e296 = base;
    out_color = _e296;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e9 = out_color;
    return _e9;
}
