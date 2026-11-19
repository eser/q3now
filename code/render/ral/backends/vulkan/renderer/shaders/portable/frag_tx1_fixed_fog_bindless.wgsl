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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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

    let _e72 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e72 + 0.5f));
    let _e77 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e79 = fogType;
    let _e82 = fogType;
    return (((_e77 > 0.5f) && (_e79 >= 1i)) && (_e82 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e72 = wired_advanced_fog_enabled_u0028_();
    if !(_e72) {
        return 0f;
    }
    let _e75 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e75, 0.000001f));
    let _e80 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e80 + 0.5f));
    let _e83 = fogType_1;
    if (_e83 == 1i) {
        let _e87 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e87 <= 0f) {
            return 0f;
        }
        let _e89 = viewDepth;
        let _e92 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e89 / _e92), 0f, 1f);
    }
    let _e97 = unnamed.advancedFogColorDensity[3u];
    let _e99 = viewDepth;
    opticalDepth = (max(_e97, 0f) * _e99);
    let _e101 = fogType_1;
    if (_e101 == 2i) {
        let _e103 = opticalDepth;
        return clamp((1f - exp(-(_e103))), 0f, 1f);
    }
    let _e108 = opticalDepth;
    let _e109 = opticalDepth;
    return clamp((1f - exp(-((_e108 * _e109)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e72 = (*rgb);
    let _e75 = unnamed.worldLightParams[0u];
    boosted = (_e72 * _e75);
    let _e78 = boosted[0u];
    let _e80 = boosted[1u];
    let _e82 = boosted[2u];
    peak = max(_e78, max(_e80, _e82));
    let _e85 = peak;
    if (_e85 > 1f) {
        let _e87 = peak;
        let _e88 = boosted;
        boosted = (_e88 / vec3(_e87));
    }
    let _e91 = boosted;
    return _e91;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e73 = (*c);
    (*c) = max(_e73, vec3<f32>(0f, 0f, 0f));
    let _e75 = (*c);
    cutoff = (_e75 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e77 = (*c);
    lo = (_e77 / vec3(12.92f));
    let _e80 = (*c);
    hi = pow(((_e80 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e85 = hi;
    let _e86 = lo;
    let _e87 = cutoff;
    return mix(_e85, _e86, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e87));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e85 = (*role);
    let _e87 = (*role);
    let _e92 = unnamed.packed_indices[(_e85 / 4u)][(_e87 % 4u)];
    let _e97 = (*uv);
    let _e98 = textureSample(wired_bindless_images[(_e82 & 4095u)], wired_bindless_samplers[((_e92 >> bitcast<u32>(12i)) & 255u)], _e97);
    c_1 = _e98;
    let _e99 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e99))) == 0i) {
        let _e104 = c_1;
        param = _e104.xyz;
        let _e106 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e106.x;
        c_1[1u] = _e106.y;
        c_1[2u] = _e106.z;
    }
    let _e113 = (*slot);
    if (lightmap_slot == (_e113 + 1i)) {
        let _e116 = c_1;
        param_1 = _e116.xyz;
        let _e118 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e118.x;
        c_1[1u] = _e118.y;
        c_1[2u] = _e118.z;
    }
    let _e125 = c_1;
    return _e125;
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

    let _e91 = unnamed.packed_indices[0i][3u];
    let _e97 = unnamed.packed_indices[0i][3u];
    let _e102 = fog_tex_coord_1;
    let _e103 = textureSample(wired_bindless_images[(_e91 & 4095u)], wired_bindless_samplers[((_e97 >> bitcast<u32>(12i)) & 255u)], _e102);
    fog = _e103;
    param_2 = 0u;
    let _e104 = frag_tex_coord0_1;
    param_3 = _e104;
    param_4 = 0i;
    let _e105 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e105;
    param_5 = 1u;
    let _e106 = frag_tex_coord1_1;
    param_6 = _e106;
    param_7 = 1i;
    let _e107 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor1_ = _e107;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e112 = sampledColor0_;
    let _e113 = frag_color;
    color0_ = (_e112 * _e113);
    if override_type_3_ {
        let _e115 = sampledColor1_;
        color1_ = _e115;
        let _e116 = color0_;
        let _e118 = color1_;
        let _e120 = (_e116.xyz + _e118.xyz);
        let _e122 = color0_[3u];
        let _e124 = color1_[3u];
        base = vec4<f32>(_e120.x, _e120.y, _e120.z, (_e122 * _e124));
    } else {
        if override_type_3_1 {
            let _e130 = sampledColor1_;
            let _e131 = frag_color;
            color1_1 = (_e130 * _e131);
            let _e133 = color0_;
            let _e135 = color1_1;
            let _e137 = (_e133.xyz + _e135.xyz);
            let _e139 = color0_[3u];
            let _e141 = color1_1[3u];
            base = vec4<f32>(_e137.x, _e137.y, _e137.z, (_e139 * _e141));
        } else {
            let _e147 = sampledColor1_;
            let _e148 = frag_color;
            color1_2 = (_e147 * _e148);
            let _e150 = color0_;
            let _e152 = color1_2;
            let _e154 = (_e150.xyz * _e152.xyz);
            base[0u] = _e154.x;
            base[1u] = _e154.y;
            base[2u] = _e154.z;
            let _e162 = color0_[3u];
            let _e164 = color1_2[3u];
            base[3u] = (_e162 * _e164);
        }
    }
    if override_type_3_2 {
        let _e169 = unnamed.worldLightParams[1u];
        wetness = clamp(_e169, 0f, 1f);
        let _e173 = unnamed.worldLightParams[2u];
        frost = clamp(_e173, 0f, 1f);
        let _e175 = base;
        luminance = dot(_e175.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e178 = wetness;
        let _e180 = base;
        let _e182 = (_e180.xyz * mix(1f, 0.82f, _e178));
        base[0u] = _e182.x;
        base[1u] = _e182.y;
        base[2u] = _e182.z;
        let _e189 = base;
        let _e191 = luminance;
        let _e193 = luminance;
        let _e195 = luminance;
        let _e197 = frost;
        let _e200 = mix(_e189.xyz, vec3<f32>((_e191 * 0.88f), (_e193 * 0.94f), _e195), vec3((_e197 * 0.55f)));
        base[0u] = _e200.x;
        base[1u] = _e200.y;
        base[2u] = _e200.z;
    }
    let _e209 = unnamed.materialFactors[0u];
    if (_e209 >= 0f) {
        let _e212 = unnamed.materialBaseColor;
        let _e213 = base;
        base = (_e213 * _e212);
    }
    let _e215 = color0_;
    let _e218 = unnamed.emissionRadiance;
    let _e221 = base;
    let _e223 = (_e221.xyz + (_e215.xyz * _e218.xyz));
    base[0u] = _e223.x;
    base[1u] = _e223.y;
    base[2u] = _e223.z;
    let _e230 = wired_advanced_fog_enabled_u0028_();
    if _e230 {
        let _e231 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e231;
        if override_type_3_3 {
            let _e232 = fogAmount;
            let _e234 = base;
            let _e236 = (_e234.xyz * (1f - _e232));
            base[0u] = _e236.x;
            base[1u] = _e236.y;
            base[2u] = _e236.z;
        } else {
            if override_type_3_4 {
                let _e243 = fogAmount;
                let _e245 = base;
                base = (_e245 * (1f - _e243));
            } else {
                if override_type_3_5 {
                    let _e247 = fogAmount;
                    let _e250 = base[3u];
                    base[3u] = (_e250 * (1f - _e247));
                } else {
                    let _e253 = base;
                    let _e256 = unnamed.advancedFogColorDensity;
                    let _e258 = fogAmount;
                    let _e260 = mix(_e253.xyz, _e256.xyz, vec3(_e258));
                    base[0u] = _e260.x;
                    base[1u] = _e260.y;
                    base[2u] = _e260.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e267 = base;
            let _e270 = fog[3u];
            let _e272 = (_e267.xyz * (1f - _e270));
            base[0u] = _e272.x;
            base[1u] = _e272.y;
            base[2u] = _e272.z;
        } else {
            if override_type_3_7 {
                let _e279 = base;
                let _e281 = fog[3u];
                base = (_e279 * (1f - _e281));
            } else {
                if override_type_3_8 {
                    let _e285 = base[3u];
                    let _e287 = fog[3u];
                    base[3u] = (_e285 * (1f - _e287));
                } else {
                    let _e291 = base;
                    let _e292 = fog;
                    let _e294 = unnamed.fogColor;
                    let _e297 = fog[3u];
                    base = mix(_e291, (_e292 * _e294), vec4(_e297));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e301 = base[3u];
        if (_e301 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e303 = base;
            let _e305 = base;
            if (dot(_e303.xyz, _e305.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e309 = base;
    out_color = _e309;
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
