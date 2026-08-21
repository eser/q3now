@group(0) @binding(0) 
var brdfLut: texture_storage_2d<rg16float,write>;
var<private> gl_GlobalInvocationID_1: vec3<u32>;

fn geometrySchlickGGX_IBL_u0028_f1_u003b_f1_u003b(NdotV: ptr<function, f32>, roughness: ptr<function, f32>) -> f32 {
    var a: f32;
    var k: f32;

    let _e36 = (*roughness);
    a = _e36;
    let _e37 = a;
    let _e38 = a;
    k = ((_e37 * _e38) / 2f);
    let _e41 = (*NdotV);
    let _e42 = (*NdotV);
    let _e43 = k;
    let _e46 = k;
    return (_e41 / ((_e42 * (1f - _e43)) + _e46));
}

fn geometrySmith_IBL_u0028_f1_u003b_f1_u003b_f1_u003b(NdotV_1: ptr<function, f32>, NdotL: ptr<function, f32>, roughness_1: ptr<function, f32>) -> f32 {
    var param: f32;
    var param_1: f32;
    var param_2: f32;
    var param_3: f32;

    let _e39 = (*NdotV_1);
    param = _e39;
    let _e40 = (*roughness_1);
    param_1 = _e40;
    let _e41 = geometrySchlickGGX_IBL_u0028_f1_u003b_f1_u003b((&param), (&param_1));
    let _e42 = (*NdotL);
    param_2 = _e42;
    let _e43 = (*roughness_1);
    param_3 = _e43;
    let _e44 = geometrySchlickGGX_IBL_u0028_f1_u003b_f1_u003b((&param_2), (&param_3));
    return (_e41 * _e44);
}

fn importanceSampleGGX_u0028_vf2_u003b_f1_u003b_vf3_u003b(xi: ptr<function, vec2<f32>>, roughness_2: ptr<function, f32>, N: ptr<function, vec3<f32>>) -> vec3<f32> {
    var a_1: f32;
    var phi: f32;
    var cosTheta: f32;
    var sinTheta: f32;
    var H: vec3<f32>;
    var up: vec3<f32>;
    var tangent: vec3<f32>;
    var bitangent: vec3<f32>;

    let _e43 = (*roughness_2);
    let _e44 = (*roughness_2);
    a_1 = (_e43 * _e44);
    let _e47 = (*xi)[0u];
    phi = (6.2831855f * _e47);
    let _e50 = (*xi)[1u];
    let _e52 = a_1;
    let _e53 = a_1;
    let _e57 = (*xi)[1u];
    cosTheta = sqrt(((1f - _e50) / (1f + (((_e52 * _e53) - 1f) * _e57))));
    let _e62 = cosTheta;
    let _e63 = cosTheta;
    sinTheta = sqrt((1f - (_e62 * _e63)));
    let _e67 = phi;
    let _e69 = sinTheta;
    H[0u] = (cos(_e67) * _e69);
    let _e72 = phi;
    let _e74 = sinTheta;
    H[1u] = (sin(_e72) * _e74);
    let _e77 = cosTheta;
    H[2u] = _e77;
    let _e80 = (*N)[2u];
    up = select(vec3<f32>(1f, 0f, 0f), vec3<f32>(0f, 0f, 1f), vec3((abs(_e80) < 0.999f)));
    let _e85 = up;
    let _e86 = (*N);
    tangent = normalize(cross(_e85, _e86));
    let _e89 = (*N);
    let _e90 = tangent;
    bitangent = cross(_e89, _e90);
    let _e92 = tangent;
    let _e94 = H[0u];
    let _e96 = bitangent;
    let _e98 = H[1u];
    let _e101 = (*N);
    let _e103 = H[2u];
    return normalize((((_e92 * _e94) + (_e96 * _e98)) + (_e101 * _e103)));
}

fn radicalInverse_VdC_u0028_u1_u003b(bits: ptr<function, u32>) -> f32 {
    let _e33 = (*bits);
    let _e36 = (*bits);
    (*bits) = ((_e33 << bitcast<u32>(16u)) | (_e36 >> bitcast<u32>(16u)));
    let _e40 = (*bits);
    let _e44 = (*bits);
    (*bits) = (((_e40 & 1431655765u) << bitcast<u32>(1u)) | ((_e44 & 2863311530u) >> bitcast<u32>(1u)));
    let _e49 = (*bits);
    let _e53 = (*bits);
    (*bits) = (((_e49 & 858993459u) << bitcast<u32>(2u)) | ((_e53 & 3435973836u) >> bitcast<u32>(2u)));
    let _e58 = (*bits);
    let _e62 = (*bits);
    (*bits) = (((_e58 & 252645135u) << bitcast<u32>(4u)) | ((_e62 & 4042322160u) >> bitcast<u32>(4u)));
    let _e67 = (*bits);
    let _e71 = (*bits);
    (*bits) = (((_e67 & 16711935u) << bitcast<u32>(8u)) | ((_e71 & 4278255360u) >> bitcast<u32>(8u)));
    let _e76 = (*bits);
    return (f32(_e76) * 0.00000000023283064f);
}

fn hammersley_u0028_u1_u003b_u1_u003b(i: ptr<function, u32>, n: ptr<function, u32>) -> vec2<f32> {
    var param_4: u32;

    let _e35 = (*i);
    let _e37 = (*n);
    let _e40 = (*i);
    param_4 = _e40;
    let _e41 = radicalInverse_VdC_u0028_u1_u003b((&param_4));
    return vec2<f32>((f32(_e35) / f32(_e37)), _e41);
}

