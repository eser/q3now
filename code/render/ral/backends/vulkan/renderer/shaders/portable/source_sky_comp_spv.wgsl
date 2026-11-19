struct Push {
    meta_: vec4<u32>,
    authoredRadiance: vec4<f32>,
    sunDirectionIntensity: vec4<f32>,
    sunRadianceExposure: vec4<f32>,
}

@group(0) @binding(8)
var<uniform> p: Push;
@group(0) @binding(1)
var authoredRt: texture_2d<f32>;
@group(0) @binding(7)
var authoredSampler: sampler;
@group(0) @binding(3)
var authoredLf: texture_2d<f32>;
@group(0) @binding(2)
var authoredBk: texture_2d<f32>;
@group(0) @binding(4)
var authoredFt: texture_2d<f32>;
@group(0) @binding(5)
var authoredUp: texture_2d<f32>;
@group(0) @binding(6)
var authoredDn: texture_2d<f32>;
@group(0) @binding(0)
var skyImage: texture_storage_2d_array<rgba16float,write>;
var<private> gl_GlobalInvocationID_1: vec3<u32>;

fn analyticSky_u0028_vf3_u003b(dir: ptr<function, vec3<f32>>) -> vec3<f32> {
    var authored: vec3<f32>;
    var authoredEnergy: f32;
    var authoredTint: vec3<f32>;
    var horizon: vec3<f32>;
    var zenith: vec3<f32>;
    var ground: vec3<f32>;
    var up: f32;
    var t: f32;
    var sky: vec3<f32>;
    var t_1: f32;
    var sunDirection: vec3<f32>;
    var sunLength: f32;
    var sd: f32;
    var local: f32;
    var lobe: f32;

    let _e59 = p.authoredRadiance;
    authored = max(_e59.xyz, vec3<f32>(0.002f, 0.002f, 0.002f));
    let _e62 = authored;
    authoredEnergy = max(dot(_e62, vec3<f32>(0.2126f, 0.7152f, 0.0722f)), 0.002f);
    let _e65 = authored;
    let _e66 = authoredEnergy;
    authoredTint = (_e65 / vec3(_e66));
    let _e69 = authored;
    horizon = _e69;
    let _e70 = authoredTint;
    let _e71 = authoredEnergy;
    zenith = ((_e70 * _e71) * 0.55f);
    let _e74 = authored;
    ground = (_e74 * 0.18f);
    let _e77 = (*dir)[2u];
    up = _e77;
    let _e78 = up;
    if (_e78 >= 0f) {
        let _e80 = up;
        t = smoothstep(0f, 1f, _e80);
        let _e82 = horizon;
        let _e83 = zenith;
        let _e84 = t;
        sky = mix(_e82, _e83, vec3(_e84));
    } else {
        let _e87 = up;
        t_1 = smoothstep(0f, 0.35f, -(_e87));
        let _e90 = horizon;
        let _e91 = ground;
        let _e92 = t_1;
        sky = mix(_e90, _e91, vec3(_e92));
    }
    let _e96 = p.sunDirectionIntensity;
    sunDirection = _e96.xyz;
    let _e98 = sunDirection;
    sunLength = length(_e98);
    let _e100 = sunLength;
    if (_e100 > 0.00001f) {
        let _e102 = (*dir);
        let _e103 = sunDirection;
        let _e104 = sunLength;
        local = max(dot(_e102, (_e103 / vec3(_e104))), 0f);
    } else {
        local = 0f;
    }
    let _e109 = local;
    sd = _e109;
    let _e110 = sd;
    lobe = pow(_e110, 320f);
    let _e112 = up;
    if (_e112 > 0f) {
        let _e115 = p.sunRadianceExposure;
        let _e120 = p.sunDirectionIntensity[3u];
        let _e123 = lobe;
        let _e125 = sky;
        sky = (_e125 + ((max(_e115.xyz, vec3<f32>(0f, 0f, 0f)) * max(_e120, 0f)) * _e123));
    }
    let _e127 = sky;
    return max(_e127, vec3<f32>(0f, 0f, 0f));
}

