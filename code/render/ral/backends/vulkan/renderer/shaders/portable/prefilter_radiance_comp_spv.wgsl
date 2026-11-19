struct Push {
    roughnessIdx: u32,
    roughnessMips: u32,
}

@group(0) @binding(0)
var srcTex: texture_2d_array<f32>;
@group(0) @binding(1)
var srcSamp: sampler;
@group(0) @binding(2)
var dstRadiance: texture_storage_2d_array<rgba16float,write>;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(3)
var<uniform> unnamed: Push;

fn importanceSampleGGX_u0028_vf2_u003b_f1_u003b_vf3_u003b(xi: ptr<function, vec2<f32>>, roughness: ptr<function, f32>, N: ptr<function, vec3<f32>>) -> vec3<f32> {
    var a: f32;
    var phi: f32;
    var cosTheta: f32;
    var sinTheta: f32;
    var H: vec3<f32>;
    var up: vec3<f32>;
    var tangent: vec3<f32>;
    var bitangent: vec3<f32>;

    let _e49 = (*roughness);
    let _e50 = (*roughness);
    a = (_e49 * _e50);
    let _e53 = (*xi)[0u];
    phi = (6.2831855f * _e53);
    let _e56 = (*xi)[1u];
    let _e58 = a;
    let _e59 = a;
    let _e63 = (*xi)[1u];
    cosTheta = sqrt(((1f - _e56) / (1f + (((_e58 * _e59) - 1f) * _e63))));
    let _e68 = cosTheta;
    let _e69 = cosTheta;
    sinTheta = sqrt((1f - (_e68 * _e69)));
    let _e73 = phi;
    let _e75 = sinTheta;
    H[0u] = (cos(_e73) * _e75);
    let _e78 = phi;
    let _e80 = sinTheta;
    H[1u] = (sin(_e78) * _e80);
    let _e83 = cosTheta;
    H[2u] = _e83;
    let _e86 = (*N)[2u];
    up = select(vec3<f32>(1f, 0f, 0f), vec3<f32>(0f, 0f, 1f), vec3((abs(_e86) < 0.999f)));
    let _e91 = up;
    let _e92 = (*N);
    tangent = normalize(cross(_e91, _e92));
    let _e95 = (*N);
    let _e96 = tangent;
    bitangent = cross(_e95, _e96);
    let _e98 = tangent;
    let _e100 = H[0u];
    let _e102 = bitangent;
    let _e104 = H[1u];
    let _e107 = (*N);
    let _e109 = H[2u];
    return normalize((((_e98 * _e100) + (_e102 * _e104)) + (_e107 * _e109)));
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
    var param: u32;

    let _e41 = (*i);
    let _e43 = (*n);
    let _e46 = (*i);
    param = _e46;
    let _e47 = radicalInverse_VdC_u0028_u1_u003b((&param));
    return vec2<f32>((f32(_e41) / f32(_e43)), _e47);
}

