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

fn skyDepthThreshold_u0028_() -> f32 {
    let _e52 = pc.zFar;
    return (min(_e52, 65000f) * 0.999f);
}

fn sampleViewDepth_u0028_vf2_u003b_f1_u003b(uv: ptr<function, vec2<f32>>, mip: ptr<function, f32>) -> f32 {
    let _e53 = (*uv);
    let _e54 = (*mip);
    let _e55 = textureSampleLevel(depthTex, depthSampler, _e53, _e54);
    return _e55.x;
}

fn sampleViewDepth0_u0028_vf2_u003b(uv_1: ptr<function, vec2<f32>>) -> f32 {
    var param: vec2<f32>;
    var param_1: f32;

    let _e54 = (*uv_1);
    param = _e54;
    param_1 = 0f;
    let _e55 = sampleViewDepth_u0028_vf2_u003b_f1_u003b((&param), (&param_1));
    return _e55;
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
    var sceneVz: f32;
    var param_2: vec2<f32>;
    var diff: f32;
    var phi_312_: bool;
    var phi_393_: bool;
    var phi_400_: bool;
    var phi_407_: bool;
    var phi_436_: bool;

    let _e66 = pc.sscsEnabled;
    let _e67 = (_e66 == 0i);
    phi_312_ = _e67;
    if !(_e67) {
        let _e70 = pc.sscsStrength;
        phi_312_ = (_e70 <= 0f);
    }
    let _e73 = phi_312_;
    if _e73 {
        return 1f;
    }
    let _e75 = pc.sscsSunDirVS;
    L = _e75.xyz;
    let _e78 = pc.sscsRadius;
    let _e80 = pc.sscsSteps;
    stepLen = (_e78 / f32(max(_e80, 1i)));
    occluded = 0f;
    i = 1i;
    loop {
        let _e84 = i;
        let _e86 = pc.sscsSteps;
        if (_e84 <= _e86) {
            let _e88 = i;
            let _e91 = (*noise);
            let _e93 = stepLen;
            t = (((f32(_e88) - 0.5f) + _e91) * _e93);
            let _e95 = (*P);
            let _e96 = L;
            let _e97 = t;
            sp = (_e95 + (_e96 * _e97));
            let _e101 = sp[2u];
            if (_e101 <= 0.001f) {
                break;
            }
            let _e103 = sp;
            let _e106 = sp[2u];
            let _e110 = pc.ndcToViewAdd;
            let _e113 = pc.ndcToViewMul;
            ndc = (((_e103.xy / vec2(_e106)) - _e110) / _e113);
            let _e115 = ndc;
            uvS = ((_e115 * 0.5f) + vec2(0.5f));
            let _e120 = uvS[0u];
            let _e121 = (_e120 < 0f);
            phi_393_ = _e121;
            if !(_e121) {
                let _e124 = uvS[0u];
                phi_393_ = (_e124 > 1f);
            }
            let _e127 = phi_393_;
            phi_400_ = _e127;
            if !(_e127) {
                let _e130 = uvS[1u];
                phi_400_ = (_e130 < 0f);
            }
            let _e133 = phi_400_;
            phi_407_ = _e133;
            if !(_e133) {
                let _e136 = uvS[1u];
                phi_407_ = (_e136 > 1f);
            }
            let _e139 = phi_407_;
            if _e139 {
                break;
            }
            let _e140 = uvS;
            param_2 = _e140;
            let _e141 = sampleViewDepth0_u0028_vf2_u003b((&param_2));
            sceneVz = _e141;
            let _e142 = sceneVz;
            let _e143 = skyDepthThreshold_u0028_();
            if (_e142 >= _e143) {
                continue;
            }
            let _e146 = sp[2u];
            let _e147 = sceneVz;
            diff = (_e146 - _e147);
            let _e149 = diff;
            let _e150 = stepLen;
            let _e152 = (_e149 > (_e150 * 0.5f));
            phi_436_ = _e152;
            if _e152 {
                let _e153 = diff;
                let _e155 = pc.sscsRadius;
                phi_436_ = (_e153 < _e155);
            }
            let _e158 = phi_436_;
            if _e158 {
                occluded = 1f;
                break;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e159 = i;
            i = (_e159 + 1i);
        }
    }
    let _e162 = pc.sscsStrength;
    let _e164 = occluded;
    return mix(1f, (1f - _e162), _e164);
}

fn fastAcos_u0028_f1_u003b(x: ptr<function, f32>) -> f32 {
    var xa: f32;
    var r: f32;
    var local: f32;

    let _e55 = (*x);
    xa = abs(_e55);
    let _e57 = xa;
    let _e60 = xa;
    r = (((-0.156583f * _e57) + 1.5707964f) * sqrt((1f - _e60)));
    let _e64 = (*x);
    if (_e64 >= 0f) {
        let _e66 = r;
        local = _e66;
    } else {
        let _e67 = r;
        local = (3.1415927f - _e67);
    }
    let _e69 = local;
    return _e69;
}

fn ign_u0028_vf2_u003b(pix: ptr<function, vec2<f32>>) -> f32 {
    let _e52 = (*pix);
    return fract((52.982918f * fract(dot(_e52, vec2<f32>(0.06711056f, 0.00583715f)))));
}

fn viewPosFromUV_u0028_vf2_u003b_f1_u003b(uv_2: ptr<function, vec2<f32>>, vz: ptr<function, f32>) -> vec3<f32> {
    var ndc_1: vec2<f32>;
    var vxy: vec2<f32>;

    let _e55 = (*uv_2);
    ndc_1 = ((_e55 * 2f) - vec2(1f));
    let _e59 = ndc_1;
    let _e61 = pc.ndcToViewMul;
    let _e64 = pc.ndcToViewAdd;
    let _e66 = (*vz);
    vxy = (((_e59 * _e61) + _e64) * _e66);
    let _e68 = vxy;
    let _e69 = (*vz);
    return vec3<f32>(_e68.x, _e68.y, _e69);
}

fn normalFromDepth_u0028_vf2_u003b_vf3_u003b_vf2_u003b(uv_3: ptr<function, vec2<f32>>, P_1: ptr<function, vec3<f32>>, texel_1: ptr<function, vec2<f32>>) -> vec3<f32> {
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

    let _e81 = (*uv_3);
    let _e83 = (*texel_1)[0u];
    param_3 = (_e81 - vec2<f32>(_e83, 0f));
    let _e86 = sampleViewDepth0_u0028_vf2_u003b((&param_3));
    zL = _e86;
    let _e87 = (*uv_3);
    let _e89 = (*texel_1)[0u];
    param_4 = (_e87 + vec2<f32>(_e89, 0f));
    let _e92 = sampleViewDepth0_u0028_vf2_u003b((&param_4));
    zR = _e92;
    let _e93 = (*uv_3);
    let _e95 = (*texel_1)[1u];
    param_5 = (_e93 - vec2<f32>(0f, _e95));
    let _e98 = sampleViewDepth0_u0028_vf2_u003b((&param_5));
    zU = _e98;
    let _e99 = (*uv_3);
    let _e101 = (*texel_1)[1u];
    param_6 = (_e99 + vec2<f32>(0f, _e101));
    let _e104 = sampleViewDepth0_u0028_vf2_u003b((&param_6));
    zD = _e104;
    let _e105 = (*uv_3);
    let _e107 = (*texel_1)[0u];
    param_7 = (_e105 - vec2<f32>(_e107, 0f));
    let _e110 = zL;
    param_8 = _e110;
    let _e111 = viewPosFromUV_u0028_vf2_u003b_f1_u003b((&param_7), (&param_8));
    Pl = _e111;
    let _e112 = (*uv_3);
    let _e114 = (*texel_1)[0u];
    param_9 = (_e112 + vec2<f32>(_e114, 0f));
    let _e117 = zR;
    param_10 = _e117;
    let _e118 = viewPosFromUV_u0028_vf2_u003b_f1_u003b((&param_9), (&param_10));
    Pr = _e118;
    let _e119 = (*uv_3);
    let _e121 = (*texel_1)[1u];
    param_11 = (_e119 - vec2<f32>(0f, _e121));
    let _e124 = zU;
    param_12 = _e124;
    let _e125 = viewPosFromUV_u0028_vf2_u003b_f1_u003b((&param_11), (&param_12));
    Pu = _e125;
    let _e126 = (*uv_3);
    let _e128 = (*texel_1)[1u];
    param_13 = (_e126 + vec2<f32>(0f, _e128));
    let _e131 = zD;
    param_14 = _e131;
    let _e132 = viewPosFromUV_u0028_vf2_u003b_f1_u003b((&param_13), (&param_14));
    Pd = _e132;
    let _e134 = Pr[2u];
    let _e136 = (*P_1)[2u];
    let _e140 = (*P_1)[2u];
    let _e142 = Pl[2u];
    if (abs((_e134 - _e136)) < abs((_e140 - _e142))) {
        let _e146 = Pr;
        let _e147 = (*P_1);
        local_1 = (_e146 - _e147);
    } else {
        let _e149 = (*P_1);
        let _e150 = Pl;
        local_1 = (_e149 - _e150);
    }
    let _e152 = local_1;
    dPdx = _e152;
    let _e154 = Pd[2u];
    let _e156 = (*P_1)[2u];
    let _e160 = (*P_1)[2u];
    let _e162 = Pu[2u];
    if (abs((_e154 - _e156)) < abs((_e160 - _e162))) {
        let _e166 = Pd;
        let _e167 = (*P_1);
        local_2 = (_e166 - _e167);
    } else {
        let _e169 = (*P_1);
        let _e170 = Pu;
        local_2 = (_e169 - _e170);
    }
    let _e172 = local_2;
    dPdy = _e172;
    let _e173 = dPdx;
    let _e174 = dPdy;
    n = cross(_e173, _e174);
    let _e176 = n;
    len = length(_e176);
    let _e178 = len;
    if (_e178 > 0.00000001f) {
        let _e180 = n;
        let _e181 = len;
        local_3 = (_e180 / vec3(_e181));
    } else {
        local_3 = vec3<f32>(0f, 0f, 1f);
    }
    let _e184 = local_3;
    return _e184;
}

fn main_1() {
    var pix_1: vec2<i32>;
    var dim: vec2<i32>;
    var texel_2: vec2<f32>;
    var uv_4: vec2<f32>;
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
    var offPixels: f32;
    var mip_1: f32;
    var uv0_: vec2<f32>;
    var uv1_: vec2<f32>;
    var z0_: f32;
    var param_23: vec2<f32>;
    var param_24: f32;
    var z1_: f32;
    var param_25: vec2<f32>;
    var param_26: f32;
    var s0_: vec3<f32>;
    var param_27: vec2<f32>;
    var param_28: f32;
    var s1_: vec3<f32>;
    var param_29: vec2<f32>;
    var param_30: f32;
    var d0_: vec3<f32>;
    var d1_: vec3<f32>;
    var dist0_: f32;
    var dist1_: f32;
    var w0_: f32;
    var w1_: f32;
    var shc0_: f32;
    var shc1_: f32;
    var skyZ: f32;
    var h0_: f32;
    var param_31: f32;
    var h1_: f32;
    var param_32: f32;
    var iarc0_: f32;
    var iarc1_: f32;
    var param_33: vec3<f32>;
    var param_34: vec2<f32>;
    var param_35: f32;
    var phi_476_: bool;

    let _e126 = gl_GlobalInvocationID_1;
    pix_1 = bitcast<vec2<i32>>(_e126.xy);
    let _e130 = pc.viewportSize;
    dim = vec2<i32>(_e130);
    let _e133 = pix_1[0u];
    let _e135 = dim[0u];
    let _e136 = (_e133 >= _e135);
    phi_476_ = _e136;
    if !(_e136) {
        let _e139 = pix_1[1u];
        let _e141 = dim[1u];
        phi_476_ = (_e139 >= _e141);
    }
    let _e144 = phi_476_;
    if _e144 {
        return;
    }
    let _e146 = pc.viewportSize;
    texel_2 = (vec2(1f) / _e146);
    let _e149 = pix_1;
    let _e153 = texel_2;
    uv_4 = ((vec2<f32>(_e149) + vec2(0.5f)) * _e153);
    let _e155 = uv_4;
    param_15 = _e155;
    let _e156 = sampleViewDepth0_u0028_vf2_u003b((&param_15));
    centerZ = _e156;
    let _e157 = centerZ;
    let _e158 = skyDepthThreshold_u0028_();
    if (_e157 >= _e158) {
        let _e160 = pix_1;
        textureStore(aoImage, _e160, vec4<f32>(1f, 1f, 1f, 1f));
        return;
    }
    let _e161 = uv_4;
    param_16 = _e161;
    let _e162 = centerZ;
    param_17 = _e162;
    let _e163 = viewPosFromUV_u0028_vf2_u003b_f1_u003b((&param_16), (&param_17));
    P_2 = _e163;
    let _e164 = uv_4;
    param_18 = _e164;
    let _e165 = P_2;
    param_19 = _e165;
    let _e166 = texel_2;
    param_20 = _e166;
    let _e167 = normalFromDepth_u0028_vf2_u003b_vf3_u003b_vf2_u003b((&param_18), (&param_19), (&param_20));
    N = _e167;
    let _e168 = P_2;
    V = normalize(-(_e168));
    let _e173 = pc.ndcToViewMul[0u];
    tanHalfFovX = (0.5f * abs(_e173));
    let _e177 = pc.effectRadius;
    let _e179 = P_2[2u];
    let _e180 = tanHalfFovX;
    let _e185 = texel_2[0u];
    screenRadiusUV = clamp((_e177 / (_e179 * max(_e180, 0.0001f))), _e185, 0.5f);
    let _e187 = pix_1;
    param_21 = vec2<f32>(_e187);
    let _e189 = ign_u0028_vf2_u003b((&param_21));
    noise_1 = _e189;
    visibility = 0f;
    let _e191 = pc.effectRadius;
    let _e193 = pc.falloffRange;
    falloffMul = (1f / max((_e191 * _e193), 0.0001f));
    let _e198 = pc.effectRadius;
    let _e201 = pc.falloffRange;
    let _e204 = falloffMul;
    falloffAdd = ((-(_e198) * (1f - _e201)) * _e204);
    slice = 0i;
    loop {
        let _e206 = slice;
        let _e208 = pc.sliceCount;
        if (_e206 < _e208) {
            let _e210 = slice;
            let _e212 = noise_1;
            let _e215 = pc.sliceCount;
            sliceK = ((f32(_e210) + _e212) / f32(_e215));
            let _e218 = sliceK;
            phi = (_e218 * 3.1415927f);
            let _e220 = phi;
            let _e222 = phi;
            dir = vec2<f32>(cos(_e220), sin(_e222));
            let _e225 = dir;
            dirVec = vec3<f32>(_e225.x, _e225.y, 0f);
            let _e229 = dirVec;
            let _e230 = dirVec;
            let _e231 = V;
            let _e233 = V;
            orthoDir = (_e229 - (_e233 * dot(_e230, _e231)));
            let _e236 = orthoDir;
            let _e237 = V;
            axis = normalize(cross(_e236, _e237));
            let _e240 = N;
            let _e241 = axis;
            let _e242 = N;
            let _e243 = axis;
            projN = (_e240 - (_e241 * dot(_e242, _e243)));
            let _e247 = projN;
            projLen = length(_e247);
            let _e249 = projLen;
            if (_e249 < 0.00001f) {
                continue;
            }
            let _e251 = orthoDir;
            let _e252 = projN;
            signN = sign(dot(_e251, _e252));
            let _e255 = projN;
            let _e256 = V;
            let _e258 = projLen;
            cosN = clamp((dot(_e255, _e256) / _e258), -1f, 1f);
            let _e261 = signN;
            let _e262 = cosN;
            param_22 = _e262;
            let _e263 = fastAcos_u0028_f1_u003b((&param_22));
            n_1 = (_e261 * _e263);
            let _e265 = n_1;
            horizonCos0_ = cos((_e265 + 1.5707964f));
            let _e268 = n_1;
            horizonCos1_ = cos((_e268 - 1.5707964f));
            let _e272 = texel_2[0u];
            let _e274 = screenRadiusUV;
            minS = ((1.5f * _e272) / max(_e274, 0.00001f));
            step_ = 0i;
            loop {
                let _e277 = step_;
                let _e279 = pc.stepsPerSlice;
                if (_e277 < _e279) {
                    let _e281 = noise_1;
                    let _e282 = slice;
                    let _e284 = pc.stepsPerSlice;
                    let _e286 = step_;
                    stepNoise = fract((_e281 + (f32(((_e282 * _e284) + _e286)) * 0.618034f)));
                    let _e292 = step_;
                    let _e294 = stepNoise;
                    let _e297 = pc.stepsPerSlice;
                    s = ((f32(_e292) + _e294) / f32(_e297));
                    let _e300 = s;
                    s = pow(_e300, 2f);
                    let _e302 = minS;
                    let _e303 = s;
                    s = (_e303 + _e302);
                    let _e305 = dir;
                    let _e306 = s;
                    let _e308 = screenRadiusUV;
                    off = ((_e305 * _e306) * _e308);
                    let _e310 = off;
                    let _e312 = pc.viewportSize;
                    offPixels = length((_e310 * _e312));
                    let _e315 = offPixels;
                    mip_1 = clamp((log2(max(_e315, 1f)) - 3.3f), 0f, 4f);
                    let _e320 = uv_4;
                    let _e321 = off;
                    uv0_ = (_e320 + _e321);
                    let _e323 = uv_4;
                    let _e324 = off;
                    uv1_ = (_e323 - _e324);
                    let _e326 = uv0_;
                    param_23 = _e326;
                    let _e327 = mip_1;
                    param_24 = _e327;
                    let _e328 = sampleViewDepth_u0028_vf2_u003b_f1_u003b((&param_23), (&param_24));
                    z0_ = _e328;
                    let _e329 = uv1_;
                    param_25 = _e329;
                    let _e330 = mip_1;
                    param_26 = _e330;
                    let _e331 = sampleViewDepth_u0028_vf2_u003b_f1_u003b((&param_25), (&param_26));
                    z1_ = _e331;
                    let _e332 = uv0_;
                    param_27 = _e332;
                    let _e333 = z0_;
                    param_28 = _e333;
                    let _e334 = viewPosFromUV_u0028_vf2_u003b_f1_u003b((&param_27), (&param_28));
                    s0_ = _e334;
                    let _e335 = uv1_;
                    param_29 = _e335;
                    let _e336 = z1_;
                    param_30 = _e336;
                    let _e337 = viewPosFromUV_u0028_vf2_u003b_f1_u003b((&param_29), (&param_30));
                    s1_ = _e337;
                    let _e338 = s0_;
                    let _e339 = P_2;
                    d0_ = (_e338 - _e339);
                    let _e341 = s1_;
                    let _e342 = P_2;
                    d1_ = (_e341 - _e342);
                    let _e344 = d0_;
                    dist0_ = length(_e344);
                    let _e346 = d1_;
                    dist1_ = length(_e346);
                    let _e348 = dist0_;
                    let _e349 = falloffMul;
                    let _e351 = falloffAdd;
                    w0_ = clamp(((_e348 * _e349) + _e351), 0f, 1f);
                    let _e354 = dist1_;
                    let _e355 = falloffMul;
                    let _e357 = falloffAdd;
                    w1_ = clamp(((_e354 * _e355) + _e357), 0f, 1f);
                    let _e360 = w0_;
                    w0_ = (1f - _e360);
                    let _e362 = w1_;
                    w1_ = (1f - _e362);
                    let _e364 = d0_;
                    let _e365 = dist0_;
                    let _e369 = V;
                    shc0_ = dot((_e364 / vec3(max(_e365, 0.00001f))), _e369);
                    let _e371 = d1_;
                    let _e372 = dist1_;
                    let _e376 = V;
                    shc1_ = dot((_e371 / vec3(max(_e372, 0.00001f))), _e376);
                    let _e378 = skyDepthThreshold_u0028_();
                    skyZ = _e378;
                    let _e379 = z0_;
                    let _e380 = skyZ;
                    if (_e379 >= _e380) {
                        let _e382 = horizonCos0_;
                        shc0_ = _e382;
                    }
                    let _e383 = z1_;
                    let _e384 = skyZ;
                    if (_e383 >= _e384) {
                        let _e386 = horizonCos1_;
                        shc1_ = _e386;
                    }
                    let _e387 = horizonCos0_;
                    let _e388 = horizonCos0_;
                    let _e389 = shc0_;
                    let _e390 = w0_;
                    horizonCos0_ = max(_e387, mix(_e388, _e389, _e390));
                    let _e393 = horizonCos1_;
                    let _e394 = horizonCos1_;
                    let _e395 = shc1_;
                    let _e396 = w1_;
                    horizonCos1_ = max(_e393, mix(_e394, _e395, _e396));
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e399 = step_;
                    step_ = (_e399 + 1i);
                }
            }
            let _e401 = horizonCos1_;
            param_31 = clamp(_e401, -1f, 1f);
            let _e403 = fastAcos_u0028_f1_u003b((&param_31));
            h0_ = -(_e403);
            let _e405 = horizonCos0_;
            param_32 = clamp(_e405, -1f, 1f);
            let _e407 = fastAcos_u0028_f1_u003b((&param_32));
            h1_ = _e407;
            let _e408 = cosN;
            let _e409 = h0_;
            let _e411 = n_1;
            let _e415 = h0_;
            let _e417 = n_1;
            iarc0_ = (((_e408 + ((2f * _e409) * sin(_e411))) - cos(((2f * _e415) - _e417))) * 0.25f);
            let _e422 = cosN;
            let _e423 = h1_;
            let _e425 = n_1;
            let _e429 = h1_;
            let _e431 = n_1;
            iarc1_ = (((_e422 + ((2f * _e423) * sin(_e425))) - cos(((2f * _e429) - _e431))) * 0.25f);
            let _e436 = projLen;
            let _e437 = iarc0_;
            let _e438 = iarc1_;
            let _e441 = visibility;
            visibility = (_e441 + (_e436 * (_e437 + _e438)));
            continue;
        } else {
            break;
        }
        continuing {
            let _e443 = slice;
            slice = (_e443 + 1i);
        }
    }
    let _e446 = pc.sliceCount;
    let _e448 = visibility;
    visibility = (_e448 / f32(_e446));
    let _e450 = visibility;
    visibility = clamp(_e450, 0f, 1f);
    let _e452 = visibility;
    let _e454 = pc.finalPower;
    visibility = pow(_e452, _e454);
    let _e456 = visibility;
    let _e458 = pc.intensity;
    visibility = mix(1f, _e456, clamp(_e458, 0f, 1f));
    let _e461 = visibility;
    visibility = max(_e461, 0f);
    let _e463 = P_2;
    param_33 = _e463;
    let _e464 = texel_2;
    param_34 = _e464;
    let _e465 = noise_1;
    param_35 = _e465;
    let _e466 = sscsContact_u0028_vf3_u003b_vf2_u003b_f1_u003b((&param_33), (&param_34), (&param_35));
    let _e467 = visibility;
    visibility = (_e467 * _e466);
    let _e469 = pix_1;
    let _e470 = visibility;
    textureStore(aoImage, _e469, vec4(_e470));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