fn authoredSky_u0028_vf3_u003b(dir_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var a: vec3<f32>;
    var axis: u32;
    var st: vec2<f32>;
    var uv: vec2<f32>;
    var phi_229_: bool;

    let _e47 = (*dir_1);
    a = abs(_e47);
    let _e50 = a[0u];
    let _e52 = a[1u];
    let _e53 = (_e50 >= _e52);
    phi_229_ = _e53;
    if _e53 {
        let _e55 = a[0u];
        let _e57 = a[2u];
        phi_229_ = (_e55 >= _e57);
    }
    let _e60 = phi_229_;
    if _e60 {
        let _e62 = (*dir_1)[0u];
        if (_e62 >= 0f) {
            axis = 0u;
            let _e65 = (*dir_1)[1u];
            let _e68 = (*dir_1)[2u];
            let _e71 = a[0u];
            st = (vec2<f32>(-(_e65), _e68) / vec2(_e71));
        } else {
            axis = 1u;
            let _e75 = (*dir_1)[1u];
            let _e77 = (*dir_1)[2u];
            let _e80 = a[0u];
            st = (vec2<f32>(_e75, _e77) / vec2(_e80));
        }
    } else {
        let _e84 = a[1u];
        let _e86 = a[2u];
        if (_e84 >= _e86) {
            let _e89 = (*dir_1)[1u];
            if (_e89 >= 0f) {
                axis = 2u;
                let _e92 = (*dir_1)[0u];
                let _e94 = (*dir_1)[2u];
                let _e97 = a[1u];
                st = (vec2<f32>(_e92, _e94) / vec2(_e97));
            } else {
                axis = 3u;
                let _e101 = (*dir_1)[0u];
                let _e104 = (*dir_1)[2u];
                let _e107 = a[1u];
                st = (vec2<f32>(-(_e101), _e104) / vec2(_e107));
            }
        } else {
            let _e111 = (*dir_1)[2u];
            if (_e111 >= 0f) {
                axis = 4u;
                let _e114 = (*dir_1)[1u];
                let _e117 = (*dir_1)[0u];
                let _e121 = a[2u];
                st = (vec2<f32>(-(_e114), -(_e117)) / vec2(_e121));
            } else {
                axis = 5u;
                let _e125 = (*dir_1)[1u];
                let _e128 = (*dir_1)[0u];
                let _e131 = a[2u];
                st = (vec2<f32>(-(_e125), _e128) / vec2(_e131));
            }
        }
    }
    let _e135 = st[0u];
    let _e139 = st[1u];
    uv = vec2<f32>(((_e135 * 0.5f) + 0.5f), (0.5f - (_e139 * 0.5f)));
    let _e143 = axis;
    if (_e143 == 0u) {
        let _e145 = uv;
        let _e146 = textureSampleLevel(authoredRt, authoredSampler, _e145, 0f);
        return _e146.xyz;
    }
    let _e148 = axis;
    if (_e148 == 1u) {
        let _e150 = uv;
        let _e151 = textureSampleLevel(authoredLf, authoredSampler, _e150, 0f);
        return _e151.xyz;
    }
    let _e153 = axis;
    if (_e153 == 2u) {
        let _e155 = uv;
        let _e156 = textureSampleLevel(authoredBk, authoredSampler, _e155, 0f);
        return _e156.xyz;
    }
    let _e158 = axis;
    if (_e158 == 3u) {
        let _e160 = uv;
        let _e161 = textureSampleLevel(authoredFt, authoredSampler, _e160, 0f);
        return _e161.xyz;
    }
    let _e163 = axis;
    if (_e163 == 4u) {
        let _e165 = uv;
        let _e166 = textureSampleLevel(authoredUp, authoredSampler, _e165, 0f);
        return _e166.xyz;
    }
    let _e168 = uv;
    let _e169 = textureSampleLevel(authoredDn, authoredSampler, _e168, 0f);
    return _e169.xyz;
}

fn wired_sky_cube_direction_u0028_u1_u003b_vf2_u003b(face: ptr<function, u32>, uv_1: ptr<function, vec2<f32>>) -> vec3<f32> {
    var u: f32;
    var v: f32;
    var dir_2: vec3<f32>;

    let _e48 = (*uv_1)[0u];
    u = _e48;
    let _e50 = (*uv_1)[1u];
    v = _e50;
    let _e51 = (*face);
    if (_e51 == 0u) {
        let _e53 = v;
        let _e55 = u;
        dir_2 = vec3<f32>(1f, -(_e53), -(_e55));
    } else {
        let _e58 = (*face);
        if (_e58 == 1u) {
            let _e60 = v;
            let _e62 = u;
            dir_2 = vec3<f32>(-1f, -(_e60), _e62);
        } else {
            let _e64 = (*face);
            if (_e64 == 2u) {
                let _e66 = u;
                let _e67 = v;
                dir_2 = vec3<f32>(_e66, 1f, _e67);
            } else {
                let _e69 = (*face);
                if (_e69 == 3u) {
                    let _e71 = u;
                    let _e72 = v;
                    dir_2 = vec3<f32>(_e71, -1f, -(_e72));
                } else {
                    let _e75 = (*face);
                    if (_e75 == 4u) {
                        let _e77 = u;
                        let _e78 = v;
                        dir_2 = vec3<f32>(_e77, -(_e78), 1f);
                    } else {
                        let _e81 = u;
                        let _e83 = v;
                        dir_2 = vec3<f32>(-(_e81), -(_e83), -1f);
                    }
                }
            }
        }
    }
    let _e86 = dir_2;
    return normalize(_e86);
}

