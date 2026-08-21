struct Push {
    roughnessIdx: u32,
    roughnessMips: u32,
}

@group(0) @binding(2) 
var dstRadiance: texture_storage_2d_array<rgba16float,write>;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(3)
var<uniform> unnamed: Push;
@group(0) @binding(0) 
var srcTex: texture_cube<f32>;
@group(0) @binding(1) 
var srcSamp: sampler;

fn importanceSampleGGX_u0028_vf2_u003b_f1_u003b_vf3_u003b(xi: ptr<function, vec2<f32>>, roughness: ptr<function, f32>, N: ptr<function, vec3<f32>>) -> vec3<f32> {
    var a: f32;
    var phi: f32;
    var cosTheta: f32;
    var sinTheta: f32;
    var H: vec3<f32>;
    var up: vec3<f32>;
    var tangent: vec3<f32>;
    var bitangent: vec3<f32>;

    let _e48 = (*roughness);
    let _e49 = (*roughness);
    a = (_e48 * _e49);
    let _e52 = (*xi)[0u];
    phi = (6.2831855f * _e52);
    let _e55 = (*xi)[1u];
    let _e57 = a;
    let _e58 = a;
    let _e62 = (*xi)[1u];
    cosTheta = sqrt(((1f - _e55) / (1f + (((_e57 * _e58) - 1f) * _e62))));
    let _e67 = cosTheta;
    let _e68 = cosTheta;
    sinTheta = sqrt((1f - (_e67 * _e68)));
    let _e72 = phi;
    let _e74 = sinTheta;
    H[0u] = (cos(_e72) * _e74);
    let _e77 = phi;
    let _e79 = sinTheta;
    H[1u] = (sin(_e77) * _e79);
    let _e82 = cosTheta;
    H[2u] = _e82;
    let _e85 = (*N)[2u];
    up = select(vec3<f32>(1f, 0f, 0f), vec3<f32>(0f, 0f, 1f), vec3((abs(_e85) < 0.999f)));
    let _e90 = up;
    let _e91 = (*N);
    tangent = normalize(cross(_e90, _e91));
    let _e94 = (*N);
    let _e95 = tangent;
    bitangent = cross(_e94, _e95);
    let _e97 = tangent;
    let _e99 = H[0u];
    let _e101 = bitangent;
    let _e103 = H[1u];
    let _e106 = (*N);
    let _e108 = H[2u];
    return normalize((((_e97 * _e99) + (_e101 * _e103)) + (_e106 * _e108)));
}

fn radicalInverse_VdC_u0028_u1_u003b(bits: ptr<function, u32>) -> f32 {
    let _e38 = (*bits);
    let _e41 = (*bits);
    (*bits) = ((_e38 << bitcast<u32>(16u)) | (_e41 >> bitcast<u32>(16u)));
    let _e45 = (*bits);
    let _e49 = (*bits);
    (*bits) = (((_e45 & 1431655765u) << bitcast<u32>(1u)) | ((_e49 & 2863311530u) >> bitcast<u32>(1u)));
    let _e54 = (*bits);
    let _e58 = (*bits);
    (*bits) = (((_e54 & 858993459u) << bitcast<u32>(2u)) | ((_e58 & 3435973836u) >> bitcast<u32>(2u)));
    let _e63 = (*bits);
    let _e67 = (*bits);
    (*bits) = (((_e63 & 252645135u) << bitcast<u32>(4u)) | ((_e67 & 4042322160u) >> bitcast<u32>(4u)));
    let _e72 = (*bits);
    let _e76 = (*bits);
    (*bits) = (((_e72 & 16711935u) << bitcast<u32>(8u)) | ((_e76 & 4278255360u) >> bitcast<u32>(8u)));
    let _e81 = (*bits);
    return (f32(_e81) * 0.00000000023283064f);
}

fn hammersley_u0028_u1_u003b_u1_u003b(i: ptr<function, u32>, n: ptr<function, u32>) -> vec2<f32> {
    var param: u32;

    let _e40 = (*i);
    let _e42 = (*n);
    let _e45 = (*i);
    param = _e45;
    let _e46 = radicalInverse_VdC_u0028_u1_u003b((&param));
    return vec2<f32>((f32(_e40) / f32(_e42)), _e46);
}

fn cubeFaceDir_u0028_u1_u003b_vf2_u003b(face: ptr<function, u32>, uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var u: f32;
    var v: f32;
    var dir: vec3<f32>;

    let _e43 = (*uv)[0u];
    u = _e43;
    let _e45 = (*uv)[1u];
    v = _e45;
    let _e46 = (*face);
    if (_e46 == 0u) {
        let _e48 = v;
        let _e50 = u;
        dir = vec3<f32>(1f, -(_e48), -(_e50));
    } else {
        let _e53 = (*face);
        if (_e53 == 1u) {
            let _e55 = v;
            let _e57 = u;
            dir = vec3<f32>(-1f, -(_e55), _e57);
        } else {
            let _e59 = (*face);
            if (_e59 == 2u) {
                let _e61 = u;
                let _e62 = v;
                dir = vec3<f32>(_e61, 1f, _e62);
            } else {
                let _e64 = (*face);
                if (_e64 == 3u) {
                    let _e66 = u;
                    let _e67 = v;
                    dir = vec3<f32>(_e66, -1f, -(_e67));
                } else {
                    let _e70 = (*face);
                    if (_e70 == 4u) {
                        let _e72 = u;
                        let _e73 = v;
                        dir = vec3<f32>(_e72, -(_e73), 1f);
                    } else {
                        let _e76 = u;
                        let _e78 = v;
                        dir = vec3<f32>(-(_e76), -(_e78), -1f);
                    }
                }
            }
        }
    }
    let _e81 = dir;
    return normalize(_e81);
}

