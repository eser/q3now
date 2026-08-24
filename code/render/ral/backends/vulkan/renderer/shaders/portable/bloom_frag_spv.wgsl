@id(5) override extract_mode: i32 = 0i;
override override_type_9_: bool = (extract_mode == 1i);
@id(3) override threshold: f32 = 0.32f;
override override_type_9_1: bool = (extract_mode == 2i);

@group(0) @binding(0)
var texture0_: texture_2d<f32>;
@group(0) @binding(32)
var texture0_sampler: sampler;
var<private> frag_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn main_1() {
    var base: vec3<f32>;
    var m: f32;
    var over: f32;
    var excess: vec3<f32>;
    var m_1: f32;
    var over_1: f32;

    let _e26 = frag_tex_coord_1;
    let _e27 = textureSample(texture0_, texture0_sampler, _e26);
    base = _e27.xyz;
    if override_type_9_ {
        let _e29 = base;
        m = dot(_e29, vec3<f32>(0.33333334f, 0.33333334f, 0.33333334f));
        let _e31 = m;
        over = max((_e31 - threshold), 0f);
        let _e34 = base;
        let _e35 = over;
        let _e36 = m;
        excess = (_e34 * (_e35 / max(_e36, 0.00001f)));
    } else {
        if override_type_9_1 {
            let _e40 = base;
            m_1 = dot(vec3<f32>(0.2126f, 0.7152f, 0.0722f), _e40);
            let _e42 = m_1;
            over_1 = max((_e42 - threshold), 0f);
            let _e45 = base;
            let _e46 = over_1;
            let _e47 = m_1;
            excess = (_e45 * (_e46 / max(_e47, 0.00001f)));
        } else {
            let _e51 = base;
            excess = max((_e51 - vec3(threshold)), vec3<f32>(0f, 0f, 0f));
        }
    }
    let _e55 = excess;
    out_color = vec4<f32>(_e55.x, _e55.y, _e55.z, 1f);
    return;
}

@fragment
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
