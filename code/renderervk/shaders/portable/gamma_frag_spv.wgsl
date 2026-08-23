@id(8) override depth_r: i32 = 255i;
@id(9) override depth_g: i32 = 255i;
@id(10) override depth_b: i32 = 255i;
@id(7) override ditherMode: i32 = 0i;
override override_type_21_: bool = (ditherMode == 2i);
@id(14) override capture_hdr_to_sdr: i32 = 0i;
override override_type_21_1: bool = (capture_hdr_to_sdr == 1i);
@id(0) override gamma: f32 = 1f;
@id(12) override hdr_mode: i32 = 0i;
override override_type_21_2: bool = (hdr_mode == 1i);
@id(11) override srgb_swapchain: i32 = 0i;
override override_type_21_3: bool = (srgb_swapchain == 1i);
override override_type_21_4: bool = (ditherMode != 0i);
@id(13) override hdr_peak_norm: f32 = 10f;

var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0) 
var blueNoiseTex: texture_2d<f32>;
@group(1) @binding(32) 
var blueNoiseTex_sampler: sampler;
@group(0) @binding(0) 
var texture0_: texture_2d<f32>;
@group(0) @binding(32) 
var texture0_sampler: sampler;
var<private> frag_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn orderedThreshold_u0028_() -> f32 {
    var coordDenormalized: vec2<u32>;
    var bayerCoord: vec2<u32>;
    var bayerIndex: u32;
    var bayerSample: f32;
    var indexable: array<f32, 64>;
    var threshold: f32;

    let _e140 = gl_FragCoord_1;
    coordDenormalized = vec2<u32>(_e140.xy);
    let _e143 = coordDenormalized;
    bayerCoord = (_e143 % vec2(8u));
    let _e147 = bayerCoord[0u];
    let _e149 = bayerCoord[1u];
    bayerIndex = (_e147 + (_e149 * 8u));
    let _e152 = bayerIndex;
    indexable = array<f32, 64>(0f, 32f, 8f, 40f, 2f, 34f, 10f, 42f, 48f, 16f, 56f, 24f, 50f, 18f, 58f, 26f, 12f, 44f, 4f, 36f, 14f, 46f, 6f, 38f, 60f, 28f, 52f, 20f, 62f, 30f, 54f, 22f, 3f, 35f, 11f, 43f, 1f, 33f, 9f, 41f, 51f, 19f, 59f, 27f, 49f, 17f, 57f, 25f, 15f, 47f, 7f, 39f, 13f, 45f, 5f, 37f, 63f, 31f, 55f, 23f, 61f, 29f, 53f, 21f);
    let _e154 = indexable[_e152];
    bayerSample = _e154;
    let _e155 = bayerSample;
    threshold = ((_e155 + 0.5f) / 64f);
    let _e158 = threshold;
    return _e158;
}

fn blueNoiseThreshold_u0028_() -> f32 {
    var tile: vec2<f32>;
    var uv: vec2<f32>;

    let _e136 = textureDimensions(blueNoiseTex, 0i);
    tile = vec2<f32>(vec2<i32>(_e136));
    let _e139 = gl_FragCoord_1;
    let _e143 = tile;
    uv = ((_e139.xy + vec2(0.5f)) / _e143);
    let _e145 = uv;
    let _e146 = textureSample(blueNoiseTex, blueNoiseTex_sampler, _e145);
    return _e146.x;
}

fn dither_u0028_vf3_u003b(color: ptr<function, vec3<f32>>) -> vec3<f32> {
    var depth: vec3<i32>;
    var t: f32;
    var local: f32;
    var cDenormalized: vec3<f32>;
    var cLow: vec3<f32>;
    var cFractional: vec3<f32>;
    var cDithered: vec3<f32>;

    depth[0u] = depth_r;
    depth[1u] = depth_g;
    depth[2u] = depth_b;
    if override_type_21_ {
        let _e145 = blueNoiseThreshold_u0028_();
        local = _e145;
    } else {
        let _e146 = orderedThreshold_u0028_();
        local = _e146;
    }
    let _e147 = local;
    t = _e147;
    let _e148 = (*color);
    let _e149 = depth;
    cDenormalized = (_e148 * vec3<f32>(_e149));
    let _e152 = cDenormalized;
    cLow = floor(_e152);
    let _e154 = cDenormalized;
    let _e155 = cLow;
    cFractional = (_e154 - _e155);
    let _e157 = cLow;
    let _e158 = t;
    let _e159 = cFractional;
    cDithered = (_e157 + step(vec3(_e158), _e159));
    let _e163 = cDithered;
    let _e164 = depth;
    return (_e163 / vec3<f32>(_e164));
}

fn sRGBEncode_u0028_vf3_u003b(linear: ptr<function, vec3<f32>>) -> vec3<f32> {
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e137 = (*linear);
    (*linear) = clamp(_e137, vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f));
    let _e139 = (*linear);
    lo = (_e139 * 12.92f);
    let _e141 = (*linear);
    hi = ((pow(_e141, vec3<f32>(0.41666666f, 0.41666666f, 0.41666666f)) * 1.055f) - vec3(0.055f));
    let _e146 = hi;
    let _e147 = lo;
    let _e148 = (*linear);
    return select(_e146, _e147, (_e148 < vec3<f32>(0.0031308f, 0.0031308f, 0.0031308f)));
}

