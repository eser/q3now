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
    shadowExponent: f32,
    shadowPivot: f32,
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

    let _e46 = frag_tex_coord_1;
    let _e48 = eb.sunScreenX;
    let _e50 = eb.sunScreenY;
    deltaUV = (((_e46 - vec2<f32>(_e48, _e50)) * sunrays_density) / vec2(f32(sunrays_samples)));
    let _e57 = frag_tex_coord_1;
    uv = _e57;
    illumination = vec3<f32>(0f, 0f, 0f);
    weight = 1f;
    i = 0i;
    loop {
        let _e58 = i;
        if (_e58 < sunrays_samples) {
            let _e60 = deltaUV;
            let _e61 = uv;
            uv = (_e61 - _e60);
            let _e64 = uv[0u];
            let _e65 = (_e64 < 0f);
            phi_74_ = _e65;
            if !(_e65) {
                let _e68 = uv[0u];
                phi_74_ = (_e68 > 1f);
            }
            let _e71 = phi_74_;
            phi_82_ = _e71;
            if !(_e71) {
                let _e74 = uv[1u];
                phi_82_ = (_e74 < 0f);
            }
            let _e77 = phi_82_;
            phi_89_ = _e77;
            if !(_e77) {
                let _e80 = uv[1u];
                phi_89_ = (_e80 > 1f);
            }
            let _e83 = phi_89_;
            if _e83 {
                break;
            }
            let _e84 = uv;
            let _e85 = textureSample(depthMap, depthMap_sampler, _e84);
            depth = _e85.x;
            let _e87 = depth;
            isSky = step(_e87, 0.001f);
            let _e89 = uv;
            let _e90 = textureSample(texture0_, texture0_sampler, _e89);
            sceneColor = _e90.xyz;
            threshold[0u] = sunrays_threshold;
            threshold[1u] = sunrays_threshold;
            threshold[2u] = sunrays_threshold;
            let _e95 = sceneColor;
            let _e96 = threshold;
            bright = max((_e95 - _e96), vec3<f32>(0f, 0f, 0f));
            let _e99 = bright;
            let _e100 = isSky;
            let _e102 = weight;
            let _e104 = illumination;
            illumination = (_e104 + ((_e99 * _e100) * _e102));
            let _e107 = eb.sunrayDecay;
            let _e108 = weight;
            weight = (_e108 * _e107);
            continue;
        } else {
            break;
        }
        continuing {
            let _e110 = i;
            i = (_e110 + 1i);
        }
    }
    let _e113 = illumination;
    illumination = (_e113 / vec3(f32(sunrays_samples)));
    let _e116 = illumination;
    let _e118 = eb.sunrayIntensity;
    return (_e116 * _e118);
}

fn sampleChromatic_u0028_vf2_u003b(uv_1: ptr<function, vec2<f32>>) -> vec3<f32> {
    var dir: vec2<f32>;
    var radial: f32;
    var offset: vec2<f32>;
    var r: f32;
    var g: f32;
    var b: f32;

    let _e43 = (*uv_1);
    dir = (_e43 - vec2<f32>(0.5f, 0.5f));
    let _e45 = dir;
    radial = length(_e45);
    let _e47 = dir;
    let _e48 = radial;
    offset = (_e47 * ((chromatic_strength * _e48) * 0.015f));
    let _e52 = (*uv_1);
    let _e53 = offset;
    let _e58 = textureSample(texture0_, texture0_sampler, clamp((_e52 + _e53), vec2(0f), vec2(1f)));
    r = _e58.x;
    let _e60 = (*uv_1);
    let _e61 = textureSample(texture0_, texture0_sampler, _e60);
    g = _e61.y;
    let _e63 = (*uv_1);
    let _e64 = offset;
    let _e69 = textureSample(texture0_, texture0_sampler, clamp((_e63 - _e64), vec2(0f), vec2(1f)));
    b = _e69.z;
    let _e71 = r;
    let _e72 = g;
    let _e73 = b;
    return vec3<f32>(_e71, _e72, _e73);
}

fn main_1() {
    var base: vec3<f32>;
    var local: vec3<f32>;
    var param: vec2<f32>;
    var shadowLuma: f32;
    var normalized: f32;
    var curved: f32;
    var luma: vec3<f32>;
    var phi_237_: bool;

    if (chromatic_strength > 0f) {
        let _e44 = frag_tex_coord_1;
        param = _e44;
        let _e45 = sampleChromatic_u0028_vf2_u003b((&param));
        local = _e45;
    } else {
        let _e46 = frag_tex_coord_1;
        let _e47 = textureSample(texture0_, texture0_sampler, _e46);
        local = _e47.xyz;
    }
    let _e49 = local;
    base = _e49;
    let _e51 = eb.exposure_bias;
    let _e52 = base;
    base = (_e52 * _e51);
    let _e54 = base;
    shadowLuma = dot(max(_e54, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e58 = eb.shadowExponent;
    let _e60 = shadowLuma;
    let _e62 = ((_e58 != 1f) && (_e60 > 0f));
    phi_237_ = _e62;
    if _e62 {
        let _e63 = shadowLuma;
        let _e65 = eb.shadowPivot;
        phi_237_ = (_e63 < _e65);
    }
    let _e68 = phi_237_;
    if _e68 {
        let _e69 = shadowLuma;
        let _e71 = eb.shadowPivot;
        normalized = (_e69 / _e71);
        let _e73 = normalized;
        let _e75 = eb.shadowExponent;
        let _e78 = eb.shadowPivot;
        curved = (pow(_e73, _e75) * _e78);
        let _e80 = curved;
        let _e81 = shadowLuma;
        let _e83 = base;
        base = (_e83 * (_e80 / _e81));
    }
    let _e85 = computeSunRays_u0028_();
    let _e86 = base;
    base = (_e86 + _e85);
    if (saturation != 1f) {
        let _e89 = base;
        luma = vec3(dot(_e89, vec3<f32>(0.2126f, 0.7152f, 0.0722f)));
        let _e92 = luma;
        let _e93 = base;
        base = mix(_e92, _e93, vec3(saturation));
    }
    let _e96 = base;
    out_color = vec4<f32>(_e96.x, _e96.y, _e96.z, 1f);
    return;
}

@fragment
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
