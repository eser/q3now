@id(4) override color_mode: i32 = 0i;
override override_type_3_: bool = (color_mode == 1i);
override override_type_3_1: bool = (color_mode == 2i);
override override_type_3_2: bool = (color_mode == 3i);

var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e28 = (*c);
    (*c) = max(_e28, vec3<f32>(0f, 0f, 0f));
    let _e30 = (*c);
    cutoff = (_e30 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e32 = (*c);
    lo = (_e32 / vec3(12.92f));
    let _e35 = (*c);
    hi = pow(((_e35 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e40 = hi;
    let _e41 = lo;
    let _e42 = cutoff;
    return mix(_e40, _e41, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e42));
}

fn main_1() {
    var param: vec3<f32>;
    var param_1: vec3<f32>;
    var param_2: vec3<f32>;
    var param_3: vec3<f32>;

    if override_type_3_ {
        param = vec3<f32>(1f, 1f, 1f);
        let _e28 = sRGBToLinear_u0028_vf3_u003b((&param));
        out_color = vec4<f32>(_e28.x, _e28.y, _e28.z, 1f);
    } else {
        if override_type_3_1 {
            param_1 = vec3<f32>(0.2f, 1f, 0.2f);
            let _e33 = sRGBToLinear_u0028_vf3_u003b((&param_1));
            out_color = vec4<f32>(_e33.x, _e33.y, _e33.z, 1f);
        } else {
            if override_type_3_2 {
                param_2 = vec3<f32>(1f, 0.33f, 0.2f);
                let _e38 = sRGBToLinear_u0028_vf3_u003b((&param_2));
                out_color = vec4<f32>(_e38.x, _e38.y, _e38.z, 1f);
            } else {
                param_3 = vec3<f32>(0f, 0f, 0f);
                let _e43 = sRGBToLinear_u0028_vf3_u003b((&param_3));
                out_color = vec4<f32>(_e43.x, _e43.y, _e43.z, 1f);
            }
        }
    }
    return;
}

@fragment
fn main() -> @location(0) vec4<f32> {
    main_1();
    let _e1 = out_color;
    return _e1;
}
