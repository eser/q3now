struct Push {
    pad: u32,
}

@group(0) @binding(0)
var srcTex: texture_2d_array<f32>;
@group(0) @binding(1)
var srcSamp: sampler;
@group(0) @binding(2)
var dstIrradiance: texture_storage_2d_array<rgba16float,write>;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(3)
var<uniform> unnamed: Push;

fn wired_sky_cube_uv_layer_u0028_vf3_u003b(dir: ptr<function, vec3<f32>>) -> vec3<f32> {
    var a: vec3<f32>;
    var face: u32;
    var uv: vec2<f32>;
    var phi_119_: bool;

    let _e42 = (*dir);
    a = abs(_e42);
    let _e45 = a[0u];
    let _e47 = a[1u];
    let _e48 = (_e45 >= _e47);
    phi_119_ = _e48;
    if _e48 {
        let _e50 = a[0u];
        let _e52 = a[2u];
        phi_119_ = (_e50 >= _e52);
    }
    let _e55 = phi_119_;
    if _e55 {
        let _e57 = (*dir)[0u];
        if (_e57 >= 0f) {
            face = 0u;
            let _e60 = (*dir)[2u];
            let _e63 = (*dir)[1u];
            let _e67 = a[0u];
            uv = (vec2<f32>(-(_e60), -(_e63)) / vec2(_e67));
        } else {
            face = 1u;
            let _e71 = (*dir)[2u];
            let _e73 = (*dir)[1u];
            let _e77 = a[0u];
            uv = (vec2<f32>(_e71, -(_e73)) / vec2(_e77));
        }
    } else {
        let _e81 = a[1u];
        let _e83 = a[2u];
        if (_e81 >= _e83) {
            let _e86 = (*dir)[1u];
            if (_e86 >= 0f) {
                face = 2u;
                let _e89 = (*dir)[0u];
                let _e91 = (*dir)[2u];
                let _e94 = a[1u];
                uv = (vec2<f32>(_e89, _e91) / vec2(_e94));
            } else {
                face = 3u;
                let _e98 = (*dir)[0u];
                let _e100 = (*dir)[2u];
                let _e104 = a[1u];
                uv = (vec2<f32>(_e98, -(_e100)) / vec2(_e104));
            }
        } else {
            let _e108 = (*dir)[2u];
            if (_e108 >= 0f) {
                face = 4u;
                let _e111 = (*dir)[0u];
                let _e113 = (*dir)[1u];
                let _e117 = a[2u];
                uv = (vec2<f32>(_e111, -(_e113)) / vec2(_e117));
            } else {
                face = 5u;
                let _e121 = (*dir)[0u];
                let _e124 = (*dir)[1u];
                let _e128 = a[2u];
                uv = (vec2<f32>(-(_e121), -(_e124)) / vec2(_e128));
            }
        }
    }
    let _e131 = uv;
    let _e134 = ((_e131 * 0.5f) + vec2(0.5f));
    let _e135 = face;
    return vec3<f32>(_e134.x, _e134.y, f32(_e135));
}

fn sampleSource_u0028_vf3_u003b(dir_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param: vec3<f32>;

    let _e40 = (*dir_1);
    param = _e40;
    let _e41 = wired_sky_cube_uv_layer_u0028_vf3_u003b((&param));
    let _e47 = textureSampleLevel(srcTex, srcSamp, vec2<f32>(_e41.x, _e41.y), i32(_e41.z), 0f);
    return _e47.xyz;
}

fn radicalInverse_VdC_u0028_u1_u003b(bits: ptr<function, u32>) -> f32 {
    let _e39 = (*bits);
    let _e42 = (*bits);
    (*bits) = ((_e39 << bitcast<u32>(16u)) | (_e42 >> bitcast<u32>(16u)));
    let _e46 = (*bits);
    let _e50 = (*bits);
    (*bits) = (((_e46 & 1431655765u) << bitcast<u32>(1u)) | ((_e50 & 2863311530u) >> bitcast<u32>(1u)));
    let _e55 = (*bits);
    let _e59 = (*bits);
    (*bits) = (((_e55 & 858993459u) << bitcast<u32>(2u)) | ((_e59 & 3435973836u) >> bitcast<u32>(2u)));
    let _e64 = (*bits);
    let _e68 = (*bits);
    (*bits) = (((_e64 & 252645135u) << bitcast<u32>(4u)) | ((_e68 & 4042322160u) >> bitcast<u32>(4u)));
    let _e73 = (*bits);
    let _e77 = (*bits);
    (*bits) = (((_e73 & 16711935u) << bitcast<u32>(8u)) | ((_e77 & 4278255360u) >> bitcast<u32>(8u)));
    let _e82 = (*bits);
    return (f32(_e82) * 0.00000000023283064f);
}

fn hammersley_u0028_u1_u003b_u1_u003b(i: ptr<function, u32>, n: ptr<function, u32>) -> vec2<f32> {
    var param_1: u32;

    let _e41 = (*i);
    let _e43 = (*n);
    let _e46 = (*i);
    param_1 = _e46;
    let _e47 = radicalInverse_VdC_u0028_u1_u003b((&param_1));
    return vec2<f32>((f32(_e41) / f32(_e43)), _e47);
}