fn main_1() {
    var size: vec2<u32>;
    var coord: vec3<i32>;
    var roughness_1: f32;
    var local: f32;
    var uv_1: vec2<f32>;
    var R: vec3<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var N_1: vec3<f32>;
    var V: vec3<f32>;
    var c: vec3<f32>;
    var prefiltered: vec3<f32>;
    var totalWeight: f32;
    var i_1: u32;
    var xi_1: vec2<f32>;
    var param_3: u32;
    var param_4: u32;
    var H_1: vec3<f32>;
    var param_5: vec2<f32>;
    var param_6: f32;
    var param_7: vec3<f32>;
    var L: vec3<f32>;
    var NdotL: f32;
    var result: vec3<f32>;
    var local_1: vec3<f32>;
    var phi_279_: bool;
    var phi_288_: bool;

    let _e62 = textureDimensions(dstRadiance);
    size = bitcast<vec2<u32>>(vec2<i32>(_e62).xy);
    let _e66 = gl_GlobalInvocationID_1;
    coord = bitcast<vec3<i32>>(_e66);
    let _e69 = coord[0u];
    let _e72 = size[0u];
    let _e73 = (bitcast<u32>(_e69) >= _e72);
    phi_279_ = _e73;
    if !(_e73) {
        let _e76 = coord[1u];
        let _e79 = size[1u];
        phi_279_ = (bitcast<u32>(_e76) >= _e79);
    }
    let _e82 = phi_279_;
    phi_288_ = _e82;
    if !(_e82) {
        let _e85 = coord[2u];
        phi_288_ = (bitcast<u32>(_e85) >= 6u);
    }
    let _e89 = phi_288_;
    if _e89 {
        return;
    }
    let _e91 = unnamed.roughnessMips;
    if (_e91 > 1u) {
        let _e94 = unnamed.roughnessIdx;
        let _e97 = unnamed.roughnessMips;
        local = (f32(_e94) / f32((_e97 - 1u)));
    } else {
        local = 0f;
    }
    let _e101 = local;
    roughness_1 = _e101;
    let _e102 = coord;
    let _e107 = size;
    uv_1 = ((((vec2<f32>(_e102.xy) + vec2(0.5f)) / vec2<f32>(_e107)) * 2f) - vec2(1f));
    let _e114 = coord[2u];
    param_1 = bitcast<u32>(_e114);
    let _e116 = uv_1;
    param_2 = _e116;
    let _e117 = cubeFaceDir_u0028_u1_u003b_vf2_u003b((&param_1), (&param_2));
    R = _e117;
    let _e118 = R;
    N_1 = _e118;
    let _e119 = R;
    V = _e119;
    let _e120 = roughness_1;
    if (_e120 <= 0f) {
        let _e122 = R;
        let _e123 = textureSampleLevel(srcTex, srcSamp, _e122, 0f);
        c = _e123.xyz;
        let _e125 = coord;
        let _e126 = c;
        textureStore(dstRadiance, vec2<i32>(_e125.x, _e125.y), i32(_e125.z), vec4<f32>(_e126.x, _e126.y, _e126.z, 1f));
        return;
    }
    prefiltered = vec3<f32>(0f, 0f, 0f);
    totalWeight = 0f;
    i_1 = 0u;
    loop {
        let _e136 = i_1;
        if (_e136 < 1024u) {
            let _e138 = i_1;
            param_3 = _e138;
            param_4 = 1024u;
            let _e139 = hammersley_u0028_u1_u003b_u1_u003b((&param_3), (&param_4));
            xi_1 = _e139;
            let _e140 = xi_1;
            param_5 = _e140;
            let _e141 = roughness_1;
            param_6 = _e141;
            let _e142 = N_1;
            param_7 = _e142;
            let _e143 = importanceSampleGGX_u0028_vf2_u003b_f1_u003b_vf3_u003b((&param_5), (&param_6), (&param_7));
            H_1 = _e143;
            let _e144 = V;
            let _e145 = H_1;
            let _e148 = H_1;
            let _e150 = V;
            L = normalize(((_e148 * (2f * dot(_e144, _e145))) - _e150));
            let _e153 = N_1;
            let _e154 = L;
            NdotL = max(dot(_e153, _e154), 0f);
            let _e157 = NdotL;
            if (_e157 > 0f) {
                let _e159 = L;
                let _e160 = textureSampleLevel(srcTex, srcSamp, _e159, 0f);
                let _e162 = NdotL;
                let _e164 = prefiltered;
                prefiltered = (_e164 + (_e160.xyz * _e162));
                let _e166 = NdotL;
                let _e167 = totalWeight;
                totalWeight = (_e167 + _e166);
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e169 = i_1;
            i_1 = (_e169 + bitcast<u32>(1i));
        }
    }
    let _e172 = totalWeight;
    if (_e172 > 0f) {
        let _e174 = prefiltered;
        let _e175 = totalWeight;
        local_1 = (_e174 / vec3(_e175));
    } else {
        let _e178 = R;
        let _e179 = textureSampleLevel(srcTex, srcSamp, _e178, 0f);
        local_1 = _e179.xyz;
    }
    let _e181 = local_1;
    result = _e181;
    let _e182 = coord;
    let _e183 = result;
    textureStore(dstRadiance, vec2<i32>(_e182.x, _e182.y), i32(_e182.z), vec4<f32>(_e183.x, _e183.y, _e183.z, 1f));
    return;
}

@compute @workgroup_size(8, 8, 1) 
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
