struct PrefilterPush {
    viewportSize: vec2<f32>,
    zNear: f32,
    zFar: f32,
    effectRadius: f32,
    falloffRange: f32,
}

@group(0) @binding(7)
var<uniform> pc: PrefilterPush;
@group(0) @binding(0)
var depthTex: texture_2d<f32>;
@group(0) @binding(1)
var depthSampler: sampler;
var<private> gl_LocalInvocationID_1: vec3<u32>;
var<private> gl_WorkGroupID_1: vec3<u32>;
@group(0) @binding(2)
var depthMip0_: texture_storage_2d<r16float,write>;
@group(0) @binding(3)
var depthMip1_: texture_storage_2d<r16float,write>;
var<workgroup> s_mip1_: array<array<f32, 8>, 8>;
@group(0) @binding(4)
var depthMip2_: texture_storage_2d<r16float,write>;
var<workgroup> s_mip2_: array<array<f32, 4>, 4>;
@group(0) @binding(5)
var depthMip3_: texture_storage_2d<r16float,write>;
var<workgroup> s_mip3_: array<array<f32, 2>, 2>;
@group(0) @binding(6)
var depthMip4_: texture_storage_2d<r16float,write>;

fn depthMipFilter_u0028_f1_u003b_f1_u003b_f1_u003b_f1_u003b(d0_: ptr<function, f32>, d1_: ptr<function, f32>, d2_: ptr<function, f32>, d3_: ptr<function, f32>) -> f32 {
    var maxDepth: f32;
    var effectRadius: f32;
    var falloffRange: f32;
    var falloffFrom: f32;
    var falloffMul: f32;
    var falloffAdd: f32;
    var w0_: f32;
    var w1_: f32;
    var w2_: f32;
    var w3_: f32;
    var weightSum: f32;

    let _e54 = (*d0_);
    let _e55 = (*d1_);
    let _e57 = (*d2_);
    let _e58 = (*d3_);
    maxDepth = max(max(_e54, _e55), max(_e57, _e58));
    let _e62 = pc.effectRadius;
    effectRadius = (0.75f * _e62);
    let _e65 = pc.falloffRange;
    let _e66 = effectRadius;
    falloffRange = (_e65 * _e66);
    let _e68 = effectRadius;
    let _e70 = pc.falloffRange;
    falloffFrom = (_e68 * (1f - _e70));
    let _e73 = falloffRange;
    falloffMul = (-1f / max(_e73, 0.0001f));
    let _e76 = falloffFrom;
    let _e77 = falloffRange;
    falloffAdd = ((_e76 / max(_e77, 0.0001f)) + 1f);
    let _e81 = maxDepth;
    let _e82 = (*d0_);
    let _e84 = falloffMul;
    let _e86 = falloffAdd;
    w0_ = clamp((((_e81 - _e82) * _e84) + _e86), 0f, 1f);
    let _e89 = maxDepth;
    let _e90 = (*d1_);
    let _e92 = falloffMul;
    let _e94 = falloffAdd;
    w1_ = clamp((((_e89 - _e90) * _e92) + _e94), 0f, 1f);
    let _e97 = maxDepth;
    let _e98 = (*d2_);
    let _e100 = falloffMul;
    let _e102 = falloffAdd;
    w2_ = clamp((((_e97 - _e98) * _e100) + _e102), 0f, 1f);
    let _e105 = maxDepth;
    let _e106 = (*d3_);
    let _e108 = falloffMul;
    let _e110 = falloffAdd;
    w3_ = clamp((((_e105 - _e106) * _e108) + _e110), 0f, 1f);
    let _e113 = w0_;
    let _e114 = w1_;
    let _e116 = w2_;
    let _e118 = w3_;
    weightSum = (((_e113 + _e114) + _e116) + _e118);
    let _e120 = w0_;
    let _e121 = (*d0_);
    let _e123 = w1_;
    let _e124 = (*d1_);
    let _e127 = w2_;
    let _e128 = (*d2_);
    let _e131 = w3_;
    let _e132 = (*d3_);
    let _e135 = weightSum;
    return (((((_e120 * _e121) + (_e123 * _e124)) + (_e127 * _e128)) + (_e131 * _e132)) / _e135);
}

fn linearizeDepth_u0028_f1_u003b(z: ptr<function, f32>) -> f32 {
    var vz: f32;

    let _e42 = pc.zNear;
    let _e44 = pc.zFar;
    let _e47 = pc.zNear;
    let _e48 = (*z);
    let _e50 = pc.zFar;
    let _e52 = pc.zNear;
    vz = ((_e42 * _e44) / (_e47 + (_e48 * (_e50 - _e52))));
    let _e57 = vz;
    return min(_e57, 65000f);
}

