@group(0) @binding(0) 
var skyImage: texture_storage_2d_array<rgba16float,write>;
var<private> gl_GlobalInvocationID_1: vec3<u32>;

fn analyticSky_u0028_vf3_u003b(dir: ptr<function, vec3<f32>>) -> vec3<f32> {
    var up: f32;
    var t: f32;
    var sky: vec3<f32>;
    var t_1: f32;
    var sd: f32;
    var lobe: f32;

    let _e49 = (*dir)[1u];
    up = _e49;
    let _e50 = up;
    if (_e50 >= 0f) {
        let _e52 = up;
        t = smoothstep(0f, 1f, _e52);
        let _e54 = t;
        sky = mix(vec3<f32>(0.13f, 0.15f, 0.18f), vec3<f32>(0.045f, 0.09f, 0.19f), vec3(_e54));
    } else {
        let _e57 = up;
        t_1 = smoothstep(0f, -0.35f, _e57);
        let _e59 = t_1;
        sky = mix(vec3<f32>(0.13f, 0.15f, 0.18f), vec3<f32>(0.045f, 0.04f, 0.034f), vec3(_e59));
    }
    let _e62 = (*dir);
    sd = max(dot(_e62, vec3<f32>(0.39990005f, 0.8197951f, 0.40989754f)), 0f);
    let _e65 = sd;
    lobe = pow(_e65, 320f);
    let _e67 = up;
    if (_e67 > 0f) {
        let _e69 = lobe;
        let _e71 = sky;
        sky = (_e71 + (vec3<f32>(1.3f, 1.05f, 0.7f) * _e69));
    }
    let _e73 = sky;
    return _e73;
}

fn cubeFaceDir_u0028_u1_u003b_vf2_u003b(face: ptr<function, u32>, uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var u: f32;
    var v: f32;
    var dir_1: vec3<f32>;

    let _e47 = (*uv)[0u];
    u = _e47;
    let _e49 = (*uv)[1u];
    v = _e49;
    let _e50 = (*face);
    if (_e50 == 0u) {
        let _e52 = v;
        let _e54 = u;
        dir_1 = vec3<f32>(1f, -(_e52), -(_e54));
    } else {
        let _e57 = (*face);
        if (_e57 == 1u) {
            let _e59 = v;
            let _e61 = u;
            dir_1 = vec3<f32>(-1f, -(_e59), _e61);
        } else {
            let _e63 = (*face);
            if (_e63 == 2u) {
                let _e65 = u;
                let _e66 = v;
                dir_1 = vec3<f32>(_e65, 1f, _e66);
            } else {
                let _e68 = (*face);
                if (_e68 == 3u) {
                    let _e70 = u;
                    let _e71 = v;
                    dir_1 = vec3<f32>(_e70, -1f, -(_e71));
                } else {
                    let _e74 = (*face);
                    if (_e74 == 4u) {
                        let _e76 = u;
                        let _e77 = v;
                        dir_1 = vec3<f32>(_e76, -(_e77), 1f);
                    } else {
                        let _e80 = u;
                        let _e82 = v;
                        dir_1 = vec3<f32>(-(_e80), -(_e82), -1f);
                    }
                }
            }
        }
    }
    let _e85 = dir_1;
    return normalize(_e85);
}

fn main_1() {
    var size: vec2<u32>;
    var coord: vec3<i32>;
    var uv_1: vec2<f32>;
    var dir_2: vec3<f32>;
    var param: u32;
    var param_1: vec2<f32>;
    var radiance: vec3<f32>;
    var param_2: vec3<f32>;
    var phi_189_: bool;
    var phi_198_: bool;

    let _e49 = textureDimensions(skyImage);
    size = bitcast<vec2<u32>>(vec2<i32>(_e49).xy);
    let _e53 = gl_GlobalInvocationID_1;
    coord = bitcast<vec3<i32>>(_e53);
    let _e56 = coord[0u];
    let _e59 = size[0u];
    let _e60 = (bitcast<u32>(_e56) >= _e59);
    phi_189_ = _e60;
    if !(_e60) {
        let _e63 = coord[1u];
        let _e66 = size[1u];
        phi_189_ = (bitcast<u32>(_e63) >= _e66);
    }
    let _e69 = phi_189_;
    phi_198_ = _e69;
    if !(_e69) {
        let _e72 = coord[2u];
        phi_198_ = (bitcast<u32>(_e72) >= 6u);
    }
    let _e76 = phi_198_;
    if _e76 {
        return;
    }
    let _e77 = coord;
    let _e82 = size;
    uv_1 = ((((vec2<f32>(_e77.xy) + vec2(0.5f)) / vec2<f32>(_e82)) * 2f) - vec2(1f));
    let _e89 = coord[2u];
    param = bitcast<u32>(_e89);
    let _e91 = uv_1;
    param_1 = _e91;
    let _e92 = cubeFaceDir_u0028_u1_u003b_vf2_u003b((&param), (&param_1));
    dir_2 = _e92;
    let _e93 = dir_2;
    param_2 = _e93;
    let _e94 = analyticSky_u0028_vf3_u003b((&param_2));
    radiance = _e94;
    let _e95 = coord;
    let _e96 = radiance;
    textureStore(skyImage, vec2<i32>(_e95.x, _e95.y), i32(_e95.z), vec4<f32>(_e96.x, _e96.y, _e96.z, 1f));
    return;
}

@compute @workgroup_size(8, 8, 1) 
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
