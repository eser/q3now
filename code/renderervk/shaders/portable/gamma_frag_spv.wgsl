@id(8) override depth_r: i32 = 255i;
@id(9) override depth_g: i32 = 255i;
@id(10) override depth_b: i32 = 255i;
@id(7) override ditherMode: i32 = 0i;
override override_type_21_: bool = (ditherMode == 2i);
@id(0) override gamma: f32 = 1f;
@id(12) override hdr_mode: i32 = 0i;
override override_type_21_1: bool = (hdr_mode == 1i);
@id(11) override srgb_swapchain: i32 = 0i;
override override_type_21_2: bool = (srgb_swapchain == 1i);
override override_type_21_3: bool = (ditherMode != 0i);
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

    let _e131 = gl_FragCoord_1;
    coordDenormalized = vec2<u32>(_e131.xy);
    let _e134 = coordDenormalized;
    bayerCoord = (_e134 % vec2(8u));
    let _e138 = bayerCoord[0u];
    let _e140 = bayerCoord[1u];
    bayerIndex = (_e138 + (_e140 * 8u));
    let _e143 = bayerIndex;
    indexable = array<f32, 64>(0f, 32f, 8f, 40f, 2f, 34f, 10f, 42f, 48f, 16f, 56f, 24f, 50f, 18f, 58f, 26f, 12f, 44f, 4f, 36f, 14f, 46f, 6f, 38f, 60f, 28f, 52f, 20f, 62f, 30f, 54f, 22f, 3f, 35f, 11f, 43f, 1f, 33f, 9f, 41f, 51f, 19f, 59f, 27f, 49f, 17f, 57f, 25f, 15f, 47f, 7f, 39f, 13f, 45f, 5f, 37f, 63f, 31f, 55f, 23f, 61f, 29f, 53f, 21f);
    let _e145 = indexable[_e143];
    bayerSample = _e145;
    let _e146 = bayerSample;
    threshold = ((_e146 + 0.5f) / 64f);
    let _e149 = threshold;
    return _e149;
}

fn blueNoiseThreshold_u0028_() -> f32 {
    var tile: vec2<f32>;
    var uv: vec2<f32>;

    let _e127 = textureDimensions(blueNoiseTex, 0i);
    tile = vec2<f32>(vec2<i32>(_e127));
    let _e130 = gl_FragCoord_1;
    let _e134 = tile;
    uv = ((_e130.xy + vec2(0.5f)) / _e134);
    let _e136 = uv;
    let _e137 = textureSample(blueNoiseTex, blueNoiseTex_sampler, _e136);
    return _e137.x;
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
        let _e136 = blueNoiseThreshold_u0028_();
        local = _e136;
    } else {
        let _e137 = orderedThreshold_u0028_();
        local = _e137;
    }
    let _e138 = local;
    t = _e138;
    let _e139 = (*color);
    let _e140 = depth;
    cDenormalized = (_e139 * vec3<f32>(_e140));
    let _e143 = cDenormalized;
    cLow = floor(_e143);
    let _e145 = cDenormalized;
    let _e146 = cLow;
    cFractional = (_e145 - _e146);
    let _e148 = cLow;
    let _e149 = t;
    let _e150 = cFractional;
    cDithered = (_e148 + step(vec3(_e149), _e150));
    let _e154 = cDithered;
    let _e155 = depth;
    return (_e154 / vec3<f32>(_e155));
}

fn sRGBEncode_u0028_vf3_u003b(linear: ptr<function, vec3<f32>>) -> vec3<f32> {
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e128 = (*linear);
    (*linear) = clamp(_e128, vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f));
    let _e130 = (*linear);
    lo = (_e130 * 12.92f);
    let _e132 = (*linear);
    hi = ((pow(_e132, vec3<f32>(0.41666666f, 0.41666666f, 0.41666666f)) * 1.055f) - vec3(0.055f));
    let _e137 = hi;
    let _e138 = lo;
    let _e139 = (*linear);
    return select(_e137, _e138, (_e139 < vec3<f32>(0.0031308f, 0.0031308f, 0.0031308f)));
}

fn pqEncode_u0028_vf3_u003b(L: ptr<function, vec3<f32>>) -> vec3<f32> {
    var Lm1_: vec3<f32>;

    let _e127 = (*L);
    (*L) = clamp(_e127, vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f));
    let _e129 = (*L);
    Lm1_ = pow(_e129, vec3<f32>(0.15930176f, 0.15930176f, 0.15930176f));
    let _e131 = Lm1_;
    let _e135 = Lm1_;
    return pow(((vec3(0.8359375f) + (_e131 * 18.851563f)) / (vec3(1f) + (_e135 * 18.6875f))), vec3<f32>(78.84375f, 78.84375f, 78.84375f));
}

fn main_1() {
    var base: vec3<f32>;
    var gamma3_: vec3<f32>;
    var lin: vec3<f32>;
    var local_1: vec3<f32>;
    var nits: vec3<f32>;
    var param: vec3<f32>;
    var curved: vec3<f32>;
    var local_2: vec3<f32>;
    var param_1: vec3<f32>;
    var param_2: vec3<f32>;

    let _e135 = frag_tex_coord_1;
    let _e136 = textureSample(texture0_, texture0_sampler, _e135);
    base = _e136.xyz;
    gamma3_[0u] = gamma;
    gamma3_[1u] = gamma;
    gamma3_[2u] = gamma;
    if override_type_21_1 {
        if (gamma != 1f) {
            let _e142 = base;
            let _e144 = gamma3_;
            local_1 = pow(max(_e142, vec3<f32>(0f, 0f, 0f)), _e144);
        } else {
            let _e146 = base;
            local_1 = max(_e146, vec3<f32>(0f, 0f, 0f));
        }
        let _e148 = local_1;
        lin = _e148;
        let _e149 = lin;
        nits = (_e149 * 100f);
        let _e151 = nits;
        nits = (mat3x3<f32>(vec3<f32>(0.6274039f, 0.06909729f, 0.01639144f), vec3<f32>(0.32928303f, 0.9195404f, 0.088013306f), vec3<f32>(0.043313067f, 0.011362315f, 0.89559525f)) * _e151);
        let _e153 = nits;
        param = (_e153 / vec3(10000f));
        let _e156 = pqEncode_u0028_vf3_u003b((&param));
        out_color = vec4<f32>(_e156.x, _e156.y, _e156.z, 1f);
    } else {
        if override_type_21_2 {
            if (gamma != 1f) {
                let _e162 = base;
                let _e163 = gamma3_;
                let _e164 = pow(_e162, _e163);
                out_color = vec4<f32>(_e164.x, _e164.y, _e164.z, 1f);
            } else {
                let _e169 = base;
                out_color = vec4<f32>(_e169.x, _e169.y, _e169.z, 1f);
            }
        } else {
            if (gamma != 1f) {
                let _e175 = base;
                let _e176 = gamma3_;
                local_2 = pow(_e175, _e176);
            } else {
                let _e178 = base;
                local_2 = _e178;
            }
            let _e179 = local_2;
            curved = _e179;
            let _e180 = curved;
            param_1 = _e180;
            let _e181 = sRGBEncode_u0028_vf3_u003b((&param_1));
            out_color = vec4<f32>(_e181.x, _e181.y, _e181.z, 1f);
        }
    }
    if override_type_21_3 {
        let _e186 = out_color;
        param_2 = _e186.xyz;
        let _e188 = dither_u0028_vf3_u003b((&param_2));
        out_color[0u] = _e188.x;
        out_color[1u] = _e188.y;
        out_color[2u] = _e188.z;
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