fn wired_sky_cube_uv_layer_u0028_vf3_u003b(dir: ptr<function, vec3<f32>>) -> vec3<f32> {
    var a_1: vec3<f32>;
    var face: u32;
    var uv: vec2<f32>;
    var phi_125_: bool;

    let _e42 = (*dir);
    a_1 = abs(_e42);
    let _e45 = a_1[0u];
    let _e47 = a_1[1u];
    let _e48 = (_e45 >= _e47);
    phi_125_ = _e48;
    if _e48 {
        let _e50 = a_1[0u];
        let _e52 = a_1[2u];
        phi_125_ = (_e50 >= _e52);
    }
    let _e55 = phi_125_;
    if _e55 {
        let _e57 = (*dir)[0u];
        if (_e57 >= 0f) {
            face = 0u;
            let _e60 = (*dir)[2u];
            let _e63 = (*dir)[1u];
            let _e67 = a_1[0u];
            uv = (vec2<f32>(-(_e60), -(_e63)) / vec2(_e67));
        } else {
            face = 1u;
            let _e71 = (*dir)[2u];
            let _e73 = (*dir)[1u];
            let _e77 = a_1[0u];
            uv = (vec2<f32>(_e71, -(_e73)) / vec2(_e77));
        }
    } else {
        let _e81 = a_1[1u];
        let _e83 = a_1[2u];
        if (_e81 >= _e83) {
            let _e86 = (*dir)[1u];
            if (_e86 >= 0f) {
                face = 2u;
                let _e89 = (*dir)[0u];
                let _e91 = (*dir)[2u];
                let _e94 = a_1[1u];
                uv = (vec2<f32>(_e89, _e91) / vec2(_e94));
            } else {
                face = 3u;
                let _e98 = (*dir)[0u];
                let _e100 = (*dir)[2u];
                let _e104 = a_1[1u];
                uv = (vec2<f32>(_e98, -(_e100)) / vec2(_e104));
            }
        } else {
            let _e108 = (*dir)[2u];
            if (_e108 >= 0f) {
                face = 4u;
                let _e111 = (*dir)[0u];
                let _e113 = (*dir)[1u];
                let _e117 = a_1[2u];
                uv = (vec2<f32>(_e111, -(_e113)) / vec2(_e117));
            } else {
                face = 5u;
                let _e121 = (*dir)[0u];
                let _e124 = (*dir)[1u];
                let _e128 = a_1[2u];
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
    var param_1: vec3<f32>;

    let _e40 = (*dir_1);
    param_1 = _e40;
    let _e41 = wired_sky_cube_uv_layer_u0028_vf3_u003b((&param_1));
    let _e47 = textureSampleLevel(srcTex, srcSamp, vec2<f32>(_e41.x, _e41.y), i32(_e41.z), 0f);
    return _e47.xyz;
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
    var roughness_1: f32;
    var local: f32;
    var uv_2: vec2<f32>;
    var R: vec3<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var N_1: vec3<f32>;
    var V: vec3<f32>;
    var c: vec3<f32>;
    var param_4: vec3<f32>;
    var prefiltered: vec3<f32>;
    var totalWeight: f32;
    var i_1: u32;
    var xi_1: vec2<f32>;
    var param_5: u32;
    var param_6: u32;
    var H_1: vec3<f32>;
    var param_7: vec2<f32>;
    var param_8: f32;
    var param_9: vec3<f32>;
    var L: vec3<f32>;
    var NdotL: f32;
    var param_10: vec3<f32>;
    var result: vec3<f32>;
    var local_1: vec3<f32>;
    var param_11: vec3<f32>;
    var phi_425_: bool;
    var phi_434_: bool;

    let _e66 = textureDimensions(dstRadiance);
    size = bitcast<vec2<u32>>(vec2<i32>(_e66).xy);
    let _e70 = gl_GlobalInvocationID_1;
    coord = bitcast<vec3<i32>>(_e70);
    let _e73 = coord[0u];
    let _e76 = size[0u];
    let _e77 = (bitcast<u32>(_e73) >= _e76);
    phi_425_ = _e77;
    if !(_e77) {
        let _e80 = coord[1u];
        let _e83 = size[1u];
        phi_425_ = (bitcast<u32>(_e80) >= _e83);
    }
    let _e86 = phi_425_;
    phi_434_ = _e86;
    if !(_e86) {
        let _e89 = coord[2u];
        phi_434_ = (bitcast<u32>(_e89) >= 6u);
    }
    let _e93 = phi_434_;
    if _e93 {
        return;
    }
    let _e95 = unnamed.roughnessMips;
    if (_e95 > 1u) {
        let _e98 = unnamed.roughnessIdx;
        let _e101 = unnamed.roughnessMips;
        local = (f32(_e98) / f32((_e101 - 1u)));
    } else {
        local = 0f;
    }
    let _e105 = local;
    roughness_1 = _e105;
    let _e106 = coord;
    let _e111 = size;
    uv_2 = ((((vec2<f32>(_e106.xy) + vec2(0.5f)) / vec2<f32>(_e111)) * 2f) - vec2(1f));
    let _e118 = coord[2u];
    param_2 = bitcast<u32>(_e118);
    let _e120 = uv_2;
    param_3 = _e120;
    let _e121 = wired_sky_cube_direction_u0028_u1_u003b_vf2_u003b((&param_2), (&param_3));
    R = _e121;
    let _e122 = R;
    N_1 = _e122;
    let _e123 = R;
    V = _e123;
    let _e124 = roughness_1;
    if (_e124 <= 0f) {
        let _e126 = R;
        param_4 = _e126;
        let _e127 = sampleSource_u0028_vf3_u003b((&param_4));
        c = _e127;
        let _e128 = coord;
        let _e129 = c;
        textureStore(dstRadiance, vec2<i32>(_e128.x, _e128.y), i32(_e128.z), vec4<f32>(_e129.x, _e129.y, _e129.z, 1f));
        return;
    }
    prefiltered = vec3<f32>(0f, 0f, 0f);
    totalWeight = 0f;
    i_1 = 0u;
    loop {
        let _e139 = i_1;
        if (_e139 < 1024u) {
            let _e141 = i_1;
            param_5 = _e141;
            param_6 = 1024u;
            let _e142 = hammersley_u0028_u1_u003b_u1_u003b((&param_5), (&param_6));
            xi_1 = _e142;
            let _e143 = xi_1;
            param_7 = _e143;
            let _e144 = roughness_1;
            param_8 = _e144;
            let _e145 = N_1;
            param_9 = _e145;
            let _e146 = importanceSampleGGX_u0028_vf2_u003b_f1_u003b_vf3_u003b((&param_7), (&param_8), (&param_9));
            H_1 = _e146;
            let _e147 = V;
            let _e148 = H_1;
            let _e151 = H_1;
            let _e153 = V;
            L = normalize(((_e151 * (2f * dot(_e147, _e148))) - _e153));
            let _e156 = N_1;
            let _e157 = L;
            NdotL = max(dot(_e156, _e157), 0f);
            let _e160 = NdotL;
            if (_e160 > 0f) {
                let _e162 = L;
                param_10 = _e162;
                let _e163 = sampleSource_u0028_vf3_u003b((&param_10));
                let _e164 = NdotL;
                let _e166 = prefiltered;
                prefiltered = (_e166 + (_e163 * _e164));
                let _e168 = NdotL;
                let _e169 = totalWeight;
                totalWeight = (_e169 + _e168);
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e171 = i_1;
            i_1 = (_e171 + bitcast<u32>(1i));
        }
    }
    let _e174 = totalWeight;
    if (_e174 > 0f) {
        let _e176 = prefiltered;
        let _e177 = totalWeight;
        local_1 = (_e176 / vec3(_e177));
    } else {
        let _e180 = R;
        param_11 = _e180;
        let _e181 = sampleSource_u0028_vf3_u003b((&param_11));
        local_1 = _e181;
    }
    let _e182 = local_1;
    result = _e182;
    let _e183 = coord;
    let _e184 = result;
    textureStore(dstRadiance, vec2<i32>(_e183.x, _e183.y), i32(_e183.z), vec4<f32>(_e184.x, _e184.y, _e184.z, 1f));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