fn main_1() {
    var size: vec2<u32>;
    var face_1: u32;
    var coord: vec3<i32>;
    var uv_2: vec2<f32>;
    var dir_3: vec3<f32>;
    var param: u32;
    var param_1: vec2<f32>;
    var radiance: vec3<f32>;
    var local_1: vec3<f32>;
    var param_2: vec3<f32>;
    var param_3: vec3<f32>;
    var sunDirection_1: vec3<f32>;
    var sunLength_1: f32;
    var sd_1: f32;
    var local_2: f32;
    var phi_453_: bool;

    let _e57 = textureDimensions(skyImage);
    size = bitcast<vec2<u32>>(vec2<i32>(_e57).xy);
    let _e63 = p.meta_[0u];
    face_1 = _e63;
    let _e64 = gl_GlobalInvocationID_1;
    let _e66 = bitcast<vec2<i32>>(_e64.xy);
    let _e67 = face_1;
    coord = vec3<i32>(_e66.x, _e66.y, bitcast<i32>(_e67));
    let _e73 = coord[0u];
    let _e76 = size[0u];
    let _e77 = (bitcast<u32>(_e73) >= _e76);
    phi_453_ = _e77;
    if !(_e77) {
        let _e80 = coord[1u];
        let _e83 = size[1u];
        phi_453_ = (bitcast<u32>(_e80) >= _e83);
    }
    let _e86 = phi_453_;
    let _e87 = face_1;
    if (_e86 || (_e87 >= 6u)) {
        return;
    }
    let _e90 = coord;
    let _e95 = size;
    uv_2 = ((((vec2<f32>(_e90.xy) + vec2(0.5f)) / vec2<f32>(_e95)) * 2f) - vec2(1f));
    let _e101 = face_1;
    param = _e101;
    let _e102 = uv_2;
    param_1 = _e102;
    let _e103 = wired_sky_cube_direction_u0028_u1_u003b_vf2_u003b((&param), (&param_1));
    dir_3 = _e103;
    let _e106 = p.meta_[3u];
    if (_e106 != 0u) {
        let _e108 = dir_3;
        param_2 = _e108;
        let _e109 = authoredSky_u0028_vf3_u003b((&param_2));
        local_1 = max(_e109, vec3<f32>(0f, 0f, 0f));
    } else {
        let _e111 = dir_3;
        param_3 = _e111;
        let _e112 = analyticSky_u0028_vf3_u003b((&param_3));
        local_1 = _e112;
    }
    let _e113 = local_1;
    radiance = _e113;
    let _e116 = p.meta_[3u];
    if (_e116 != 0u) {
        let _e119 = p.sunDirectionIntensity;
        sunDirection_1 = _e119.xyz;
        let _e121 = sunDirection_1;
        sunLength_1 = length(_e121);
        let _e123 = sunLength_1;
        if (_e123 > 0.00001f) {
            let _e125 = dir_3;
            let _e126 = sunDirection_1;
            let _e127 = sunLength_1;
            local_2 = max(dot(_e125, (_e126 / vec3(_e127))), 0f);
        } else {
            local_2 = 0f;
        }
        let _e132 = local_2;
        sd_1 = _e132;
        let _e134 = p.sunRadianceExposure;
        let _e139 = p.sunDirectionIntensity[3u];
        let _e142 = sd_1;
        let _e145 = radiance;
        radiance = (_e145 + ((max(_e134.xyz, vec3<f32>(0f, 0f, 0f)) * max(_e139, 0f)) * pow(_e142, 320f)));
    }
    let _e149 = p.sunRadianceExposure[3u];
    let _e151 = radiance;
    radiance = (_e151 * clamp(_e149, 0f, 8f));
    let _e153 = coord;
    let _e154 = radiance;
    textureStore(skyImage, vec2<i32>(_e153.x, _e153.y), i32(_e153.z), vec4<f32>(_e154.x, _e154.y, _e154.z, 1f));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
