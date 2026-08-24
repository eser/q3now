@id(0) override texel_x: f32 = 0f;
@id(1) override texel_y: f32 = 0f;
@id(2) override filter_radius: f32 = 1f;

var<private> frag_tex_coord_1: vec2<f32>;
@group(0) @binding(0)
var texture0_: texture_2d<f32>;
@group(0) @binding(32)
var texture0_sampler: sampler;
var<private> out_color: vec4<f32>;

fn main_1() {
    var uv: vec2<f32>;
    var r: vec2<f32>;
    var s: vec3<f32>;

    let _e26 = frag_tex_coord_1;
    uv = _e26;
    r[0u] = texel_x;
    r[1u] = texel_y;
    let _e29 = r;
    r = (_e29 * filter_radius);
    let _e31 = uv;
    let _e32 = r;
    let _e35 = textureSample(texture0_, texture0_sampler, (_e31 + (_e32 * vec2<f32>(-1f, -1f))));
    s = _e35.xyz;
    let _e37 = uv;
    let _e38 = r;
    let _e41 = textureSample(texture0_, texture0_sampler, (_e37 + (_e38 * vec2<f32>(0f, -1f))));
    let _e44 = s;
    s = (_e44 + (_e41.xyz * 2f));
    let _e46 = uv;
    let _e47 = r;
    let _e50 = textureSample(texture0_, texture0_sampler, (_e46 + (_e47 * vec2<f32>(1f, -1f))));
    let _e52 = s;
    s = (_e52 + _e50.xyz);
    let _e54 = uv;
    let _e55 = r;
    let _e58 = textureSample(texture0_, texture0_sampler, (_e54 + (_e55 * vec2<f32>(-1f, 0f))));
    let _e61 = s;
    s = (_e61 + (_e58.xyz * 2f));
    let _e63 = uv;
    let _e64 = textureSample(texture0_, texture0_sampler, _e63);
    let _e67 = s;
    s = (_e67 + (_e64.xyz * 4f));
    let _e69 = uv;
    let _e70 = r;
    let _e73 = textureSample(texture0_, texture0_sampler, (_e69 + (_e70 * vec2<f32>(1f, 0f))));
    let _e76 = s;
    s = (_e76 + (_e73.xyz * 2f));
    let _e78 = uv;
    let _e79 = r;
    let _e82 = textureSample(texture0_, texture0_sampler, (_e78 + (_e79 * vec2<f32>(-1f, 1f))));
    let _e84 = s;
    s = (_e84 + _e82.xyz);
    let _e86 = uv;
    let _e87 = r;
    let _e90 = textureSample(texture0_, texture0_sampler, (_e86 + (_e87 * vec2<f32>(0f, 1f))));
    let _e93 = s;
    s = (_e93 + (_e90.xyz * 2f));
    let _e95 = uv;
    let _e96 = r;
    let _e99 = textureSample(texture0_, texture0_sampler, (_e95 + (_e96 * vec2<f32>(1f, 1f))));
    let _e101 = s;
    s = (_e101 + _e99.xyz);
    let _e103 = s;
    let _e104 = (_e103 * 0.0625f);
    out_color = vec4<f32>(_e104.x, _e104.y, _e104.z, 1f);
    return;
}

@fragment
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