fn fetchDeviceDepth_u0028_vi2_u003b(p: ptr<function, vec2<i32>>) -> f32 {
    var dim: vec2<i32>;

    let _e42 = pc.viewportSize;
    dim = vec2<i32>(_e42);
    let _e44 = (*p);
    let _e45 = dim;
    (*p) = clamp(_e44, vec2<i32>(0i, 0i), (_e45 - vec2(1i)));
    let _e49 = (*p);
    let _e50 = textureLoad(depthTex, _e49, 0i);
    return _e50.x;
}

fn main_1() {
    var dim_1: vec2<i32>;
    var local: vec2<i32>;
    var base: vec2<i32>;
    var d00_: f32;
    var param: vec2<i32>;
    var param_1: f32;
    var d10_: f32;
    var param_2: vec2<i32>;
    var param_3: f32;
    var d01_: f32;
    var param_4: vec2<i32>;
    var param_5: f32;
    var d11_: f32;
    var param_6: vec2<i32>;
    var param_7: f32;
    var m1_: f32;
    var param_8: f32;
    var param_9: f32;
    var param_10: f32;
    var param_11: f32;
    var p1_: vec2<i32>;
    var q: vec2<i32>;
    var m2_: f32;
    var param_12: f32;
    var param_13: f32;
    var param_14: f32;
    var param_15: f32;
    var p2_: vec2<i32>;
    var q_1: vec2<i32>;
    var m3_: f32;
    var param_16: f32;
    var param_17: f32;
    var param_18: f32;
    var param_19: f32;
    var p3_: vec2<i32>;
    var m4_: f32;
    var param_20: f32;
    var param_21: f32;
    var param_22: f32;
    var param_23: f32;
    var p4_: vec2<i32>;
    var phi_259_: bool;
    var phi_283_: bool;
    var phi_304_: bool;
    var phi_326_: bool;
    var phi_369_: bool;
    var phi_399_: bool;
    var phi_464_: bool;
    var phi_491_: bool;
    var phi_557_: bool;
    var phi_583_: bool;
    var phi_621_: bool;

    let _e81 = pc.viewportSize;
    dim_1 = vec2<i32>(_e81);
    let _e83 = gl_LocalInvocationID_1;
    local = bitcast<vec2<i32>>(_e83.xy);
    let _e86 = gl_WorkGroupID_1;
    let _e91 = local;
    base = ((bitcast<vec2<i32>>(_e86.xy) * vec2(16i)) + (_e91 * vec2(2i)));
    let _e95 = base;
    param = (_e95 + vec2<i32>(0i, 0i));
    let _e97 = fetchDeviceDepth_u0028_vi2_u003b((&param));
    param_1 = _e97;
    let _e98 = linearizeDepth_u0028_f1_u003b((&param_1));
    d00_ = _e98;
    let _e99 = base;
    param_2 = (_e99 + vec2<i32>(1i, 0i));
    let _e101 = fetchDeviceDepth_u0028_vi2_u003b((&param_2));
    param_3 = _e101;
    let _e102 = linearizeDepth_u0028_f1_u003b((&param_3));
    d10_ = _e102;
    let _e103 = base;
    param_4 = (_e103 + vec2<i32>(0i, 1i));
    let _e105 = fetchDeviceDepth_u0028_vi2_u003b((&param_4));
    param_5 = _e105;
    let _e106 = linearizeDepth_u0028_f1_u003b((&param_5));
    d01_ = _e106;
    let _e107 = base;
    param_6 = (_e107 + vec2<i32>(1i, 1i));
    let _e109 = fetchDeviceDepth_u0028_vi2_u003b((&param_6));
    param_7 = _e109;
    let _e110 = linearizeDepth_u0028_f1_u003b((&param_7));
    d11_ = _e110;
    let _e112 = base[0u];
    let _e114 = dim_1[0u];
    let _e115 = (_e112 < _e114);
    phi_259_ = _e115;
    if _e115 {
        let _e117 = base[1u];
        let _e119 = dim_1[1u];
        phi_259_ = (_e117 < _e119);
    }
    let _e122 = phi_259_;
    if _e122 {
        let _e123 = base;
        let _e125 = d00_;
        textureStore(depthMip0_, (_e123 + vec2<i32>(0i, 0i)), vec4(_e125));
    }
    let _e128 = base[0u];
    let _e131 = dim_1[0u];
    let _e132 = ((_e128 + 1i) < _e131);
    phi_283_ = _e132;
    if _e132 {
        let _e134 = base[1u];
        let _e136 = dim_1[1u];
        phi_283_ = (_e134 < _e136);
    }
    let _e139 = phi_283_;
    if _e139 {
        let _e140 = base;
        let _e142 = d10_;
        textureStore(depthMip0_, (_e140 + vec2<i32>(1i, 0i)), vec4(_e142));
    }
    let _e145 = base[0u];
    let _e147 = dim_1[0u];
    let _e148 = (_e145 < _e147);
    phi_304_ = _e148;
    if _e148 {
        let _e150 = base[1u];
        let _e153 = dim_1[1u];
        phi_304_ = ((_e150 + 1i) < _e153);
    }
    let _e156 = phi_304_;
    if _e156 {
        let _e157 = base;
        let _e159 = d01_;
        textureStore(depthMip0_, (_e157 + vec2<i32>(0i, 1i)), vec4(_e159));
    }
    let _e162 = base[0u];
    let _e165 = dim_1[0u];
    let _e166 = ((_e162 + 1i) < _e165);
    phi_326_ = _e166;
    if _e166 {
        let _e168 = base[1u];
        let _e171 = dim_1[1u];
        phi_326_ = ((_e168 + 1i) < _e171);
    }
    let _e174 = phi_326_;
    if _e174 {
        let _e175 = base;
        let _e177 = d11_;
        textureStore(depthMip0_, (_e175 + vec2<i32>(1i, 1i)), vec4(_e177));
    }
    let _e179 = d00_;
    param_8 = _e179;
    let _e180 = d10_;
    param_9 = _e180;
    let _e181 = d01_;
    param_10 = _e181;
    let _e182 = d11_;
    param_11 = _e182;
    let _e183 = depthMipFilter_u0028_f1_u003b_f1_u003b_f1_u003b_f1_u003b((&param_8), (&param_9), (&param_10), (&param_11));
    m1_ = _e183;
    let _e184 = gl_WorkGroupID_1;
    let _e189 = local;
    p1_ = ((bitcast<vec2<i32>>(_e184.xy) * vec2(8i)) + _e189);
    let _e192 = p1_[0u];
    let _e194 = dim_1[0u];
    let _e197 = (_e192 < ((_e194 + 1i) / 2i));
    phi_369_ = _e197;
    if _e197 {
        let _e199 = p1_[1u];
        let _e201 = dim_1[1u];
        phi_369_ = (_e199 < ((_e201 + 1i) / 2i));
    }
    let _e206 = phi_369_;
    if _e206 {
        let _e207 = p1_;
        let _e208 = m1_;
        textureStore(depthMip1_, _e207, vec4(_e208));
    }
    let _e211 = local[1u];
    let _e213 = local[0u];
    let _e214 = m1_;
    s_mip1_[_e211][_e213] = _e214;
    workgroupBarrier();
    let _e218 = local[0u];
    let _e219 = (_e218 < 4i);
    phi_399_ = _e219;
    if _e219 {
        let _e221 = local[1u];
        phi_399_ = (_e221 < 4i);
    }
    let _e224 = phi_399_;
    if _e224 {
        let _e225 = local;
        q = (_e225 * vec2(2i));
        let _e229 = q[1u];
        let _e231 = q[0u];
        let _e233 = q[1u];
        let _e235 = q[0u];
        let _e238 = q[1u];
        let _e241 = q[0u];
        let _e243 = q[1u];
        let _e246 = q[0u];
        let _e250 = s_mip1_[_e229][_e231];
        param_12 = _e250;
        let _e253 = s_mip1_[_e233][(_e235 + 1i)];
        param_13 = _e253;
        let _e256 = s_mip1_[(_e238 + 1i)][_e241];
        param_14 = _e256;
        let _e259 = s_mip1_[(_e243 + 1i)][(_e246 + 1i)];
        param_15 = _e259;
        let _e260 = depthMipFilter_u0028_f1_u003b_f1_u003b_f1_u003b_f1_u003b((&param_12), (&param_13), (&param_14), (&param_15));
        m2_ = _e260;
        let _e261 = gl_WorkGroupID_1;
        let _e266 = local;
        p2_ = ((bitcast<vec2<i32>>(_e261.xy) * vec2(4i)) + _e266);
        let _e269 = p2_[0u];
        let _e271 = dim_1[0u];
        let _e274 = (_e269 < ((_e271 + 3i) / 4i));
        phi_464_ = _e274;
        if _e274 {
            let _e276 = p2_[1u];
            let _e278 = dim_1[1u];
            phi_464_ = (_e276 < ((_e278 + 3i) / 4i));
        }
        let _e283 = phi_464_;
        if _e283 {
            let _e284 = p2_;
            let _e285 = m2_;
            textureStore(depthMip2_, _e284, vec4(_e285));
        }
        let _e288 = local[1u];
        let _e290 = local[0u];
        let _e291 = m2_;
        s_mip2_[_e288][_e290] = _e291;
    }
    workgroupBarrier();
    let _e295 = local[0u];
    let _e296 = (_e295 < 2i);
    phi_491_ = _e296;
    if _e296 {
        let _e298 = local[1u];
        phi_491_ = (_e298 < 2i);
    }
    let _e301 = phi_491_;
    if _e301 {
        let _e302 = local;
        q_1 = (_e302 * vec2(2i));
        let _e306 = q_1[1u];
        let _e308 = q_1[0u];
        let _e310 = q_1[1u];
        let _e312 = q_1[0u];
        let _e315 = q_1[1u];
        let _e318 = q_1[0u];
        let _e320 = q_1[1u];
        let _e323 = q_1[0u];
        let _e327 = s_mip2_[_e306][_e308];
        param_16 = _e327;
        let _e330 = s_mip2_[_e310][(_e312 + 1i)];
        param_17 = _e330;
        let _e333 = s_mip2_[(_e315 + 1i)][_e318];
        param_18 = _e333;
        let _e336 = s_mip2_[(_e320 + 1i)][(_e323 + 1i)];
        param_19 = _e336;
        let _e337 = depthMipFilter_u0028_f1_u003b_f1_u003b_f1_u003b_f1_u003b((&param_16), (&param_17), (&param_18), (&param_19));
        m3_ = _e337;
        let _e338 = gl_WorkGroupID_1;
        let _e343 = local;
        p3_ = ((bitcast<vec2<i32>>(_e338.xy) * vec2(2i)) + _e343);
        let _e346 = p3_[0u];
        let _e348 = dim_1[0u];
        let _e351 = (_e346 < ((_e348 + 7i) / 8i));
        phi_557_ = _e351;
        if _e351 {
            let _e353 = p3_[1u];
            let _e355 = dim_1[1u];
            phi_557_ = (_e353 < ((_e355 + 7i) / 8i));
        }
        let _e360 = phi_557_;
        if _e360 {
            let _e361 = p3_;
            let _e362 = m3_;
            textureStore(depthMip3_, _e361, vec4(_e362));
        }
        let _e365 = local[1u];
        let _e367 = local[0u];
        let _e368 = m3_;
        s_mip3_[_e365][_e367] = _e368;
    }
    workgroupBarrier();
    let _e372 = local[0u];
    let _e373 = (_e372 == 0i);
    phi_583_ = _e373;
    if _e373 {
        let _e375 = local[1u];
        phi_583_ = (_e375 == 0i);
    }
    let _e378 = phi_583_;
    if _e378 {
        let _e381 = s_mip3_[0i][0i];
        param_20 = _e381;
        let _e384 = s_mip3_[0i][1i];
        param_21 = _e384;
        let _e387 = s_mip3_[1i][0i];
        param_22 = _e387;
        let _e390 = s_mip3_[1i][1i];
        param_23 = _e390;
        let _e391 = depthMipFilter_u0028_f1_u003b_f1_u003b_f1_u003b_f1_u003b((&param_20), (&param_21), (&param_22), (&param_23));
        m4_ = _e391;
        let _e392 = gl_WorkGroupID_1;
        p4_ = bitcast<vec2<i32>>(_e392.xy);
        let _e396 = p4_[0u];
        let _e398 = dim_1[0u];
        let _e401 = (_e396 < ((_e398 + 15i) / 16i));
        phi_621_ = _e401;
        if _e401 {
            let _e403 = p4_[1u];
            let _e405 = dim_1[1u];
            phi_621_ = (_e403 < ((_e405 + 15i) / 16i));
        }
        let _e410 = phi_621_;
        if _e410 {
            let _e411 = p4_;
            let _e412 = m4_;
            textureStore(depthMip4_, _e411, vec4(_e412));
        }
    }
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(local_invocation_id) gl_LocalInvocationID: vec3<u32>, @builtin(workgroup_id) gl_WorkGroupID: vec3<u32>) {
    gl_LocalInvocationID_1 = gl_LocalInvocationID;
    gl_WorkGroupID_1 = gl_WorkGroupID;
    main_1();
}
