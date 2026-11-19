struct DenoisePush {
    viewportSize: vec2<f32>,
    zNear: f32,
    zFar: f32,
}

@group(0) @binding(4)
var<uniform> pc: DenoisePush;
@group(0) @binding(1)
var depthTex: texture_2d<f32>;
@group(0) @binding(2)
var depthSampler: sampler;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
var<private> gl_WorkGroupID_1: vec3<u32>;
var<private> gl_LocalInvocationIndex_1: u32;
var<workgroup> tile: array<vec2<f32>, 144>;
@group(0) @binding(0)
var aoIn: texture_storage_2d<r16float,read>;
var<private> gl_LocalInvocationID_1: vec3<u32>;
@group(0) @binding(3)
var aoOut: texture_storage_2d<r16float,write>;

fn linearizeDepth_u0028_f1_u003b(z: ptr<function, f32>) -> f32 {
    let _e36 = pc.zNear;
    let _e38 = pc.zFar;
    let _e41 = pc.zNear;
    let _e42 = (*z);
    let _e44 = pc.zFar;
    let _e46 = pc.zNear;
    return ((_e36 * _e38) / (_e41 + (_e42 * (_e44 - _e46))));
}

fn linearAt_u0028_vi2_u003b(p: ptr<function, vec2<i32>>) -> f32 {
    var uv: vec2<f32>;
    var param: f32;

    let _e37 = (*p);
    let _e42 = pc.viewportSize;
    uv = ((vec2<f32>(_e37) + vec2(0.5f)) / _e42);
    let _e44 = uv;
    let _e45 = textureSampleLevel(depthTex, depthSampler, _e44, 0f);
    param = _e45.x;
    let _e47 = linearizeDepth_u0028_f1_u003b((&param));
    return _e47;
}

fn main_1() {
    var pix: vec2<i32>;
    var dim: vec2<i32>;
    var origin: vec2<i32>;
    var i: u32;
    var q: vec2<i32>;
    var param_1: vec2<i32>;
    var local: vec2<i32>;
    var center: i32;
    var centerLin: f32;
    var depthScale: f32;
    var sum: f32;
    var wsum: f32;
    var dy: i32;
    var dx: i32;
    var sampleValue: vec2<f32>;
    var ao: f32;
    var dl: f32;
    var wd: f32;
    var ddiff: f32;
    var we: f32;
    var w: f32;
    var result: f32;
    var local_1: f32;
    var phi_166_: bool;

    let _e57 = gl_GlobalInvocationID_1;
    pix = bitcast<vec2<i32>>(_e57.xy);
    let _e61 = pc.viewportSize;
    dim = vec2<i32>(_e61);
    let _e63 = gl_WorkGroupID_1;
    origin = ((bitcast<vec2<i32>>(_e63.xy) * vec2(8i)) - vec2<i32>(2i, 2i));
    let _e69 = gl_LocalInvocationIndex_1;
    i = _e69;
    loop {
        let _e70 = i;
        if (_e70 < 144u) {
            let _e72 = origin;
            let _e73 = i;
            let _e76 = i;
            let _e81 = dim;
            q = clamp((_e72 + vec2<i32>(bitcast<i32>((_e73 % 12u)), bitcast<i32>((_e76 / 12u)))), vec2<i32>(0i, 0i), (_e81 - vec2(1i)));
            let _e85 = i;
            let _e86 = q;
            let _e87 = textureLoad(aoIn, _e86);
            let _e89 = q;
            param_1 = _e89;
            let _e90 = linearAt_u0028_vi2_u003b((&param_1));
            tile[_e85] = vec2<f32>(_e87.x, _e90);
            continue;
        } else {
            break;
        }
        continuing {
            let _e93 = i;
            i = (_e93 + 64u);
        }
    }
    workgroupBarrier();
    let _e96 = pix[0u];
    let _e98 = dim[0u];
    let _e99 = (_e96 >= _e98);
    phi_166_ = _e99;
    if !(_e99) {
        let _e102 = pix[1u];
        let _e104 = dim[1u];
        phi_166_ = (_e102 >= _e104);
    }
    let _e107 = phi_166_;
    if _e107 {
        return;
    }
    let _e108 = gl_LocalInvocationID_1;
    local = (bitcast<vec2<i32>>(_e108.xy) + vec2<i32>(2i, 2i));
    let _e113 = local[1u];
    let _e116 = local[0u];
    center = ((_e113 * 12i) + _e116);
    let _e118 = center;
    let _e121 = tile[_e118][1u];
    centerLin = _e121;
    let _e122 = centerLin;
    depthScale = (1f / max((_e122 * 0.05f), 0.001f));
    sum = 0f;
    wsum = 0f;
    dy = -2i;
    loop {
        let _e126 = dy;
        if (_e126 <= 2i) {
            dx = -2i;
            loop {
                let _e128 = dx;
                if (_e128 <= 2i) {
                    let _e130 = center;
                    let _e131 = dy;
                    let _e134 = dx;
                    let _e137 = tile[((_e130 + (_e131 * 12i)) + _e134)];
                    sampleValue = _e137;
                    let _e139 = sampleValue[0u];
                    ao = _e139;
                    let _e141 = sampleValue[1u];
                    dl = _e141;
                    let _e142 = dx;
                    let _e143 = dx;
                    let _e145 = dy;
                    let _e146 = dy;
                    wd = exp((-(f32(((_e142 * _e143) + (_e145 * _e146)))) * 0.5f));
                    let _e153 = dl;
                    let _e154 = centerLin;
                    let _e157 = depthScale;
                    ddiff = (abs((_e153 - _e154)) * _e157);
                    let _e159 = ddiff;
                    let _e161 = ddiff;
                    we = exp((-(_e159) * _e161));
                    let _e164 = wd;
                    let _e165 = we;
                    w = (_e164 * _e165);
                    let _e167 = ao;
                    let _e168 = w;
                    let _e170 = sum;
                    sum = (_e170 + (_e167 * _e168));
                    let _e172 = w;
                    let _e173 = wsum;
                    wsum = (_e173 + _e172);
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e175 = dx;
                    dx = (_e175 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e177 = dy;
            dy = (_e177 + 1i);
        }
    }
    let _e179 = wsum;
    if (_e179 > 0.00001f) {
        let _e181 = sum;
        let _e182 = wsum;
        local_1 = (_e181 / _e182);
    } else {
        let _e184 = center;
        let _e187 = tile[_e184][0u];
        local_1 = _e187;
    }
    let _e188 = local_1;
    result = _e188;
    let _e189 = pix;
    let _e190 = result;
    textureStore(aoOut, _e189, vec4(_e190));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>, @builtin(workgroup_id) gl_WorkGroupID: vec3<u32>, @builtin(local_invocation_index) gl_LocalInvocationIndex: u32, @builtin(local_invocation_id) gl_LocalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    gl_WorkGroupID_1 = gl_WorkGroupID;
    gl_LocalInvocationIndex_1 = gl_LocalInvocationIndex;
    gl_LocalInvocationID_1 = gl_LocalInvocationID;
    main_1();
}
