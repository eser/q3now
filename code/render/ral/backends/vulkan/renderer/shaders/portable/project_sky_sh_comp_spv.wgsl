struct Push {
    coefficient: u32,
}

struct SkySH {
    coefficients: array<vec4<f32>, 9>,
}

@group(0) @binding(0)
var srcTex: texture_2d_array<f32>;
@group(0) @binding(1)
var srcSamp: sampler;
@group(0) @binding(3)
var<uniform> unnamed: Push;
@group(0) @binding(2)
var<storage, read_write> dstSH: SkySH;

fn shBasis_u0028_u1_u003b_vf3_u003b(index: ptr<function, u32>, d: ptr<function, vec3<f32>>) -> f32 {
    let _e34 = (*index);
    if (_e34 == 0u) {
        return 0.2820948f;
    }
    let _e36 = (*index);
    if (_e36 == 1u) {
        let _e39 = (*d)[1u];
        return (0.48860252f * _e39);
    }
    let _e41 = (*index);
    if (_e41 == 2u) {
        let _e44 = (*d)[2u];
        return (0.48860252f * _e44);
    }
    let _e46 = (*index);
    if (_e46 == 3u) {
        let _e49 = (*d)[0u];
        return (0.48860252f * _e49);
    }
    let _e51 = (*index);
    if (_e51 == 4u) {
        let _e54 = (*d)[0u];
        let _e57 = (*d)[1u];
        return ((1.0925485f * _e54) * _e57);
    }
    let _e59 = (*index);
    if (_e59 == 5u) {
        let _e62 = (*d)[1u];
        let _e65 = (*d)[2u];
        return ((1.0925485f * _e62) * _e65);
    }
    let _e67 = (*index);
    if (_e67 == 6u) {
        let _e70 = (*d)[2u];
        let _e73 = (*d)[2u];
        return (0.31539157f * (((3f * _e70) * _e73) - 1f));
    }
    let _e77 = (*index);
    if (_e77 == 7u) {
        let _e80 = (*d)[0u];
        let _e83 = (*d)[2u];
        return ((1.0925485f * _e80) * _e83);
    }
    let _e86 = (*d)[0u];
    let _e88 = (*d)[0u];
    let _e91 = (*d)[1u];
    let _e93 = (*d)[1u];
    return (0.54627424f * ((_e86 * _e88) - (_e91 * _e93)));
}

fn wired_sky_cube_uv_layer_u0028_vf3_u003b(dir: ptr<function, vec3<f32>>) -> vec3<f32> {
    var a: vec3<f32>;
    var face: u32;
    var uv: vec2<f32>;
    var phi_115_: bool;

    let _e36 = (*dir);
    a = abs(_e36);
    let _e39 = a[0u];
    let _e41 = a[1u];
    let _e42 = (_e39 >= _e41);
    phi_115_ = _e42;
    if _e42 {
        let _e44 = a[0u];
        let _e46 = a[2u];
        phi_115_ = (_e44 >= _e46);
    }
    let _e49 = phi_115_;
    if _e49 {
        let _e51 = (*dir)[0u];
        if (_e51 >= 0f) {
            face = 0u;
            let _e54 = (*dir)[2u];
            let _e57 = (*dir)[1u];
            let _e61 = a[0u];
            uv = (vec2<f32>(-(_e54), -(_e57)) / vec2(_e61));
        } else {
            face = 1u;
            let _e65 = (*dir)[2u];
            let _e67 = (*dir)[1u];
            let _e71 = a[0u];
            uv = (vec2<f32>(_e65, -(_e67)) / vec2(_e71));
        }
    } else {
        let _e75 = a[1u];
        let _e77 = a[2u];
        if (_e75 >= _e77) {
            let _e80 = (*dir)[1u];
            if (_e80 >= 0f) {
                face = 2u;
                let _e83 = (*dir)[0u];
                let _e85 = (*dir)[2u];
                let _e88 = a[1u];
                uv = (vec2<f32>(_e83, _e85) / vec2(_e88));
            } else {
                face = 3u;
                let _e92 = (*dir)[0u];
                let _e94 = (*dir)[2u];
                let _e98 = a[1u];
                uv = (vec2<f32>(_e92, -(_e94)) / vec2(_e98));
            }
        } else {
            let _e102 = (*dir)[2u];
            if (_e102 >= 0f) {
                face = 4u;
                let _e105 = (*dir)[0u];
                let _e107 = (*dir)[1u];
                let _e111 = a[2u];
                uv = (vec2<f32>(_e105, -(_e107)) / vec2(_e111));
            } else {
                face = 5u;
                let _e115 = (*dir)[0u];
                let _e118 = (*dir)[1u];
                let _e122 = a[2u];
                uv = (vec2<f32>(-(_e115), -(_e118)) / vec2(_e122));
            }
        }
    }
    let _e125 = uv;
    let _e128 = ((_e125 * 0.5f) + vec2(0.5f));
    let _e129 = face;
    return vec3<f32>(_e128.x, _e128.y, f32(_e129));
}

