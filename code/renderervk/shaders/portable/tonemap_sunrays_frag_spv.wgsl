struct ExposureBlock {
    exposure_bias: f32,
    key: f32,
    pctLow: f32,
    pctHigh: f32,
    rateUp: f32,
    rateDown: f32,
    minExp: f32,
    maxExp: f32,
    autoEnabled: i32,
    brightness: f32,
    sunScreenX: f32,
    sunScreenY: f32,
    sunrayIntensity: f32,
    sunrayDecay: f32,
}

@id(27) override sunrays_density: f32 = 1f;
@id(26) override sunrays_samples: i32 = 64i;
@id(33) override sunrays_threshold: f32 = 0.32f;
@id(24) override chromatic_strength: f32 = 0f;
@id(2) override saturation: f32 = 1f;
@id(12) override hdr_mode: i32 = 0i;
@id(13) override hdr_peak_norm: f32 = 10f;

var<private> frag_tex_coord_1: vec2<f32>;
@group(2) @binding(0) 
var<uniform> eb: ExposureBlock;
@group(1) @binding(0) 
var depthMap: texture_2d<f32>;
@group(1) @binding(32) 
var depthMap_sampler: sampler;
@group(0) @binding(0) 
var texture0_: texture_2d<f32>;
@group(0) @binding(32) 
var texture0_sampler: sampler;
var<private> out_color: vec4<f32>;

fn computeSunRays_u0028_() -> vec3<f32> {
    var deltaUV: vec2<f32>;
    var uv: vec2<f32>;
    var illumination: vec3<f32>;
    var weight: f32;
    var i: i32;
    var depth: f32;
    var isSky: f32;
    var sceneColor: vec3<f32>;
    var threshold: vec3<f32>;
    var bright: vec3<f32>;
    var phi_74_: bool;
    var phi_82_: bool;
    var phi_89_: bool;

    let _e44 = frag_tex_coord_1;
    let _e46 = eb.sunScreenX;
    let _e48 = eb.sunScreenY;
    deltaUV = (((_e44 - vec2<f32>(_e46, _e48)) * sunrays_density) / vec2(f32(sunrays_samples)));
    let _e55 = frag_tex_coord_1;
    uv = _e55;
    illumination = vec3<f32>(0f, 0f, 0f);
    weight = 1f;
    i = 0i;
    loop {
        let _e56 = i;
        if (_e56 < sunrays_samples) {
            let _e58 = deltaUV;
            let _e59 = uv;
            uv = (_e59 - _e58);
            let _e62 = uv[0u];
            let _e63 = (_e62 < 0f);
            phi_74_ = _e63;
            if !(_e63) {
                let _e66 = uv[0u];
                phi_74_ = (_e66 > 1f);
            }
            let _e69 = phi_74_;
            phi_82_ = _e69;
            if !(_e69) {
                let _e72 = uv[1u];
                phi_82_ = (_e72 < 0f);
            }
            let _e75 = phi_82_;
            phi_89_ = _e75;
            if !(_e75) {
                let _e78 = uv[1u];
                phi_89_ = (_e78 > 1f);
            }
            let _e81 = phi_89_;
            if _e81 {
                break;
            }
            let _e82 = uv;
            let _e83 = textureSample(depthMap, depthMap_sampler, _e82);
            depth = _e83.x;
            let _e85 = depth;
            isSky = step(_e85, 0.001f);
            let _e87 = uv;
            let _e88 = textureSample(texture0_, texture0_sampler, _e87);
            sceneColor = _e88.xyz;
            threshold[0u] = sunrays_threshold;
            threshold[1u] = sunrays_threshold;
            threshold[2u] = sunrays_threshold;
            let _e93 = sceneColor;
            let _e94 = threshold;
            bright = max((_e93 - _e94), vec3<f32>(0f, 0f, 0f));
            let _e97 = bright;
            let _e98 = isSky;
            let _e100 = weight;
            let _e102 = illumination;
            illumination = (_e102 + ((_e97 * _e98) * _e100));
            let _e105 = eb.sunrayDecay;
            let _e106 = weight;
            weight = (_e106 * _e105);
            continue;
        } else {
            break;
        }
        continuing {
            let _e108 = i;
            i = (_e108 + 1i);
        }
    }
    let _e111 = illumination;
    illumination = (_e111 / vec3(f32(sunrays_samples)));
    let _e114 = illumination;
    let _e116 = eb.sunrayIntensity;
    return (_e114 * _e116);
}

fn sampleChromatic_u0028_vf2_u003b(uv_1: ptr<function, vec2<f32>>) -> vec3<f32> {
    var dir: vec2<f32>;
    var radial: f32;
    var offset: vec2<f32>;
    var r: f32;
    var g: f32;
    var b: f32;

    let _e41 = (*uv_1);
    dir = (_e41 - vec2<f32>(0.5f, 0.5f));
    let _e43 = dir;
    radial = length(_e43);
    let _e45 = dir;
    let _e46 = radial;
    offset = (_e45 * ((chromatic_strength * _e46) * 0.015f));
    let _e50 = (*uv_1);
    let _e51 = offset;
    let _e56 = textureSample(texture0_, texture0_sampler, clamp((_e50 + _e51), vec2(0f), vec2(1f)));
    r = _e56.x;
    let _e58 = (*uv_1);
    let _e59 = textureSample(texture0_, texture0_sampler, _e58);
    g = _e59.y;
    let _e61 = (*uv_1);
    let _e62 = offset;
    let _e67 = textureSample(texture0_, texture0_sampler, clamp((_e61 - _e62), vec2(0f), vec2(1f)));
    b = _e67.z;
    let _e69 = r;
    let _e70 = g;
    let _e71 = b;
    return vec3<f32>(_e69, _e70, _e71);
}

fn main_1() {
    var base: vec3<f32>;
    var local: vec3<f32>;
    var param: vec2<f32>;
    var luma: vec3<f32>;

    if (chromatic_strength > 0f) {
        let _e39 = frag_tex_coord_1;
        param = _e39;
        let _e40 = sampleChromatic_u0028_vf2_u003b((&param));
        local = _e40;
    } else {
        let _e41 = frag_tex_coord_1;
        let _e42 = textureSample(texture0_, texture0_sampler, _e41);
        local = _e42.xyz;
    }
    let _e44 = local;
    base = _e44;
    let _e46 = eb.exposure_bias;
    let _e47 = base;
    base = (_e47 * _e46);
    let _e49 = computeSunRays_u0028_();
    let _e50 = base;
    base = (_e50 + _e49);
    if (saturation != 1f) {
        let _e53 = base;
        luma = vec3(dot(_e53, vec3<f32>(0.2126f, 0.7152f, 0.0722f)));
        let _e56 = luma;
        let _e57 = base;
        base = mix(_e56, _e57, vec3(saturation));
    }
    let _e60 = base;
    out_color = vec4<f32>(_e60.x, _e60.y, _e60.z, 1f);
    return;
}

@fragment 
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
