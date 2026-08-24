struct Push {
    pad: u32,
}

@group(0) @binding(2)
var dstIrradiance: texture_storage_2d_array<rgba16float,write>;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(0)
var srcTex: texture_cube<f32>;
@group(0) @binding(1)
var srcSamp: sampler;
@group(0) @binding(3)
var<uniform> unnamed: Push;

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
    var uv_1: vec2<f32>;
    var N: vec3<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var up: vec3<f32>;
    var tangent: vec3<f32>;
    var bitangent: vec3<f32>;
    var irradiance: vec3<f32>;
    var i_1: u32;
    var xi: vec2<f32>;
    var param_3: u32;
    var param_4: u32;
    var phi: f32;
    var cosTheta: f32;
    var sinTheta: f32;
    var s: vec3<f32>;
    var sampleDir: vec3<f32>;
    var phi_193_: bool;
    var phi_202_: bool;

    let _e56 = textureDimensions(dstIrradiance);
    size = bitcast<vec2<u32>>(vec2<i32>(_e56).xy);
    let _e60 = gl_GlobalInvocationID_1;
    coord = bitcast<vec3<i32>>(_e60);
    let _e63 = coord[0u];
    let _e66 = size[0u];
    let _e67 = (bitcast<u32>(_e63) >= _e66);
    phi_193_ = _e67;
    if !(_e67) {
        let _e70 = coord[1u];
        let _e73 = size[1u];
        phi_193_ = (bitcast<u32>(_e70) >= _e73);
    }
    let _e76 = phi_193_;
    phi_202_ = _e76;
    if !(_e76) {
        let _e79 = coord[2u];
        phi_202_ = (bitcast<u32>(_e79) >= 6u);
    }
    let _e83 = phi_202_;
    if _e83 {
        return;
    }
    let _e84 = coord;
    let _e89 = size;
    uv_1 = ((((vec2<f32>(_e84.xy) + vec2(0.5f)) / vec2<f32>(_e89)) * 2f) - vec2(1f));
    let _e96 = coord[2u];
    param_1 = bitcast<u32>(_e96);
    let _e98 = uv_1;
    param_2 = _e98;
    let _e99 = cubeFaceDir_u0028_u1_u003b_vf2_u003b((&param_1), (&param_2));
    N = _e99;
    let _e101 = N[2u];
    up = select(vec3<f32>(1f, 0f, 0f), vec3<f32>(0f, 0f, 1f), vec3((abs(_e101) < 0.999f)));
    let _e106 = up;
    let _e107 = N;
    tangent = normalize(cross(_e106, _e107));
    let _e110 = N;
    let _e111 = tangent;
    bitangent = cross(_e110, _e111);
    irradiance = vec3<f32>(0f, 0f, 0f);
    i_1 = 0u;
    loop {
        let _e113 = i_1;
        if (_e113 < 256u) {
            let _e115 = i_1;
            param_3 = _e115;
            param_4 = 256u;
            let _e116 = hammersley_u0028_u1_u003b_u1_u003b((&param_3), (&param_4));
            xi = _e116;
            let _e118 = xi[0u];
            phi = (6.2831855f * _e118);
            let _e121 = xi[1u];
            cosTheta = sqrt((1f - _e121));
            let _e125 = xi[1u];
            sinTheta = sqrt(_e125);
            let _e127 = phi;
            let _e129 = sinTheta;
            let _e131 = phi;
            let _e133 = sinTheta;
            let _e135 = cosTheta;
            s = vec3<f32>((cos(_e127) * _e129), (sin(_e131) * _e133), _e135);
            let _e138 = s[0u];
            let _e139 = tangent;
            let _e142 = s[1u];
            let _e143 = bitangent;
            let _e147 = s[2u];
            let _e148 = N;
            sampleDir = (((_e139 * _e138) + (_e143 * _e142)) + (_e148 * _e147));
            let _e151 = sampleDir;
            let _e152 = textureSampleLevel(srcTex, srcSamp, _e151, 0f);
            let _e154 = irradiance;
            irradiance = (_e154 + _e152.xyz);
            continue;
        } else {
            break;
        }
        continuing {
            let _e156 = i_1;
            i_1 = (_e156 + bitcast<u32>(1i));
        }
    }
    let _e159 = irradiance;
    irradiance = ((_e159 * 3.1415927f) / vec3(256f));
    let _e163 = coord;
    let _e164 = irradiance;
    textureStore(dstIrradiance, vec2<i32>(_e163.x, _e163.y), i32(_e163.z), vec4<f32>(_e164.x, _e164.y, _e164.z, 1f));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