fn integrateBRDF_u0028_f1_u003b_f1_u003b(NdotV_2: ptr<function, f32>, roughness_3: ptr<function, f32>) -> vec2<f32> {
    var V: vec3<f32>;
    var A: f32;
    var B: f32;
    var N_1: vec3<f32>;
    var i_1: u32;
    var xi_1: vec2<f32>;
    var param_5: u32;
    var param_6: u32;
    var H_1: vec3<f32>;
    var param_7: vec2<f32>;
    var param_8: f32;
    var param_9: vec3<f32>;
    var L: vec3<f32>;
    var NdotL_1: f32;
    var NdotH: f32;
    var VdotH: f32;
    var G: f32;
    var param_10: f32;
    var param_11: f32;
    var param_12: f32;
    var G_Vis: f32;
    var Fc: f32;

    let _e56 = (*NdotV_2);
    let _e57 = (*NdotV_2);
    V[0u] = sqrt((1f - (_e56 * _e57)));
    V[1u] = 0f;
    let _e63 = (*NdotV_2);
    V[2u] = _e63;
    A = 0f;
    B = 0f;
    N_1 = vec3<f32>(0f, 0f, 1f);
    i_1 = 0u;
    loop {
        let _e65 = i_1;
        if (_e65 < 1024u) {
            let _e67 = i_1;
            param_5 = _e67;
            param_6 = 1024u;
            let _e68 = hammersley_u0028_u1_u003b_u1_u003b((&param_5), (&param_6));
            xi_1 = _e68;
            let _e69 = xi_1;
            param_7 = _e69;
            let _e70 = (*roughness_3);
            param_8 = _e70;
            let _e71 = N_1;
            param_9 = _e71;
            let _e72 = importanceSampleGGX_u0028_vf2_u003b_f1_u003b_vf3_u003b((&param_7), (&param_8), (&param_9));
            H_1 = _e72;
            let _e73 = V;
            let _e74 = H_1;
            let _e77 = H_1;
            let _e79 = V;
            L = normalize(((_e77 * (2f * dot(_e73, _e74))) - _e79));
            let _e83 = L[2u];
            NdotL_1 = max(_e83, 0f);
            let _e86 = H_1[2u];
            NdotH = max(_e86, 0f);
            let _e88 = V;
            let _e89 = H_1;
            VdotH = max(dot(_e88, _e89), 0f);
            let _e92 = NdotL_1;
            if (_e92 > 0f) {
                let _e94 = (*NdotV_2);
                param_10 = _e94;
                let _e95 = NdotL_1;
                param_11 = _e95;
                let _e96 = (*roughness_3);
                param_12 = _e96;
                let _e97 = geometrySmith_IBL_u0028_f1_u003b_f1_u003b_f1_u003b((&param_10), (&param_11), (&param_12));
                G = _e97;
                let _e98 = G;
                let _e99 = VdotH;
                let _e101 = NdotH;
                let _e102 = (*NdotV_2);
                G_Vis = ((_e98 * _e99) / (_e101 * _e102));
                let _e105 = VdotH;
                Fc = pow((1f - _e105), 5f);
                let _e108 = Fc;
                let _e110 = G_Vis;
                let _e112 = A;
                A = (_e112 + ((1f - _e108) * _e110));
                let _e114 = Fc;
                let _e115 = G_Vis;
                let _e117 = B;
                B = (_e117 + (_e114 * _e115));
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e119 = i_1;
            i_1 = (_e119 + bitcast<u32>(1i));
        }
    }
    let _e122 = A;
    let _e123 = B;
    return (vec2<f32>(_e122, _e123) / vec2(1024f));
}

fn main_1() {
    var size: vec2<i32>;
    var coord: vec2<i32>;
    var NdotV_3: f32;
    var roughness_4: f32;
    var ab: vec2<f32>;
    var param_13: f32;
    var param_14: f32;
    var phi_358_: bool;

    let _e39 = textureDimensions(brdfLut);
    size = vec2<i32>(_e39);
    let _e41 = gl_GlobalInvocationID_1;
    coord = bitcast<vec2<i32>>(_e41.xy);
    let _e45 = coord[0u];
    let _e47 = size[0u];
    let _e48 = (_e45 >= _e47);
    phi_358_ = _e48;
    if !(_e48) {
        let _e51 = coord[1u];
        let _e53 = size[1u];
        phi_358_ = (_e51 >= _e53);
    }
    let _e56 = phi_358_;
    if _e56 {
        return;
    }
    let _e58 = coord[0u];
    let _e62 = size[0u];
    NdotV_3 = ((f32(_e58) + 0.5f) / f32(_e62));
    let _e66 = coord[1u];
    let _e70 = size[1u];
    roughness_4 = ((f32(_e66) + 0.5f) / f32(_e70));
    let _e73 = NdotV_3;
    NdotV_3 = max(_e73, 0.0001f);
    let _e75 = NdotV_3;
    param_13 = _e75;
    let _e76 = roughness_4;
    param_14 = _e76;
    let _e77 = integrateBRDF_u0028_f1_u003b_f1_u003b((&param_13), (&param_14));
    ab = _e77;
    let _e78 = coord;
    let _e79 = ab;
    textureStore(brdfLut, _e78, vec4<f32>(_e79.x, _e79.y, 0f, 0f));
    return;
}

@compute @workgroup_size(16, 16, 1) 
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