fn sampleSource_u0028_vf3_u003b(dir_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param: vec3<f32>;

    let _e34 = (*dir_1);
    param = _e34;
    let _e35 = wired_sky_cube_uv_layer_u0028_vf3_u003b((&param));
    let _e41 = textureSampleLevel(srcTex, srcSamp, vec2<f32>(_e35.x, _e35.y), i32(_e35.z), 0f);
    return _e41.xyz;
}

fn wired_sky_cube_direction_u0028_u1_u003b_vf2_u003b(face_1: ptr<function, u32>, uv_1: ptr<function, vec2<f32>>) -> vec3<f32> {
    var u: f32;
    var v: f32;
    var dir_2: vec3<f32>;

    let _e38 = (*uv_1)[0u];
    u = _e38;
    let _e40 = (*uv_1)[1u];
    v = _e40;
    let _e41 = (*face_1);
    if (_e41 == 0u) {
        let _e43 = v;
        let _e45 = u;
        dir_2 = vec3<f32>(1f, -(_e43), -(_e45));
    } else {
        let _e48 = (*face_1);
        if (_e48 == 1u) {
            let _e50 = v;
            let _e52 = u;
            dir_2 = vec3<f32>(-1f, -(_e50), _e52);
        } else {
            let _e54 = (*face_1);
            if (_e54 == 2u) {
                let _e56 = u;
                let _e57 = v;
                dir_2 = vec3<f32>(_e56, 1f, _e57);
            } else {
                let _e59 = (*face_1);
                if (_e59 == 3u) {
                    let _e61 = u;
                    let _e62 = v;
                    dir_2 = vec3<f32>(_e61, -1f, -(_e62));
                } else {
                    let _e65 = (*face_1);
                    if (_e65 == 4u) {
                        let _e67 = u;
                        let _e68 = v;
                        dir_2 = vec3<f32>(_e67, -(_e68), 1f);
                    } else {
                        let _e71 = u;
                        let _e73 = v;
                        dir_2 = vec3<f32>(-(_e71), -(_e73), -1f);
                    }
                }
            }
        }
    }
    let _e76 = dir_2;
    return normalize(_e76);
}

fn main_1() {
    var sum: vec3<f32>;
    var weightSum: f32;
    var face_2: u32;
    var y: u32;
    var x: u32;
    var uv_2: vec2<f32>;
    var dir_3: vec3<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var q: f32;
    var weight: f32;
    var param_3: vec3<f32>;
    var param_4: u32;
    var param_5: vec3<f32>;

    let _e47 = unnamed.coefficient;
    if (_e47 >= 9u) {
        return;
    }
    sum = vec3<f32>(0f, 0f, 0f);
    weightSum = 0f;
    face_2 = 0u;
    loop {
        let _e49 = face_2;
        if (_e49 < 6u) {
            y = 0u;
            loop {
                let _e51 = y;
                if (_e51 < 16u) {
                    x = 0u;
                    loop {
                        let _e53 = x;
                        if (_e53 < 16u) {
                            let _e55 = x;
                            let _e57 = y;
                            uv_2 = ((((vec2<f32>(f32(_e55), f32(_e57)) + vec2(0.5f)) / vec2(16f)) * 2f) - vec2(1f));
                            let _e67 = face_2;
                            param_1 = _e67;
                            let _e68 = uv_2;
                            param_2 = _e68;
                            let _e69 = wired_sky_cube_direction_u0028_u1_u003b_vf2_u003b((&param_1), (&param_2));
                            dir_3 = _e69;
                            let _e70 = uv_2;
                            let _e71 = uv_2;
                            q = (1f + dot(_e70, _e71));
                            let _e74 = q;
                            let _e75 = q;
                            weight = (1f / (_e74 * sqrt(_e75)));
                            let _e79 = dir_3;
                            param_3 = _e79;
                            let _e80 = sampleSource_u0028_vf3_u003b((&param_3));
                            let _e82 = unnamed.coefficient;
                            param_4 = _e82;
                            let _e83 = dir_3;
                            param_5 = _e83;
                            let _e84 = shBasis_u0028_u1_u003b_vf3_u003b((&param_4), (&param_5));
                            let _e86 = weight;
                            let _e88 = sum;
                            sum = (_e88 + ((_e80 * _e84) * _e86));
                            let _e90 = weight;
                            let _e91 = weightSum;
                            weightSum = (_e91 + _e90);
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e93 = x;
                            x = (_e93 + bitcast<u32>(1i));
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e96 = y;
                    y = (_e96 + bitcast<u32>(1i));
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e99 = face_2;
            face_2 = (_e99 + bitcast<u32>(1i));
        }
    }
    let _e103 = unnamed.coefficient;
    let _e104 = sum;
    let _e105 = weightSum;
    let _e108 = (_e104 * (12.566371f / max(_e105, 0.000001f)));
    dstSH.coefficients[_e103] = vec4<f32>(_e108.x, _e108.y, _e108.z, 0f);
    return;
}

@compute @workgroup_size(1, 1, 1)
fn main() {
    main_1();
}
