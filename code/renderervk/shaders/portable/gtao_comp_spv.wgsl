struct GtaoPush {
    viewportSize: vec2<f32>,
    ndcToViewMul: vec2<f32>,
    ndcToViewAdd: vec2<f32>,
    zNear: f32,
    zFar: f32,
    effectRadius: f32,
    falloffRange: f32,
    sliceCount: i32,
    stepsPerSlice: i32,
    finalPower: f32,
    intensity: f32,
    sscsSunDirVS: vec4<f32>,
    sscsRadius: f32,
    sscsSteps: i32,
    sscsStrength: f32,
    sscsEnabled: i32,
}

@group(0) @binding(3)
var<uniform> pc: GtaoPush;
@group(0) @binding(0) 
var depthTex: texture_2d<f32>;
@group(0) @binding(1) 
var depthSampler: sampler;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(2) 
var aoImage: texture_storage_2d<r16float,write>;

fn linearizeDepth_u0028_f1_u003b(z: ptr<function, f32>) -> f32 {
    let _e49 = pc.zNear;
    let _e51 = pc.zFar;
    let _e54 = pc.zNear;
    let _e55 = (*z);
    let _e57 = pc.zFar;
    let _e59 = pc.zNear;
    return ((_e49 * _e51) / (_e54 + (_e55 * (_e57 - _e59))));
}

fn sampleDeviceDepth_u0028_vf2_u003b(uv: ptr<function, vec2<f32>>) -> f32 {
    let _e48 = (*uv);
    let _e49 = textureSampleLevel(depthTex, depthSampler, _e48, 0f);
    return _e49.x;
}

