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
@group(0) @binding(0) 
var aoIn: texture_storage_2d<r16float,read>;
@group(0) @binding(3) 
var aoOut: texture_storage_2d<r16float,write>;

fn linearizeDepth_u0028_f1_u003b(z: ptr<function, f32>) -> f32 {
    let _e24 = pc.zNear;
    let _e26 = pc.zFar;
    let _e29 = pc.zNear;
    let _e30 = (*z);
    let _e32 = pc.zFar;
    let _e34 = pc.zNear;
    return ((_e24 * _e26) / (_e29 + (_e30 * (_e32 - _e34))));
}

fn linearAt_u0028_vi2_u003b(p: ptr<function, vec2<i32>>) -> f32 {
    var uv: vec2<f32>;
    var param: f32;

    let _e25 = (*p);
    let _e30 = pc.viewportSize;
    uv = ((vec2<f32>(_e25) + vec2(0.5f)) / _e30);
    let _e32 = uv;
    let _e33 = textureSampleLevel(depthTex, depthSampler, _e32, 0f);
    param = _e33.x;
    let _e35 = linearizeDepth_u0028_f1_u003b((&param));
    return _e35;
}

fn main_1() {
    var pix: vec2<i32>;
    var dim: vec2<i32>;
    var centerLin: f32;
    var param_1: vec2<i32>;
    var depthScale: f32;
    var sum: f32;
    var wsum: f32;
    var dy: i32;
    var dx: i32;
    var q: vec2<i32>;
    var ao: f32;
    var dl: f32;
    var param_2: vec2<i32>;
    var wd: f32;
    var ddiff: f32;
    var we: f32;
    var w: f32;
    var result: f32;
    var local: f32;
    var phi_105_: bool;

    let _e41 = gl_GlobalInvocationID_1;
    pix = bitcast<vec2<i32>>(_e41.xy);
    let _e45 = pc.viewportSize;
    dim = vec2<i32>(_e45);
    let _e48 = pix[0u];
    let _e50 = dim[0u];
    let _e51 = (_e48 >= _e50);
    phi_105_ = _e51;
    if !(_e51) {
        let _e54 = pix[1u];
        let _e56 = dim[1u];
        phi_105_ = (_e54 >= _e56);
    }
    let _e59 = phi_105_;
    if _e59 {
        return;
    }
    let _e60 = pix;
    param_1 = _e60;
    let _e61 = linearAt_u0028_vi2_u003b((&param_1));
    centerLin = _e61;
    let _e62 = centerLin;
    depthScale = (1f / max((_e62 * 0.05f), 0.001f));
    sum = 0f;
    wsum = 0f;
    dy = -2i;
    loop {
        let _e66 = dy;
        if (_e66 <= 2i) {
            dx = -2i;
            loop {
                let _e68 = dx;
                if (_e68 <= 2i) {
                    let _e70 = pix;
                    let _e71 = dx;
                    let _e72 = dy;
                    let _e75 = dim;
                    q = clamp((_e70 + vec2<i32>(_e71, _e72)), vec2<i32>(0i, 0i), (_e75 - vec2(1i)));
                    let _e79 = q;
                    let _e80 = textureLoad(aoIn, _e79);
                    ao = _e80.x;
                    let _e82 = q;
                    param_2 = _e82;
                    let _e83 = linearAt_u0028_vi2_u003b((&param_2));
                    dl = _e83;
                    let _e84 = dx;
                    let _e85 = dx;
                    let _e87 = dy;
                    let _e88 = dy;
                    wd = exp((-(f32(((_e84 * _e85) + (_e87 * _e88)))) * 0.5f));
                    let _e95 = dl;
                    let _e96 = centerLin;
                    let _e99 = depthScale;
                    ddiff = (abs((_e95 - _e96)) * _e99);
                    let _e101 = ddiff;
                    let _e103 = ddiff;
                    we = exp((-(_e101) * _e103));
                    let _e106 = wd;
                    let _e107 = we;
                    w = (_e106 * _e107);
                    let _e109 = ao;
                    let _e110 = w;
                    let _e112 = sum;
                    sum = (_e112 + (_e109 * _e110));
                    let _e114 = w;
                    let _e115 = wsum;
                    wsum = (_e115 + _e114);
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e117 = dx;
                    dx = (_e117 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e119 = dy;
            dy = (_e119 + 1i);
        }
    }
    let _e121 = wsum;
    if (_e121 > 0.00001f) {
        let _e123 = sum;
        let _e124 = wsum;
        local = (_e123 / _e124);
    } else {
        let _e126 = pix;
        let _e127 = textureLoad(aoIn, _e126);
        local = _e127.x;
    }
    let _e129 = local;
    result = _e129;
    let _e130 = pix;
    let _e131 = result;
    textureStore(aoOut, _e130, vec4(_e131));
    return;
}

@compute @workgroup_size(8, 8, 1) 
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