fn wired_sky_cube_direction_u0028_u1_u003b_vf2_u003b(face_1: ptr<function, u32>, uv_1: ptr<function, vec2<f32>>) -> vec3<f32> {
    var u: f32;
    var v: f32;
    var dir_2: vec3<f32>;

    let _e44 = (*uv_1)[0u];
    u = _e44;
    let _e46 = (*uv_1)[1u];
    v = _e46;
    let _e47 = (*face_1);
    if (_e47 == 0u) {
        let _e49 = v;
        let _e51 = u;
        dir_2 = vec3<f32>(1f, -(_e49), -(_e51));
    } else {
        let _e54 = (*face_1);
        if (_e54 == 1u) {
            let _e56 = v;
            let _e58 = u;
            dir_2 = vec3<f32>(-1f, -(_e56), _e58);
        } else {
            let _e60 = (*face_1);
            if (_e60 == 2u) {
                let _e62 = u;
                let _e63 = v;
                dir_2 = vec3<f32>(_e62, 1f, _e63);
            } else {
                let _e65 = (*face_1);
                if (_e65 == 3u) {
                    let _e67 = u;
                    let _e68 = v;
                    dir_2 = vec3<f32>(_e67, -1f, -(_e68));
                } else {
                    let _e71 = (*face_1);
                    if (_e71 == 4u) {
                        let _e73 = u;
                        let _e74 = v;
                        dir_2 = vec3<f32>(_e73, -(_e74), 1f);
                    } else {
                        let _e77 = u;
                        let _e79 = v;
                        dir_2 = vec3<f32>(-(_e77), -(_e79), -1f);
                    }
                }
            }
        }
    }
    let _e82 = dir_2;
    return normalize(_e82);
}

fn main_1() {
    var size: vec2<u32>;
    var coord: vec3<i32>;
    var uv_2: vec2<f32>;
    var N: vec3<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var up: vec3<f32>;
    var tangent: vec3<f32>;
    var bitangent: vec3<f32>;
    var irradiance: vec3<f32>;
    var i_1: u32;
    var xi: vec2<f32>;
    var param_4: u32;
    var param_5: u32;
    var phi: f32;
    var cosTheta: f32;
    var sinTheta: f32;
    var s: vec3<f32>;
    var sampleDir: vec3<f32>;
    var param_6: vec3<f32>;
    var phi_340_: bool;
    var phi_349_: bool;

    let _e58 = textureDimensions(dstIrradiance);
    size = bitcast<vec2<u32>>(vec2<i32>(_e58).xy);
    let _e62 = gl_GlobalInvocationID_1;
    coord = bitcast<vec3<i32>>(_e62);
    let _e65 = coord[0u];
    let _e68 = size[0u];
    let _e69 = (bitcast<u32>(_e65) >= _e68);
    phi_340_ = _e69;
    if !(_e69) {
        let _e72 = coord[1u];
        let _e75 = size[1u];
        phi_340_ = (bitcast<u32>(_e72) >= _e75);
    }
    let _e78 = phi_340_;
    phi_349_ = _e78;
    if !(_e78) {
        let _e81 = coord[2u];
        phi_349_ = (bitcast<u32>(_e81) >= 6u);
    }
    let _e85 = phi_349_;
    if _e85 {
        return;
    }
    let _e86 = coord;
    let _e91 = size;
    uv_2 = ((((vec2<f32>(_e86.xy) + vec2(0.5f)) / vec2<f32>(_e91)) * 2f) - vec2(1f));
    let _e98 = coord[2u];
    param_2 = bitcast<u32>(_e98);
    let _e100 = uv_2;
    param_3 = _e100;
    let _e101 = wired_sky_cube_direction_u0028_u1_u003b_vf2_u003b((&param_2), (&param_3));
    N = _e101;
    let _e103 = N[2u];
    up = select(vec3<f32>(1f, 0f, 0f), vec3<f32>(0f, 0f, 1f), vec3((abs(_e103) < 0.999f)));
    let _e108 = up;
    let _e109 = N;
    tangent = normalize(cross(_e108, _e109));
    let _e112 = N;
    let _e113 = tangent;
    bitangent = cross(_e112, _e113);
    irradiance = vec3<f32>(0f, 0f, 0f);
    i_1 = 0u;
    loop {
        let _e115 = i_1;
        if (_e115 < 256u) {
            let _e117 = i_1;
            param_4 = _e117;
            param_5 = 256u;
            let _e118 = hammersley_u0028_u1_u003b_u1_u003b((&param_4), (&param_5));
            xi = _e118;
            let _e120 = xi[0u];
            phi = (6.2831855f * _e120);
            let _e123 = xi[1u];
            cosTheta = sqrt((1f - _e123));
            let _e127 = xi[1u];
            sinTheta = sqrt(_e127);
            let _e129 = phi;
            let _e131 = sinTheta;
            let _e133 = phi;
            let _e135 = sinTheta;
            let _e137 = cosTheta;
            s = vec3<f32>((cos(_e129) * _e131), (sin(_e133) * _e135), _e137);
            let _e140 = s[0u];
            let _e141 = tangent;
            let _e144 = s[1u];
            let _e145 = bitangent;
            let _e149 = s[2u];
            let _e150 = N;
            sampleDir = (((_e141 * _e140) + (_e145 * _e144)) + (_e150 * _e149));
            let _e153 = sampleDir;
            param_6 = _e153;
            let _e154 = sampleSource_u0028_vf3_u003b((&param_6));
            let _e155 = irradiance;
            irradiance = (_e155 + _e154);
            continue;
        } else {
            break;
        }
        continuing {
            let _e157 = i_1;
            i_1 = (_e157 + bitcast<u32>(1i));
        }
    }
    let _e160 = irradiance;
    irradiance = ((_e160 * 3.1415927f) / vec3(256f));
    let _e164 = coord;
    let _e165 = irradiance;
    textureStore(dstIrradiance, vec2<i32>(_e164.x, _e164.y), i32(_e164.z), vec4<f32>(_e165.x, _e165.y, _e165.z, 1f));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