fn pqEncode_u0028_vf3_u003b(L: ptr<function, vec3<f32>>) -> vec3<f32> {
    var Lm1_: vec3<f32>;

    let _e136 = (*L);
    (*L) = clamp(_e136, vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f));
    let _e138 = (*L);
    Lm1_ = pow(_e138, vec3<f32>(0.15930176f, 0.15930176f, 0.15930176f));
    let _e140 = Lm1_;
    let _e144 = Lm1_;
    return pow(((vec3(0.8359375f) + (_e140 * 18.851563f)) / (vec3(1f) + (_e144 * 18.6875f))), vec3<f32>(78.84375f, 78.84375f, 78.84375f));
}

fn tonemapPBRNeutralSDR_u0028_vf3_u003b(color_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x: f32;
    var offset: f32;
    var local_1: f32;
    var peak: f32;
    var newPeak: f32;
    var g: f32;

    let _e142 = (*color_1)[0u];
    let _e144 = (*color_1)[1u];
    let _e146 = (*color_1)[2u];
    x = min(_e142, min(_e144, _e146));
    let _e149 = x;
    if (_e149 < 0.08f) {
        let _e151 = x;
        let _e152 = x;
        let _e154 = x;
        local_1 = (_e151 - ((6.25f * _e152) * _e154));
    } else {
        local_1 = 0.04f;
    }
    let _e157 = local_1;
    offset = _e157;
    let _e158 = offset;
    let _e159 = (*color_1);
    (*color_1) = (_e159 - vec3(_e158));
    let _e163 = (*color_1)[0u];
    let _e165 = (*color_1)[1u];
    let _e167 = (*color_1)[2u];
    peak = max(_e163, max(_e165, _e167));
    let _e170 = peak;
    if (_e170 < 0.76f) {
        let _e172 = (*color_1);
        return _e172;
    }
    let _e173 = peak;
    newPeak = (1f - (0.0576f / ((_e173 + 0.24f) - 0.76f)));
    let _e178 = newPeak;
    let _e179 = peak;
    let _e181 = (*color_1);
    (*color_1) = (_e181 * (_e178 / _e179));
    let _e183 = peak;
    let _e184 = newPeak;
    g = (1f - (1f / ((0.15f * (_e183 - _e184)) + 1f)));
    let _e190 = (*color_1);
    let _e191 = newPeak;
    let _e193 = g;
    return mix(_e190, (vec3<f32>(1f, 1f, 1f) * _e191), vec3(_e193));
}

fn main_1() {
    var base: vec3<f32>;
    var param: vec3<f32>;
    var gamma3_: vec3<f32>;
    var lin: vec3<f32>;
    var local_2: vec3<f32>;
    var nits: vec3<f32>;
    var param_1: vec3<f32>;
    var curved: vec3<f32>;
    var local_3: vec3<f32>;
    var param_2: vec3<f32>;
    var param_3: vec3<f32>;

    let _e145 = frag_tex_coord_1;
    let _e146 = textureSample(texture0_, texture0_sampler, _e145);
    base = _e146.xyz;
    if override_type_21_1 {
        let _e148 = base;
        param = max(_e148, vec3<f32>(0f, 0f, 0f));
        let _e150 = tonemapPBRNeutralSDR_u0028_vf3_u003b((&param));
        base = _e150;
    }
    gamma3_[0u] = gamma;
    gamma3_[1u] = gamma;
    gamma3_[2u] = gamma;
    if override_type_21_2 {
        if (gamma != 1f) {
            let _e155 = base;
            let _e157 = gamma3_;
            local_2 = pow(max(_e155, vec3<f32>(0f, 0f, 0f)), _e157);
        } else {
            let _e159 = base;
            local_2 = max(_e159, vec3<f32>(0f, 0f, 0f));
        }
        let _e161 = local_2;
        lin = _e161;
        let _e162 = lin;
        nits = (_e162 * 100f);
        let _e164 = nits;
        nits = (mat3x3<f32>(vec3<f32>(0.6274039f, 0.06909729f, 0.01639144f), vec3<f32>(0.32928303f, 0.9195404f, 0.088013306f), vec3<f32>(0.043313067f, 0.011362315f, 0.89559525f)) * _e164);
        let _e166 = nits;
        param_1 = (_e166 / vec3(10000f));
        let _e169 = pqEncode_u0028_vf3_u003b((&param_1));
        out_color = vec4<f32>(_e169.x, _e169.y, _e169.z, 1f);
    } else {
        if override_type_21_3 {
            if (gamma != 1f) {
                let _e175 = base;
                let _e176 = gamma3_;
                let _e177 = pow(_e175, _e176);
                out_color = vec4<f32>(_e177.x, _e177.y, _e177.z, 1f);
            } else {
                let _e182 = base;
                out_color = vec4<f32>(_e182.x, _e182.y, _e182.z, 1f);
            }
        } else {
            if (gamma != 1f) {
                let _e188 = base;
                let _e189 = gamma3_;
                local_3 = pow(_e188, _e189);
            } else {
                let _e191 = base;
                local_3 = _e191;
            }
            let _e192 = local_3;
            curved = _e192;
            let _e193 = curved;
            param_2 = _e193;
            let _e194 = sRGBEncode_u0028_vf3_u003b((&param_2));
            out_color = vec4<f32>(_e194.x, _e194.y, _e194.z, 1f);
        }
    }
    if override_type_21_4 {
        let _e199 = out_color;
        param_3 = _e199.xyz;
        let _e201 = dither_u0028_vf3_u003b((&param_3));
        out_color[0u] = _e201.x;
        out_color[1u] = _e201.y;
        out_color[2u] = _e201.z;
    }
    return;
}

@fragment 
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e5 = out_color;
    return _e5;
}
