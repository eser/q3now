enable wgpu_binding_array;

var<private> in_packed_1: u32;
@group(2) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(2) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> in_texcoord_1: vec2<f32>;

fn main_1() {
    var atFunc: u32;
    var imgSlot: u32;
    var smpSlot: u32;
    var a: f32;

    let _e17 = in_packed_1;
    atFunc = ((_e17 >> bitcast<u32>(24i)) & 255u);
    let _e21 = in_packed_1;
    imgSlot = (_e21 & 4095u);
    let _e23 = in_packed_1;
    smpSlot = ((_e23 >> bitcast<u32>(12i)) & 255u);
    let _e27 = imgSlot;
    let _e29 = smpSlot;
    let _e31 = in_texcoord_1;
    let _e32 = textureSample(wired_bindless_images[_e27], wired_bindless_samplers[_e29], _e31);
    a = _e32.w;
    let _e34 = atFunc;
    if (_e34 == 1u) {
        let _e36 = a;
        if (_e36 == 0f) {
            discard;
        }
    } else {
        let _e38 = atFunc;
        if (_e38 == 2u) {
            let _e40 = a;
            if (_e40 >= 0.5f) {
                discard;
            }
        } else {
            let _e42 = atFunc;
            if (_e42 == 3u) {
                let _e44 = a;
                if (_e44 < 0.5f) {
                    discard;
                }
            }
        }
    }
    return;
}

@fragment
fn main(@location(1) @interpolate(flat) in_packed: u32, @location(0) in_texcoord: vec2<f32>) {
    in_packed_1 = in_packed;
    in_texcoord_1 = in_texcoord;
    main_1();
}
