struct TemporalHistoryPush {
    extent: vec2<u32>,
    zNear: f32,
    zFar: f32,
}

@group(0) @binding(5)
var<uniform> pc: TemporalHistoryPush;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(0) 
var currentColor: texture_2d<f32>;
@group(0) @binding(2) 
var nearestSampler: sampler;
@group(0) @binding(1) 
var currentDeviceDepth: texture_2d<f32>;
@group(0) @binding(3) 
var outputColor: texture_storage_2d<rgba16float,write>;
@group(0) @binding(4) 
var outputLinearDepth: texture_storage_2d<r32float,write>;

fn linearizeDepth_u0028_f1_u003b(z: ptr<function, f32>) -> f32 {
    let _e16 = pc.zNear;
    let _e18 = pc.zFar;
    let _e21 = pc.zNear;
    let _e22 = (*z);
    let _e24 = pc.zFar;
    let _e26 = pc.zNear;
    return ((_e16 * _e18) / (_e21 + (_e22 * (_e24 - _e26))));
}

fn main_1() {
    var pixel: vec2<i32>;
    var color: vec4<f32>;
    var depth: f32;
    var param: f32;

    let _e18 = gl_GlobalInvocationID_1;
    pixel = bitcast<vec2<i32>>(_e18.xy);
    let _e21 = pixel;
    let _e23 = pc.extent;
    if any((_e21 >= bitcast<vec2<i32>>(_e23))) {
        return;
    }
    let _e27 = pixel;
    let _e28 = textureLoad(currentColor, _e27, 0i);
    color = _e28;
    let _e29 = pixel;
    let _e30 = textureLoad(currentDeviceDepth, _e29, 0i);
    param = _e30.x;
    let _e32 = linearizeDepth_u0028_f1_u003b((&param));
    depth = _e32;
    let _e33 = pixel;
    let _e34 = color;
    textureStore(outputColor, _e33, _e34);
    let _e35 = pixel;
    let _e36 = depth;
    textureStore(outputLinearDepth, _e35, vec4(_e36));
    return;
}

@compute @workgroup_size(8, 8, 1) 
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