fn sscsContact_u0028_vf3_u003b_vf2_u003b_f1_u003b(P: ptr<function, vec3<f32>>, texel: ptr<function, vec2<f32>>, noise: ptr<function, f32>) -> f32 {
    var L: vec3<f32>;
    var stepLen: f32;
    var occluded: f32;
    var i: i32;
    var t: f32;
    var sp: vec3<f32>;
    var ndc: vec2<f32>;
    var uvS: vec2<f32>;
    var zScene: f32;
    var param: vec2<f32>;
    var sceneVz: f32;
    var param_1: f32;
    var diff: f32;
    var phi_315_: bool;
    var phi_396_: bool;
    var phi_403_: bool;
    var phi_410_: bool;
    var phi_442_: bool;

    let _e64 = pc.sscsEnabled;
    let _e65 = (_e64 == 0i);
    phi_315_ = _e65;
    if !(_e65) {
        let _e68 = pc.sscsStrength;
        phi_315_ = (_e68 <= 0f);
    }
    let _e71 = phi_315_;
    if _e71 {
        return 1f;
    }
    let _e73 = pc.sscsSunDirVS;
    L = _e73.xyz;
    let _e76 = pc.sscsRadius;
    let _e78 = pc.sscsSteps;
    stepLen = (_e76 / f32(max(_e78, 1i)));
    occluded = 0f;
    i = 1i;
    loop {
        let _e82 = i;
        let _e84 = pc.sscsSteps;
        if (_e82 <= _e84) {
            let _e86 = i;
            let _e89 = (*noise);
            let _e91 = stepLen;
            t = (((f32(_e86) - 0.5f) + _e89) * _e91);
            let _e93 = (*P);
            let _e94 = L;
            let _e95 = t;
            sp = (_e93 + (_e94 * _e95));
            let _e99 = sp[2u];
            if (_e99 <= 0.001f) {
                break;
            }
            let _e101 = sp;
            let _e104 = sp[2u];
            let _e108 = pc.ndcToViewAdd;
            let _e111 = pc.ndcToViewMul;
            ndc = (((_e101.xy / vec2(_e104)) - _e108) / _e111);
            let _e113 = ndc;
            uvS = ((_e113 * 0.5f) + vec2(0.5f));
            let _e118 = uvS[0u];
            let _e119 = (_e118 < 0f);
            phi_396_ = _e119;
            if !(_e119) {
                let _e122 = uvS[0u];
                phi_396_ = (_e122 > 1f);
            }
            let _e125 = phi_396_;
            phi_403_ = _e125;
            if !(_e125) {
                let _e128 = uvS[1u];
                phi_403_ = (_e128 < 0f);
            }
            let _e131 = phi_403_;
            phi_410_ = _e131;
            if !(_e131) {
                let _e134 = uvS[1u];
                phi_410_ = (_e134 > 1f);
            }
            let _e137 = phi_410_;
            if _e137 {
                break;
            }
            let _e138 = uvS;
            param = _e138;
            let _e139 = sampleDeviceDepth_u0028_vf2_u003b((&param));
            zScene = _e139;
            let _e140 = zScene;
            if (_e140 <= 0f) {
                continue;
            }
            let _e142 = zScene;
            param_1 = _e142;
            let _e143 = linearizeDepth_u0028_f1_u003b((&param_1));
            sceneVz = _e143;
            let _e145 = sp[2u];
            let _e146 = sceneVz;
            diff = (_e145 - _e146);
            let _e148 = diff;
            let _e149 = stepLen;
            let _e151 = (_e148 > (_e149 * 0.5f));
            phi_442_ = _e151;
            if _e151 {
                let _e152 = diff;
                let _e154 = pc.sscsRadius;
                phi_442_ = (_e152 < _e154);
            }
            let _e157 = phi_442_;
            if _e157 {
                occluded = 1f;
                break;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e158 = i;
            i = (_e158 + 1i);
        }
    }
    let _e161 = pc.sscsStrength;
    let _e163 = occluded;
    return mix(1f, (1f - _e161), _e163);
}

fn fastAcos_u0028_f1_u003b(x: ptr<function, f32>) -> f32 {
    var xa: f32;
    var r: f32;
    var local: f32;

    let _e51 = (*x);
    xa = abs(_e51);
    let _e53 = xa;
    let _e56 = xa;
    r = (((-0.156583f * _e53) + 1.5707964f) * sqrt((1f - _e56)));
    let _e60 = (*x);
    if (_e60 >= 0f) {
        let _e62 = r;
        local = _e62;
    } else {
        let _e63 = r;
        local = (3.1415927f - _e63);
    }
    let _e65 = local;
    return _e65;
}

fn ign_u0028_vf2_u003b(pix: ptr<function, vec2<f32>>) -> f32 {
    let _e48 = (*pix);
    return fract((52.982918f * fract(dot(_e48, vec2<f32>(0.06711056f, 0.00583715f)))));
}

fn viewPosFromUV_u0028_vf2_u003b_f1_u003b(uv_1: ptr<function, vec2<f32>>, deviceZ: ptr<function, f32>) -> vec3<f32> {
    var vz: f32;
    var param_2: f32;
    var ndc_1: vec2<f32>;
    var vxy: vec2<f32>;

    let _e53 = (*deviceZ);
    param_2 = _e53;
    let _e54 = linearizeDepth_u0028_f1_u003b((&param_2));
    vz = _e54;
    let _e55 = (*uv_1);
    ndc_1 = ((_e55 * 2f) - vec2(1f));
    let _e59 = ndc_1;
    let _e61 = pc.ndcToViewMul;
    let _e64 = pc.ndcToViewAdd;
    let _e66 = vz;
    vxy = (((_e59 * _e61) + _e64) * _e66);
    let _e68 = vxy;
    let _e69 = vz;
    return vec3<f32>(_e68.x, _e68.y, _e69);
}

fn normalFromDepth_u0028_vf2_u003b_vf3_u003b_vf2_u003b(uv_2: ptr<function, vec2<f32>>, P_1: ptr<function, vec3<f32>>, texel_1: ptr<function, vec2<f32>>) -> vec3<f32> {
    var zL: f32;
    var param_3: vec2<f32>;
    var zR: f32;
    var param_4: vec2<f32>;
    var zU: f32;
    var param_5: vec2<f32>;
    var zD: f32;
    var param_6: vec2<f32>;
    var Pl: vec3<f32>;
    var param_7: vec2<f32>;
    var param_8: f32;
    var Pr: vec3<f32>;
    var param_9: vec2<f32>;
    var param_10: f32;
    var Pu: vec3<f32>;
    var param_11: vec2<f32>;
    var param_12: f32;
    var Pd: vec3<f32>;
    var param_13: vec2<f32>;
    var param_14: f32;
    var dPdx: vec3<f32>;
    var local_1: vec3<f32>;
    var dPdy: vec3<f32>;
    var local_2: vec3<f32>;
    var n: vec3<f32>;
    var len: f32;
    var local_3: vec3<f32>;

    let _e77 = (*uv_2);
    let _e79 = (*texel_1)[0u];
    param_3 = (_e77 - vec2<f32>(_e79, 0f));
    let _e82 = sampleDeviceDepth_u0028_vf2_u003b((&param_3));
    zL = _e82;
    let _e83 = (*uv_2);
    let _e85 = (*texel_1)[0u];
    param_4 = (_e83 + vec2<f32>(_e85, 0f));
    let _e88 = sampleDeviceDepth_u0028_vf2_u003b((&param_4));
    zR = _e88;
    let _e89 = (*uv_2);
    let _e91 = (*texel_1)[1u];
    param_5 = (_e89 - vec2<f32>(0f, _e91));
    let _e94 = sampleDeviceDepth_u0028_vf2_u003b((&param_5));
    zU = _e94;
    let _e95 = (*uv_2);
    let _e97 = (*texel_1)[1u];
    param_6 = (_e95 + vec2<f32>(0f, _e97));
    let _e100 = sampleDeviceDepth_u0028_vf2_u003b((&param_6));
    zD = _e100;
    let _e101 = (*uv_2);
    let _e103 = (*texel_1)[0u];
    param_7 = (_e101 - vec2<f32>(_e103, 0f));
    let _e106 = zL;
    param_8 = _e106;
    let _e107 = viewPosFromUV_u0028_vf2_u003b_f1_u003b((&param_7), (&param_8));
    Pl = _e107;
    let _e108 = (*uv_2);
    let _e110 = (*texel_1)[0u];
    param_9 = (_e108 + vec2<f32>(_e110, 0f));
    let _e113 = zR;
    param_10 = _e113;
    let _e114 = viewPosFromUV_u0028_vf2_u003b_f1_u003b((&param_9), (&param_10));
    Pr = _e114;
    let _e115 = (*uv_2);
    let _e117 = (*texel_1)[1u];
    param_11 = (_e115 - vec2<f32>(0f, _e117));
    let _e120 = zU;
    param_12 = _e120;
    let _e121 = viewPosFromUV_u0028_vf2_u003b_f1_u003b((&param_11), (&param_12));
    Pu = _e121;
    let _e122 = (*uv_2);
    let _e124 = (*texel_1)[1u];
    param_13 = (_e122 + vec2<f32>(0f, _e124));
    let _e127 = zD;
    param_14 = _e127;
    let _e128 = viewPosFromUV_u0028_vf2_u003b_f1_u003b((&param_13), (&param_14));
    Pd = _e128;
    let _e130 = Pr[2u];
    let _e132 = (*P_1)[2u];
    let _e136 = (*P_1)[2u];
    let _e138 = Pl[2u];
    if (abs((_e130 - _e132)) < abs((_e136 - _e138))) {
        let _e142 = Pr;
        let _e143 = (*P_1);
        local_1 = (_e142 - _e143);
    } else {
        let _e145 = (*P_1);
        let _e146 = Pl;
        local_1 = (_e145 - _e146);
    }
    let _e148 = local_1;
    dPdx = _e148;
    let _e150 = Pd[2u];
    let _e152 = (*P_1)[2u];
    let _e156 = (*P_1)[2u];
    let _e158 = Pu[2u];
    if (abs((_e150 - _e152)) < abs((_e156 - _e158))) {
        let _e162 = Pd;
        let _e163 = (*P_1);
        local_2 = (_e162 - _e163);
    } else {
        let _e165 = (*P_1);
        let _e166 = Pu;
        local_2 = (_e165 - _e166);
    }
    let _e168 = local_2;
    dPdy = _e168;
    let _e169 = dPdx;
    let _e170 = dPdy;
    n = cross(_e169, _e170);
    let _e172 = n;
    len = length(_e172);
    let _e174 = len;
    if (_e174 > 0.00000001f) {
        let _e176 = n;
        let _e177 = len;
        local_3 = (_e176 / vec3(_e177));
    } else {
        local_3 = vec3<f32>(0f, 0f, 1f);
    }
    let _e180 = local_3;
    return _e180;
}

fn main_1() {
    var pix_1: vec2<i32>;
    var dim: vec2<i32>;
    var texel_2: vec2<f32>;
    var uv_3: vec2<f32>;
    var centerZ: f32;
    var param_15: vec2<f32>;
    var P_2: vec3<f32>;
    var param_16: vec2<f32>;
    var param_17: f32;
    var N: vec3<f32>;
    var param_18: vec2<f32>;
    var param_19: vec3<f32>;
    var param_20: vec2<f32>;
    var V: vec3<f32>;
    var tanHalfFovX: f32;
    var screenRadiusUV: f32;
    var noise_1: f32;
    var param_21: vec2<f32>;
    var visibility: f32;
    var falloffMul: f32;
    var falloffAdd: f32;
    var slice: i32;
    var sliceK: f32;
    var phi: f32;
    var dir: vec2<f32>;
    var dirVec: vec3<f32>;
    var orthoDir: vec3<f32>;
    var axis: vec3<f32>;
    var projN: vec3<f32>;
    var projLen: f32;
    var signN: f32;
    var cosN: f32;
    var n_1: f32;
    var param_22: f32;
    var horizonCos0_: f32;
    var horizonCos1_: f32;
    var minS: f32;
    var step_: i32;
    var stepNoise: f32;
    var s: f32;
    var off: vec2<f32>;
    var uv0_: vec2<f32>;
    var uv1_: vec2<f32>;
    var z0_: f32;
    var param_23: vec2<f32>;
    var z1_: f32;
    var param_24: vec2<f32>;
    var s0_: vec3<f32>;
    var param_25: vec2<f32>;
    var param_26: f32;
    var s1_: vec3<f32>;
    var param_27: vec2<f32>;
    var param_28: f32;
    var d0_: vec3<f32>;
    var d1_: vec3<f32>;
    var dist0_: f32;
    var dist1_: f32;
    var w0_: f32;
    var w1_: f32;
    var shc0_: f32;
    var shc1_: f32;
    var h0_: f32;
    var param_29: f32;
    var h1_: f32;
    var param_30: f32;
    var iarc0_: f32;
    var iarc1_: f32;
    var param_31: vec3<f32>;
    var param_32: vec2<f32>;
    var param_33: f32;
    var phi_482_: bool;

    let _e117 = gl_GlobalInvocationID_1;
    pix_1 = bitcast<vec2<i32>>(_e117.xy);
    let _e121 = pc.viewportSize;
    dim = vec2<i32>(_e121);
    let _e124 = pix_1[0u];
    let _e126 = dim[0u];
    let _e127 = (_e124 >= _e126);
    phi_482_ = _e127;
    if !(_e127) {
        let _e130 = pix_1[1u];
        let _e132 = dim[1u];
        phi_482_ = (_e130 >= _e132);
    }
    let _e135 = phi_482_;
    if _e135 {
        return;
    }
    let _e137 = pc.viewportSize;
    texel_2 = (vec2(1f) / _e137);
    let _e140 = pix_1;
    let _e144 = texel_2;
    uv_3 = ((vec2<f32>(_e140) + vec2(0.5f)) * _e144);
    let _e146 = uv_3;
    param_15 = _e146;
    let _e147 = sampleDeviceDepth_u0028_vf2_u003b((&param_15));
    centerZ = _e147;
    let _e148 = centerZ;
    if (_e148 <= 0f) {
        let _e150 = pix_1;
        textureStore(aoImage, _e150, vec4<f32>(1f, 1f, 1f, 1f));
        return;
    }
    let _e151 = uv_3;
    param_16 = _e151;
    let _e152 = centerZ;
    param_17 = _e152;
    let _e153 = viewPosFromUV_u0028_vf2_u003b_f1_u003b((&param_16), (&param_17));
    P_2 = _e153;
    let _e154 = uv_3;
    param_18 = _e154;
    let _e155 = P_2;
    param_19 = _e155;
    let _e156 = texel_2;
    param_20 = _e156;
    let _e157 = normalFromDepth_u0028_vf2_u003b_vf3_u003b_vf2_u003b((&param_18), (&param_19), (&param_20));
    N = _e157;
    let _e158 = P_2;
    V = normalize(-(_e158));
    let _e163 = pc.ndcToViewMul[0u];
    tanHalfFovX = (0.5f * abs(_e163));
    let _e167 = pc.effectRadius;
    let _e169 = P_2[2u];
    let _e170 = tanHalfFovX;
    let _e175 = texel_2[0u];
    screenRadiusUV = clamp((_e167 / (_e169 * max(_e170, 0.0001f))), _e175, 0.5f);
    let _e177 = pix_1;
    param_21 = vec2<f32>(_e177);
    let _e179 = ign_u0028_vf2_u003b((&param_21));
    noise_1 = _e179;
    visibility = 0f;
    let _e181 = pc.effectRadius;
    let _e183 = pc.falloffRange;
    falloffMul = (1f / max((_e181 * _e183), 0.0001f));
    let _e188 = pc.effectRadius;
    let _e191 = pc.falloffRange;
    let _e194 = falloffMul;
    falloffAdd = ((-(_e188) * (1f - _e191)) * _e194);
    slice = 0i;
    loop {
        let _e196 = slice;
        let _e198 = pc.sliceCount;
        if (_e196 < _e198) {
            let _e200 = slice;
            let _e202 = noise_1;
            let _e205 = pc.sliceCount;
            sliceK = ((f32(_e200) + _e202) / f32(_e205));
            let _e208 = sliceK;
            phi = (_e208 * 3.1415927f);
            let _e210 = phi;
            let _e212 = phi;
            dir = vec2<f32>(cos(_e210), sin(_e212));
            let _e215 = dir;
            dirVec = vec3<f32>(_e215.x, _e215.y, 0f);
            let _e219 = dirVec;
            let _e220 = dirVec;
            let _e221 = V;
            let _e223 = V;
            orthoDir = (_e219 - (_e223 * dot(_e220, _e221)));
            let _e226 = orthoDir;
            let _e227 = V;
            axis = normalize(cross(_e226, _e227));
            let _e230 = N;
            let _e231 = axis;
            let _e232 = N;
            let _e233 = axis;
            projN = (_e230 - (_e231 * dot(_e232, _e233)));
            let _e237 = projN;
            projLen = length(_e237);
            let _e239 = projLen;
            if (_e239 < 0.00001f) {
                continue;
            }
            let _e241 = orthoDir;
            let _e242 = projN;
            signN = sign(dot(_e241, _e242));
            let _e245 = projN;
            let _e246 = V;
            let _e248 = projLen;
            cosN = clamp((dot(_e245, _e246) / _e248), -1f, 1f);
            let _e251 = signN;
            let _e252 = cosN;
            param_22 = _e252;
            let _e253 = fastAcos_u0028_f1_u003b((&param_22));
            n_1 = (_e251 * _e253);
            let _e255 = n_1;
            horizonCos0_ = cos((_e255 + 1.5707964f));
            let _e258 = n_1;
            horizonCos1_ = cos((_e258 - 1.5707964f));
            let _e262 = texel_2[0u];
            let _e264 = screenRadiusUV;
            minS = ((1.5f * _e262) / max(_e264, 0.00001f));
            step_ = 0i;
            loop {
                let _e267 = step_;
                let _e269 = pc.stepsPerSlice;
                if (_e267 < _e269) {
                    let _e271 = noise_1;
                    let _e272 = slice;
                    let _e274 = pc.stepsPerSlice;
                    let _e276 = step_;
                    stepNoise = fract((_e271 + (f32(((_e272 * _e274) + _e276)) * 0.618034f)));
                    let _e282 = step_;
                    let _e284 = stepNoise;
                    let _e287 = pc.stepsPerSlice;
                    s = ((f32(_e282) + _e284) / f32(_e287));
                    let _e290 = s;
                    s = pow(_e290, 2f);
                    let _e292 = minS;
                    let _e293 = s;
                    s = (_e293 + _e292);
                    let _e295 = dir;
                    let _e296 = s;
                    let _e298 = screenRadiusUV;
                    off = ((_e295 * _e296) * _e298);
                    let _e300 = uv_3;
                    let _e301 = off;
                    uv0_ = (_e300 + _e301);
                    let _e303 = uv_3;
                    let _e304 = off;
                    uv1_ = (_e303 - _e304);
                    let _e306 = uv0_;
                    param_23 = _e306;
                    let _e307 = sampleDeviceDepth_u0028_vf2_u003b((&param_23));
                    z0_ = _e307;
                    let _e308 = uv1_;
                    param_24 = _e308;
                    let _e309 = sampleDeviceDepth_u0028_vf2_u003b((&param_24));
                    z1_ = _e309;
                    let _e310 = uv0_;
                    param_25 = _e310;
                    let _e311 = z0_;
                    param_26 = _e311;
                    let _e312 = viewPosFromUV_u0028_vf2_u003b_f1_u003b((&param_25), (&param_26));
                    s0_ = _e312;
                    let _e313 = uv1_;
                    param_27 = _e313;
                    let _e314 = z1_;
                    param_28 = _e314;
                    let _e315 = viewPosFromUV_u0028_vf2_u003b_f1_u003b((&param_27), (&param_28));
                    s1_ = _e315;
                    let _e316 = s0_;
                    let _e317 = P_2;
                    d0_ = (_e316 - _e317);
                    let _e319 = s1_;
                    let _e320 = P_2;
                    d1_ = (_e319 - _e320);
                    let _e322 = d0_;
                    dist0_ = length(_e322);
                    let _e324 = d1_;
                    dist1_ = length(_e324);
                    let _e326 = dist0_;
                    let _e327 = falloffMul;
                    let _e329 = falloffAdd;
                    w0_ = clamp(((_e326 * _e327) + _e329), 0f, 1f);
                    let _e332 = dist1_;
                    let _e333 = falloffMul;
                    let _e335 = falloffAdd;
                    w1_ = clamp(((_e332 * _e333) + _e335), 0f, 1f);
                    let _e338 = w0_;
                    w0_ = (1f - _e338);
                    let _e340 = w1_;
                    w1_ = (1f - _e340);
                    let _e342 = d0_;
                    let _e343 = dist0_;
                    let _e347 = V;
                    shc0_ = dot((_e342 / vec3(max(_e343, 0.00001f))), _e347);
                    let _e349 = d1_;
                    let _e350 = dist1_;
                    let _e354 = V;
                    shc1_ = dot((_e349 / vec3(max(_e350, 0.00001f))), _e354);
                    let _e356 = z0_;
                    if (_e356 <= 0f) {
                        let _e358 = horizonCos0_;
                        shc0_ = _e358;
                    }
                    let _e359 = z1_;
                    if (_e359 <= 0f) {
                        let _e361 = horizonCos1_;
                        shc1_ = _e361;
                    }
                    let _e362 = horizonCos0_;
                    let _e363 = horizonCos0_;
                    let _e364 = shc0_;
                    let _e365 = w0_;
                    horizonCos0_ = max(_e362, mix(_e363, _e364, _e365));
                    let _e368 = horizonCos1_;
                    let _e369 = horizonCos1_;
                    let _e370 = shc1_;
                    let _e371 = w1_;
                    horizonCos1_ = max(_e368, mix(_e369, _e370, _e371));
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e374 = step_;
                    step_ = (_e374 + 1i);
                }
            }
            let _e376 = horizonCos1_;
            param_29 = clamp(_e376, -1f, 1f);
            let _e378 = fastAcos_u0028_f1_u003b((&param_29));
            h0_ = -(_e378);
            let _e380 = horizonCos0_;
            param_30 = clamp(_e380, -1f, 1f);
            let _e382 = fastAcos_u0028_f1_u003b((&param_30));
            h1_ = _e382;
            let _e383 = cosN;
            let _e384 = h0_;
            let _e386 = n_1;
            let _e390 = h0_;
            let _e392 = n_1;
            iarc0_ = (((_e383 + ((2f * _e384) * sin(_e386))) - cos(((2f * _e390) - _e392))) * 0.25f);
            let _e397 = cosN;
            let _e398 = h1_;
            let _e400 = n_1;
            let _e404 = h1_;
            let _e406 = n_1;
            iarc1_ = (((_e397 + ((2f * _e398) * sin(_e400))) - cos(((2f * _e404) - _e406))) * 0.25f);
            let _e411 = projLen;
            let _e412 = iarc0_;
            let _e413 = iarc1_;
            let _e416 = visibility;
            visibility = (_e416 + (_e411 * (_e412 + _e413)));
            continue;
        } else {
            break;
        }
        continuing {
            let _e418 = slice;
            slice = (_e418 + 1i);
        }
    }
    let _e421 = pc.sliceCount;
    let _e423 = visibility;
    visibility = (_e423 / f32(_e421));
    let _e425 = visibility;
    visibility = clamp(_e425, 0f, 1f);
    let _e427 = visibility;
    let _e429 = pc.finalPower;
    visibility = pow(_e427, _e429);
    let _e431 = visibility;
    let _e433 = pc.intensity;
    visibility = mix(1f, _e431, clamp(_e433, 0f, 1f));
    let _e436 = visibility;
    visibility = max(_e436, 0f);
    let _e438 = P_2;
    param_31 = _e438;
    let _e439 = texel_2;
    param_32 = _e439;
    let _e440 = noise_1;
    param_33 = _e440;
    let _e441 = sscsContact_u0028_vf3_u003b_vf2_u003b_f1_u003b((&param_31), (&param_32), (&param_33));
    let _e442 = visibility;
    visibility = (_e442 * _e441);
    let _e444 = pix_1;
    let _e445 = visibility;
    textureStore(aoImage, _e444, vec4(_e445));
    return;
}

@compute @workgroup_size(8, 8, 1) 
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
