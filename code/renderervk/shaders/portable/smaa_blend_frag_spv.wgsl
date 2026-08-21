struct RtMetrics {
    rtMetrics: vec4<f32>,
}

@id(1) override SMAA_MAX_SEARCH_STEPS_DIAG: i32 = 8i;
override override_type_14_: i32 = (SMAA_MAX_SEARCH_STEPS_DIAG - 1i);
override override_type_14_1: i32 = (SMAA_MAX_SEARCH_STEPS_DIAG - 1i);
@id(2) override SMAA_CORNER_ROUNDING: i32 = 25i;
override override_type: bool = (SMAA_MAX_SEARCH_STEPS_DIAG > 0i);
@id(0) override SMAA_MAX_SEARCH_STEPS: i32 = 16i;

@group(3) @binding(0) 
var<uniform> unnamed: RtMetrics;
@group(0) @binding(0) 
var edgesTex: texture_2d<f32>;
@group(0) @binding(32) 
var edgesTex_sampler: sampler;
@group(1) @binding(0) 
var areaTex: texture_2d<f32>;
@group(1) @binding(32) 
var areaTex_sampler: sampler;
@group(2) @binding(0) 
var searchTex: texture_2d<f32>;
@group(2) @binding(32) 
var searchTex_sampler: sampler;
var<private> texcoord_1: vec2<f32>;
var<private> offset0_1: vec4<f32>;
var<private> offset2_1: vec4<f32>;
var<private> offset1_1: vec4<f32>;
var<private> pixcoord_1: vec2<f32>;
var<private> out_weights: vec4<f32>;

fn SMAADetectVerticalCornerPattern_u0028_vf2_u003b_vf4_u003b_vf2_u003b(weights: ptr<function, vec2<f32>>, tc: ptr<function, vec4<f32>>, d: ptr<function, vec2<f32>>) {
    var leftRight: vec2<f32>;
    var rounding: vec2<f32>;
    var factor: vec2<f32>;

    let _e92 = (*d);
    let _e93 = (*d);
    leftRight = step(_e92, _e93.yx);
    let _e99 = leftRight;
    rounding = (_e99 * (1f - (f32(SMAA_CORNER_ROUNDING) / 100f)));
    let _e102 = leftRight[0u];
    let _e104 = leftRight[1u];
    let _e106 = rounding;
    rounding = (_e106 / vec2((_e102 + _e104)));
    factor = vec2<f32>(1f, 1f);
    let _e110 = rounding[0u];
    let _e111 = (*tc);
    let _e113 = textureSampleLevel(edgesTex, edgesTex_sampler, _e111.xy, 0f, vec2<i32>(1i, 0i));
    let _e117 = factor[0u];
    factor[0u] = (_e117 - (_e110 * _e113.y));
    let _e121 = rounding[1u];
    let _e122 = (*tc);
    let _e124 = textureSampleLevel(edgesTex, edgesTex_sampler, _e122.zw, 0f, vec2<i32>(1i, 1i));
    let _e128 = factor[0u];
    factor[0u] = (_e128 - (_e121 * _e124.y));
    let _e132 = rounding[0u];
    let _e133 = (*tc);
    let _e135 = textureSampleLevel(edgesTex, edgesTex_sampler, _e133.xy, 0f, vec2<i32>(-2i, 0i));
    let _e139 = factor[1u];
    factor[1u] = (_e139 - (_e132 * _e135.y));
    let _e143 = rounding[1u];
    let _e144 = (*tc);
    let _e146 = textureSampleLevel(edgesTex, edgesTex_sampler, _e144.zw, 0f, vec2<i32>(-2i, 1i));
    let _e150 = factor[1u];
    factor[1u] = (_e150 - (_e143 * _e146.y));
    let _e153 = factor;
    let _e157 = (*weights);
    (*weights) = (_e157 * clamp(_e153, vec2(0f), vec2(1f)));
    return;
}

fn SMAASearchLength_u0028_vf2_u003b_f1_u003b(e: ptr<function, vec2<f32>>, offs: ptr<function, f32>) -> f32 {
    var scale: vec2<f32>;
    var bias: vec2<f32>;

    scale = vec2<f32>(33f, -33f);
    let _e90 = (*offs);
    bias = (vec2<f32>(66f, 33f) * vec2<f32>(_e90, 1f));
    let _e93 = scale;
    scale = (_e93 + vec2<f32>(-1f, 1f));
    let _e95 = bias;
    bias = (_e95 + vec2<f32>(0.5f, -0.5f));
    let _e97 = scale;
    scale = (_e97 * vec2<f32>(0.015625f, 0.0625f));
    let _e99 = bias;
    bias = (_e99 * vec2<f32>(0.015625f, 0.0625f));
    let _e101 = scale;
    let _e102 = (*e);
    let _e104 = bias;
    let _e106 = textureSampleLevel(searchTex, searchTex_sampler, ((_e101 * _e102) + _e104), 0f);
    return _e106.x;
}

fn SMAASearchYDown_u0028_vf2_u003b_f1_u003b(tc_1: ptr<function, vec2<f32>>, end: ptr<function, f32>) -> f32 {
    var e_1: vec2<f32>;
    var offs_1: f32;
    var param: vec2<f32>;
    var param_1: f32;
    var phi_774_: bool;
    var phi_780_: bool;

    e_1 = vec2<f32>(1f, 0f);
    loop {
        let _e93 = (*tc_1)[1u];
        let _e94 = (*end);
        let _e95 = (_e93 < _e94);
        phi_774_ = _e95;
        if _e95 {
            let _e97 = e_1[0u];
            phi_774_ = (_e97 > 0.8281f);
        }
        let _e100 = phi_774_;
        phi_780_ = _e100;
        if _e100 {
            let _e102 = e_1[1u];
            phi_780_ = (_e102 == 0f);
        }
        let _e105 = phi_780_;
        if _e105 {
            let _e106 = (*tc_1);
            let _e107 = textureSampleLevel(edgesTex, edgesTex_sampler, _e106, 0f);
            e_1 = _e107.xy;
            let _e110 = unnamed.rtMetrics;
            let _e113 = (*tc_1);
            (*tc_1) = (_e113 + (vec2<f32>(0f, 2f) * _e110.xy));
            continue;
        } else {
            break;
        }
    }
    let _e115 = e_1;
    param = _e115.yx;
    param_1 = 0.5f;
    let _e117 = SMAASearchLength_u0028_vf2_u003b_f1_u003b((&param), (&param_1));
    offs_1 = ((-2.007874f * _e117) + 3.25f);
    let _e122 = unnamed.rtMetrics[1u];
    let _e124 = offs_1;
    let _e127 = (*tc_1)[1u];
    return ((-(_e122) * _e124) + _e127);
}

fn SMAASearchYUp_u0028_vf2_u003b_f1_u003b(tc_2: ptr<function, vec2<f32>>, end_1: ptr<function, f32>) -> f32 {
    var e_2: vec2<f32>;
    var offs_2: f32;
    var param_2: vec2<f32>;
    var param_3: f32;
    var phi_724_: bool;
    var phi_730_: bool;

    e_2 = vec2<f32>(1f, 0f);
    loop {
        let _e93 = (*tc_2)[1u];
        let _e94 = (*end_1);
        let _e95 = (_e93 > _e94);
        phi_724_ = _e95;
        if _e95 {
            let _e97 = e_2[0u];
            phi_724_ = (_e97 > 0.8281f);
        }
        let _e100 = phi_724_;
        phi_730_ = _e100;
        if _e100 {
            let _e102 = e_2[1u];
            phi_730_ = (_e102 == 0f);
        }
        let _e105 = phi_730_;
        if _e105 {
            let _e106 = (*tc_2);
            let _e107 = textureSampleLevel(edgesTex, edgesTex_sampler, _e106, 0f);
            e_2 = _e107.xy;
            let _e110 = unnamed.rtMetrics;
            let _e113 = (*tc_2);
            (*tc_2) = (_e113 + (vec2<f32>(0f, -2f) * _e110.xy));
            continue;
        } else {
            break;
        }
    }
    let _e115 = e_2;
    param_2 = _e115.yx;
    param_3 = 0f;
    let _e117 = SMAASearchLength_u0028_vf2_u003b_f1_u003b((&param_2), (&param_3));
    offs_2 = ((-2.007874f * _e117) + 3.25f);
    let _e122 = unnamed.rtMetrics[1u];
    let _e123 = offs_2;
    let _e126 = (*tc_2)[1u];
    return ((_e122 * _e123) + _e126);
}

fn SMAADetectHorizontalCornerPattern_u0028_vf2_u003b_vf4_u003b_vf2_u003b(weights_1: ptr<function, vec2<f32>>, tc_3: ptr<function, vec4<f32>>, d_1: ptr<function, vec2<f32>>) {
    var leftRight_1: vec2<f32>;
    var rounding_1: vec2<f32>;
    var factor_1: vec2<f32>;

    let _e92 = (*d_1);
    let _e93 = (*d_1);
    leftRight_1 = step(_e92, _e93.yx);
    let _e99 = leftRight_1;
    rounding_1 = (_e99 * (1f - (f32(SMAA_CORNER_ROUNDING) / 100f)));
    let _e102 = leftRight_1[0u];
    let _e104 = leftRight_1[1u];
    let _e106 = rounding_1;
    rounding_1 = (_e106 / vec2((_e102 + _e104)));
    factor_1 = vec2<f32>(1f, 1f);
    let _e110 = rounding_1[0u];
    let _e111 = (*tc_3);
    let _e113 = textureSampleLevel(edgesTex, edgesTex_sampler, _e111.xy, 0f, vec2<i32>(0i, 1i));
    let _e117 = factor_1[0u];
    factor_1[0u] = (_e117 - (_e110 * _e113.x));
    let _e121 = rounding_1[1u];
    let _e122 = (*tc_3);
    let _e124 = textureSampleLevel(edgesTex, edgesTex_sampler, _e122.zw, 0f, vec2<i32>(1i, 1i));
    let _e128 = factor_1[0u];
    factor_1[0u] = (_e128 - (_e121 * _e124.x));
    let _e132 = rounding_1[0u];
    let _e133 = (*tc_3);
    let _e135 = textureSampleLevel(edgesTex, edgesTex_sampler, _e133.xy, 0f, vec2<i32>(0i, -2i));
    let _e139 = factor_1[1u];
    factor_1[1u] = (_e139 - (_e132 * _e135.x));
    let _e143 = rounding_1[1u];
    let _e144 = (*tc_3);
    let _e146 = textureSampleLevel(edgesTex, edgesTex_sampler, _e144.zw, 0f, vec2<i32>(1i, -2i));
    let _e150 = factor_1[1u];
    factor_1[1u] = (_e150 - (_e143 * _e146.x));
    let _e153 = factor_1;
    let _e157 = (*weights_1);
    (*weights_1) = (_e157 * clamp(_e153, vec2(0f), vec2(1f)));
    return;
}

fn SMAAArea_u0028_vf2_u003b_f1_u003b_f1_u003b_f1_u003b(dist: ptr<function, vec2<f32>>, e1_: ptr<function, f32>, e2_: ptr<function, f32>, offs_3: ptr<function, f32>) -> vec2<f32> {
    var tc_4: vec2<f32>;

    let _e91 = (*e1_);
    let _e92 = (*e2_);
    let _e97 = (*dist);
    tc_4 = ((vec2<f32>(16f, 16f) * round((vec2<f32>(_e91, _e92) * 4f))) + _e97);
    let _e99 = tc_4;
    tc_4 = ((vec2<f32>(0.00625f, 0.0017857143f) * _e99) + vec2<f32>(0.003125f, 0.00089285715f));
    let _e102 = (*offs_3);
    let _e105 = tc_4[1u];
    tc_4[1u] = ((0.14285715f * _e102) + _e105);
    let _e108 = tc_4;
    let _e109 = textureSampleLevel(areaTex, areaTex_sampler, _e108, 0f);
    return _e109.xy;
}

fn SMAASearchXRight_u0028_vf2_u003b_f1_u003b(tc_5: ptr<function, vec2<f32>>, end_2: ptr<function, f32>) -> f32 {
    var e_3: vec2<f32>;
    var offs_4: f32;
    var param_4: vec2<f32>;
    var param_5: f32;
    var phi_673_: bool;
    var phi_679_: bool;

    e_3 = vec2<f32>(0f, 1f);
    loop {
        let _e93 = (*tc_5)[0u];
        let _e94 = (*end_2);
        let _e95 = (_e93 < _e94);
        phi_673_ = _e95;
        if _e95 {
            let _e97 = e_3[1u];
            phi_673_ = (_e97 > 0.8281f);
        }
        let _e100 = phi_673_;
        phi_679_ = _e100;
        if _e100 {
            let _e102 = e_3[0u];
            phi_679_ = (_e102 == 0f);
        }
        let _e105 = phi_679_;
        if _e105 {
            let _e106 = (*tc_5);
            let _e107 = textureSampleLevel(edgesTex, edgesTex_sampler, _e106, 0f);
            e_3 = _e107.xy;
            let _e110 = unnamed.rtMetrics;
            let _e113 = (*tc_5);
            (*tc_5) = (_e113 + (vec2<f32>(2f, 0f) * _e110.xy));
            continue;
        } else {
            break;
        }
    }
    let _e115 = e_3;
    param_4 = _e115;
    param_5 = 0.5f;
    let _e116 = SMAASearchLength_u0028_vf2_u003b_f1_u003b((&param_4), (&param_5));
    offs_4 = ((-2.007874f * _e116) + 3.25f);
    let _e121 = unnamed.rtMetrics[0u];
    let _e123 = offs_4;
    let _e126 = (*tc_5)[0u];
    return ((-(_e121) * _e123) + _e126);
}

fn SMAASearchXLeft_u0028_vf2_u003b_f1_u003b(tc_6: ptr<function, vec2<f32>>, end_3: ptr<function, f32>) -> f32 {
    var e_4: vec2<f32>;
    var offs_5: f32;
    var param_6: vec2<f32>;
    var param_7: f32;
    var phi_621_: bool;
    var phi_627_: bool;

    e_4 = vec2<f32>(0f, 1f);
    loop {
        let _e93 = (*tc_6)[0u];
        let _e94 = (*end_3);
        let _e95 = (_e93 > _e94);
        phi_621_ = _e95;
        if _e95 {
            let _e97 = e_4[1u];
            phi_621_ = (_e97 > 0.8281f);
        }
        let _e100 = phi_621_;
        phi_627_ = _e100;
        if _e100 {
            let _e102 = e_4[0u];
            phi_627_ = (_e102 == 0f);
        }
        let _e105 = phi_627_;
        if _e105 {
            let _e106 = (*tc_6);
            let _e107 = textureSampleLevel(edgesTex, edgesTex_sampler, _e106, 0f);
            e_4 = _e107.xy;
            let _e110 = unnamed.rtMetrics;
            let _e113 = (*tc_6);
            (*tc_6) = (_e113 + (vec2<f32>(-2f, 0f) * _e110.xy));
            continue;
        } else {
            break;
        }
    }
    let _e115 = e_4;
    param_6 = _e115;
    param_7 = 0f;
    let _e116 = SMAASearchLength_u0028_vf2_u003b_f1_u003b((&param_6), (&param_7));
    offs_5 = ((-2.007874f * _e116) + 3.25f);
    let _e121 = unnamed.rtMetrics[0u];
    let _e122 = offs_5;
    let _e125 = (*tc_6)[0u];
    return ((_e121 * _e122) + _e125);
}

fn SMAADecodeDiagBilinearAccess_u0028_vf2_u003b(e_5: ptr<function, vec2<f32>>) -> vec2<f32> {
    let _e88 = (*e_5)[0u];
    let _e90 = (*e_5)[0u];
    (*e_5)[0u] = (_e88 * abs(((5f * _e90) - 3.75f)));
    let _e96 = (*e_5);
    return round(_e96);
}

fn SMAASearchDiag2_u0028_vf2_u003b_vf2_u003b_vf2_u003b(tc_7: ptr<function, vec2<f32>>, dir: ptr<function, vec2<f32>>, e_6: ptr<function, vec2<f32>>) -> vec2<f32> {
    var coord: vec4<f32>;
    var t: vec3<f32>;
    var param_8: vec2<f32>;
    var phi_253_: bool;

    let _e92 = (*tc_7);
    coord = vec4<f32>(_e92.x, _e92.y, -1f, 1f);
    let _e98 = unnamed.rtMetrics[0u];
    let _e101 = coord[0u];
    coord[0u] = (_e101 + (0.25f * _e98));
    let _e105 = unnamed.rtMetrics;
    let _e106 = _e105.xy;
    t = vec3<f32>(_e106.x, _e106.y, 1f);
    loop {
        let _e111 = coord[2u];
        let _e113 = (_e111 < f32(override_type_14_1));
        phi_253_ = _e113;
        if _e113 {
            let _e115 = coord[3u];
            phi_253_ = (_e115 > 0.9f);
        }
        let _e118 = phi_253_;
        if _e118 {
            let _e119 = t;
            let _e120 = (*dir);
            let _e125 = coord;
            let _e127 = ((_e119 * vec3<f32>(_e120.x, _e120.y, 1f)) + _e125.xyz);
            coord[0u] = _e127.x;
            coord[1u] = _e127.y;
            coord[2u] = _e127.z;
            let _e134 = coord;
            let _e136 = textureSampleLevel(edgesTex, edgesTex_sampler, _e134.xy, 0f);
            (*e_6) = _e136.xy;
            let _e138 = (*e_6);
            param_8 = _e138;
            let _e139 = SMAADecodeDiagBilinearAccess_u0028_vf2_u003b((&param_8));
            (*e_6) = _e139;
            let _e140 = (*e_6);
            coord[3u] = dot(_e140, vec2<f32>(0.5f, 0.5f));
            continue;
        } else {
            break;
        }
    }
    let _e143 = coord;
    return _e143.zw;
}

fn SMAAAreaDiag_u0028_vf2_u003b_vf2_u003b_f1_u003b(dist_1: ptr<function, vec2<f32>>, e_7: ptr<function, vec2<f32>>, offs_6: ptr<function, f32>) -> vec2<f32> {
    var tc_8: vec2<f32>;

    let _e90 = (*e_7);
    let _e92 = (*dist_1);
    tc_8 = ((vec2<f32>(20f, 20f) * _e90) + _e92);
    let _e94 = tc_8;
    tc_8 = ((vec2<f32>(0.00625f, 0.0017857143f) * _e94) + vec2<f32>(0.003125f, 0.00089285715f));
    let _e98 = tc_8[0u];
    tc_8[0u] = (_e98 + 0.5f);
    let _e101 = (*offs_6);
    let _e104 = tc_8[1u];
    tc_8[1u] = (_e104 + (0.14285715f * _e101));
    let _e107 = tc_8;
    let _e108 = textureSampleLevel(areaTex, areaTex_sampler, _e107, 0f);
    return _e108.xy;
}

fn SMAAMovc_u0028_vb2_u003b_vf2_u003b_vf2_u003b(cond: ptr<function, vec2<bool>>, variable: ptr<function, vec2<f32>>, value: ptr<function, vec2<f32>>) {
    let _e90 = (*cond)[0u];
    if _e90 {
        let _e92 = (*value)[0u];
        (*variable)[0u] = _e92;
    }
    let _e95 = (*cond)[1u];
    if _e95 {
        let _e97 = (*value)[1u];
        (*variable)[1u] = _e97;
    }
    return;
}

fn SMAADecodeDiagBilinearAccess_u0028_vf4_u003b(e_8: ptr<function, vec4<f32>>) -> vec4<f32> {
    let _e87 = (*e_8);
    let _e89 = (*e_8);
    let _e95 = (_e87.xz * abs(((_e89.xz * 5f) - vec2(3.75f))));
    (*e_8)[0u] = _e95.x;
    (*e_8)[2u] = _e95.y;
    let _e100 = (*e_8);
    return round(_e100);
}

fn SMAASearchDiag1_u0028_vf2_u003b_vf2_u003b_vf2_u003b(tc_9: ptr<function, vec2<f32>>, dir_1: ptr<function, vec2<f32>>, e_9: ptr<function, vec2<f32>>) -> vec2<f32> {
    var coord_1: vec4<f32>;
    var t_1: vec3<f32>;
    var phi_182_: bool;

    let _e91 = (*tc_9);
    coord_1 = vec4<f32>(_e91.x, _e91.y, -1f, 1f);
    let _e96 = unnamed.rtMetrics;
    let _e97 = _e96.xy;
    t_1 = vec3<f32>(_e97.x, _e97.y, 1f);
    loop {
        let _e102 = coord_1[2u];
        let _e104 = (_e102 < f32(override_type_14_));
        phi_182_ = _e104;
        if _e104 {
            let _e106 = coord_1[3u];
            phi_182_ = (_e106 > 0.9f);
        }
        let _e109 = phi_182_;
        if _e109 {
            let _e110 = t_1;
            let _e111 = (*dir_1);
            let _e116 = coord_1;
            let _e118 = ((_e110 * vec3<f32>(_e111.x, _e111.y, 1f)) + _e116.xyz);
            coord_1[0u] = _e118.x;
            coord_1[1u] = _e118.y;
            coord_1[2u] = _e118.z;
            let _e125 = coord_1;
            let _e127 = textureSampleLevel(edgesTex, edgesTex_sampler, _e125.xy, 0f);
            (*e_9) = _e127.xy;
            let _e129 = (*e_9);
            coord_1[3u] = dot(_e129, vec2<f32>(0.5f, 0.5f));
            continue;
        } else {
            break;
        }
    }
    let _e132 = coord_1;
    return _e132.zw;
}

fn SMAACalculateDiagWeights_u0028_vf2_u003b_vf2_u003b(tc_10: ptr<function, vec2<f32>>, e_10: ptr<function, vec2<f32>>) -> vec2<f32> {
    var weights_2: vec2<f32>;
    var d_2: vec4<f32>;
    var end_4: vec2<f32>;
    var param_9: vec2<f32>;
    var param_10: vec2<f32>;
    var param_11: vec2<f32>;
    var param_12: vec2<f32>;
    var param_13: vec2<f32>;
    var param_14: vec2<f32>;
    var coords: vec4<f32>;
    var c: vec4<f32>;
    var param_15: vec4<f32>;
    var cc: vec2<f32>;
    var param_16: vec2<bool>;
    var param_17: vec2<f32>;
    var param_18: vec2<f32>;
    var param_19: vec2<f32>;
    var param_20: vec2<f32>;
    var param_21: f32;
    var param_22: vec2<f32>;
    var param_23: vec2<f32>;
    var param_24: vec2<f32>;
    var param_25: vec2<f32>;
    var param_26: vec2<f32>;
    var param_27: vec2<f32>;
    var coords_1: vec4<f32>;
    var c_1: vec4<f32>;
    var cc_1: vec2<f32>;
    var param_28: vec2<bool>;
    var param_29: vec2<f32>;
    var param_30: vec2<f32>;
    var param_31: vec2<f32>;
    var param_32: vec2<f32>;
    var param_33: f32;

    weights_2 = vec2<f32>(0f, 0f);
    let _e123 = (*e_10)[0u];
    if (_e123 > 0f) {
        let _e125 = (*tc_10);
        param_9 = _e125;
        param_10 = vec2<f32>(-1f, 1f);
        let _e126 = SMAASearchDiag1_u0028_vf2_u003b_vf2_u003b_vf2_u003b((&param_9), (&param_10), (&param_11));
        let _e127 = param_11;
        end_4 = _e127;
        d_2[0u] = _e126.x;
        d_2[2u] = _e126.y;
        let _e133 = end_4[1u];
        let _e137 = d_2[0u];
        d_2[0u] = (_e137 + select(0f, 1f, (_e133 > 0.9f)));
    } else {
        d_2[0u] = vec2<f32>(0f, 0f).x;
        d_2[2u] = vec2<f32>(0f, 0f).y;
    }
    let _e144 = (*tc_10);
    param_12 = _e144;
    param_13 = vec2<f32>(1f, -1f);
    let _e145 = SMAASearchDiag1_u0028_vf2_u003b_vf2_u003b_vf2_u003b((&param_12), (&param_13), (&param_14));
    let _e146 = param_14;
    end_4 = _e146;
    d_2[1u] = _e145.x;
    d_2[3u] = _e145.y;
    let _e152 = d_2[0u];
    let _e154 = d_2[1u];
    if ((_e152 + _e154) > 2f) {
        let _e158 = d_2[0u];
        let _e162 = d_2[0u];
        let _e164 = d_2[1u];
        let _e166 = d_2[1u];
        let _e171 = unnamed.rtMetrics;
        let _e174 = (*tc_10);
        coords = ((vec4<f32>((-(_e158) + 0.25f), _e162, _e164, (-(_e166) - 0.25f)) * _e171.xyxy) + _e174.xyxy);
        let _e177 = coords;
        let _e179 = textureSampleLevel(edgesTex, edgesTex_sampler, _e177.xy, 0f, vec2<i32>(-1i, 0i));
        let _e180 = _e179.xy;
        c[0u] = _e180.x;
        c[1u] = _e180.y;
        let _e185 = coords;
        let _e187 = textureSampleLevel(edgesTex, edgesTex_sampler, _e185.zw, 0f, vec2<i32>(1i, 0i));
        let _e188 = _e187.xy;
        c[2u] = _e188.x;
        c[3u] = _e188.y;
        let _e193 = c;
        param_15 = _e193;
        let _e194 = SMAADecodeDiagBilinearAccess_u0028_vf4_u003b((&param_15));
        c = vec4<f32>(_e194.y, _e194.x, _e194.w, _e194.z);
        let _e201 = c;
        let _e204 = c;
        cc = ((_e201.xz * 2f) + _e204.yw);
        let _e207 = d_2;
        param_16 = (step(vec2(0.9f), _e207.zw) != vec2<f32>(0f, 0f));
        let _e212 = cc;
        param_17 = _e212;
        param_18 = vec2<f32>(0f, 0f);
        SMAAMovc_u0028_vb2_u003b_vf2_u003b_vf2_u003b((&param_16), (&param_17), (&param_18));
        let _e213 = param_17;
        cc = _e213;
        let _e214 = d_2;
        param_19 = _e214.xy;
        let _e216 = cc;
        param_20 = _e216;
        param_21 = 0f;
        let _e217 = SMAAAreaDiag_u0028_vf2_u003b_vf2_u003b_f1_u003b((&param_19), (&param_20), (&param_21));
        let _e218 = weights_2;
        weights_2 = (_e218 + _e217);
    }
    let _e220 = (*tc_10);
    param_22 = _e220;
    param_23 = vec2<f32>(-1f, -1f);
    let _e221 = SMAASearchDiag2_u0028_vf2_u003b_vf2_u003b_vf2_u003b((&param_22), (&param_23), (&param_24));
    let _e222 = param_24;
    end_4 = _e222;
    d_2[0u] = _e221.x;
    d_2[2u] = _e221.y;
    let _e227 = (*tc_10);
    let _e228 = textureSampleLevel(edgesTex, edgesTex_sampler, _e227, 0f, vec2<i32>(1i, 0i));
    if (_e228.x > 0f) {
        let _e231 = (*tc_10);
        param_25 = _e231;
        param_26 = vec2<f32>(1f, 1f);
        let _e232 = SMAASearchDiag2_u0028_vf2_u003b_vf2_u003b_vf2_u003b((&param_25), (&param_26), (&param_27));
        let _e233 = param_27;
        end_4 = _e233;
        d_2[1u] = _e232.x;
        d_2[3u] = _e232.y;
        let _e239 = end_4[1u];
        let _e243 = d_2[1u];
        d_2[1u] = (_e243 + select(0f, 1f, (_e239 > 0.9f)));
    } else {
        d_2[1u] = vec2<f32>(0f, 0f).x;
        d_2[3u] = vec2<f32>(0f, 0f).y;
    }
    let _e251 = d_2[0u];
    let _e253 = d_2[1u];
    if ((_e251 + _e253) > 2f) {
        let _e257 = d_2[0u];
        let _e260 = d_2[0u];
        let _e263 = d_2[1u];
        let _e265 = d_2[1u];
        let _e268 = unnamed.rtMetrics;
        let _e271 = (*tc_10);
        coords_1 = ((vec4<f32>(-(_e257), -(_e260), _e263, _e265) * _e268.xyxy) + _e271.xyxy);
        let _e274 = coords_1;
        let _e276 = textureSampleLevel(edgesTex, edgesTex_sampler, _e274.xy, 0f, vec2<i32>(-1i, 0i));
        c_1[0u] = _e276.y;
        let _e279 = coords_1;
        let _e281 = textureSampleLevel(edgesTex, edgesTex_sampler, _e279.xy, 0f, vec2<i32>(0i, -1i));
        c_1[1u] = _e281.x;
        let _e284 = coords_1;
        let _e286 = textureSampleLevel(edgesTex, edgesTex_sampler, _e284.zw, 0f, vec2<i32>(1i, 0i));
        let _e287 = _e286.yx;
        c_1[2u] = _e287.x;
        c_1[3u] = _e287.y;
        let _e292 = c_1;
        let _e295 = c_1;
        cc_1 = ((_e292.xz * 2f) + _e295.yw);
        let _e298 = d_2;
        param_28 = (step(vec2(0.9f), _e298.zw) != vec2<f32>(0f, 0f));
        let _e303 = cc_1;
        param_29 = _e303;
        param_30 = vec2<f32>(0f, 0f);
        SMAAMovc_u0028_vb2_u003b_vf2_u003b_vf2_u003b((&param_28), (&param_29), (&param_30));
        let _e304 = param_29;
        cc_1 = _e304;
        let _e305 = d_2;
        param_31 = _e305.xy;
        let _e307 = cc_1;
        param_32 = _e307;
        param_33 = 0f;
        let _e308 = SMAAAreaDiag_u0028_vf2_u003b_vf2_u003b_f1_u003b((&param_31), (&param_32), (&param_33));
        let _e310 = weights_2;
        weights_2 = (_e310 + _e308.yx);
    }
    let _e312 = weights_2;
    return _e312;
}

fn main_1() {
    var weights_3: vec4<f32>;
    var e_11: vec2<f32>;
    var skipHorizontal: bool;
    var param_34: vec2<f32>;
    var param_35: vec2<f32>;
    var coords_2: vec3<f32>;
    var param_36: vec2<f32>;
    var param_37: f32;
    var d_3: vec2<f32>;
    var e1_1: f32;
    var param_38: vec2<f32>;
    var param_39: f32;
    var sqrt_d: vec2<f32>;
    var e2_1: f32;
    var param_40: vec2<f32>;
    var param_41: f32;
    var param_42: f32;
    var param_43: f32;
    var param_44: vec2<f32>;
    var param_45: vec4<f32>;
    var param_46: vec2<f32>;
    var coords_3: vec3<f32>;
    var param_47: vec2<f32>;
    var param_48: f32;
    var d_4: vec2<f32>;
    var e1_2: f32;
    var param_49: vec2<f32>;
    var param_50: f32;
    var sqrt_d_1: vec2<f32>;
    var e2_2: f32;
    var param_51: vec2<f32>;
    var param_52: f32;
    var param_53: f32;
    var param_54: f32;
    var param_55: vec2<f32>;
    var param_56: vec4<f32>;
    var param_57: vec2<f32>;

    weights_3 = vec4<f32>(0f, 0f, 0f, 0f);
    let _e123 = texcoord_1;
    let _e124 = textureSample(edgesTex, edgesTex_sampler, _e123);
    e_11 = _e124.xy;
    let _e127 = e_11[1u];
    if (_e127 > 0f) {
        skipHorizontal = false;
        if override_type {
            let _e129 = texcoord_1;
            param_34 = _e129;
            let _e130 = e_11;
            param_35 = _e130;
            let _e131 = SMAACalculateDiagWeights_u0028_vf2_u003b_vf2_u003b((&param_34), (&param_35));
            weights_3[0u] = _e131.x;
            weights_3[1u] = _e131.y;
            let _e137 = weights_3[0u];
            let _e139 = weights_3[1u];
            skipHorizontal = ((_e137 + _e139) != 0f);
            let _e142 = skipHorizontal;
            if _e142 {
                e_11[0u] = 0f;
            }
        }
        let _e144 = skipHorizontal;
        if !(_e144) {
            let _e146 = offset0_1;
            param_36 = _e146.xy;
            let _e149 = offset2_1[0u];
            param_37 = _e149;
            let _e150 = SMAASearchXLeft_u0028_vf2_u003b_f1_u003b((&param_36), (&param_37));
            coords_2[0u] = _e150;
            let _e153 = offset1_1[1u];
            coords_2[1u] = _e153;
            let _e156 = coords_2[0u];
            d_3[0u] = _e156;
            let _e158 = coords_2;
            let _e160 = textureSampleLevel(edgesTex, edgesTex_sampler, _e158.xy, 0f);
            e1_1 = _e160.x;
            let _e162 = offset0_1;
            param_38 = _e162.zw;
            let _e165 = offset2_1[1u];
            param_39 = _e165;
            let _e166 = SMAASearchXRight_u0028_vf2_u003b_f1_u003b((&param_38), (&param_39));
            coords_2[2u] = _e166;
            let _e169 = coords_2[2u];
            d_3[1u] = _e169;
            let _e172 = unnamed.rtMetrics;
            let _e174 = d_3;
            let _e176 = pixcoord_1;
            d_3 = abs(round(((_e172.zz * _e174) - _e176.xx)));
            let _e181 = d_3;
            sqrt_d = sqrt(_e181);
            let _e183 = coords_2;
            let _e185 = textureSampleLevel(edgesTex, edgesTex_sampler, _e183.zy, 0f, vec2<i32>(1i, 0i));
            e2_1 = _e185.x;
            let _e187 = sqrt_d;
            param_40 = _e187;
            let _e188 = e1_1;
            param_41 = _e188;
            let _e189 = e2_1;
            param_42 = _e189;
            param_43 = 0f;
            let _e190 = SMAAArea_u0028_vf2_u003b_f1_u003b_f1_u003b_f1_u003b((&param_40), (&param_41), (&param_42), (&param_43));
            weights_3[0u] = _e190.x;
            weights_3[1u] = _e190.y;
            let _e196 = texcoord_1[1u];
            coords_2[1u] = _e196;
            let _e198 = weights_3;
            param_44 = _e198.xy;
            let _e200 = coords_2;
            param_45 = _e200.xyzy;
            let _e202 = d_3;
            param_46 = _e202;
            SMAADetectHorizontalCornerPattern_u0028_vf2_u003b_vf4_u003b_vf2_u003b((&param_44), (&param_45), (&param_46));
            let _e203 = param_44;
            weights_3[0u] = _e203.x;
            weights_3[1u] = _e203.y;
        }
    }
    let _e209 = e_11[0u];
    if (_e209 > 0f) {
        let _e211 = offset1_1;
        param_47 = _e211.xy;
        let _e214 = offset2_1[2u];
        param_48 = _e214;
        let _e215 = SMAASearchYUp_u0028_vf2_u003b_f1_u003b((&param_47), (&param_48));
        coords_3[1u] = _e215;
        let _e218 = offset0_1[0u];
        coords_3[0u] = _e218;
        let _e221 = coords_3[1u];
        d_4[0u] = _e221;
        let _e223 = coords_3;
        let _e225 = textureSampleLevel(edgesTex, edgesTex_sampler, _e223.xy, 0f);
        e1_2 = _e225.y;
        let _e227 = offset1_1;
        param_49 = _e227.zw;
        let _e230 = offset2_1[3u];
        param_50 = _e230;
        let _e231 = SMAASearchYDown_u0028_vf2_u003b_f1_u003b((&param_49), (&param_50));
        coords_3[2u] = _e231;
        let _e234 = coords_3[2u];
        d_4[1u] = _e234;
        let _e237 = unnamed.rtMetrics;
        let _e239 = d_4;
        let _e241 = pixcoord_1;
        d_4 = abs(round(((_e237.ww * _e239) - _e241.yy)));
        let _e246 = d_4;
        sqrt_d_1 = sqrt(_e246);
        let _e248 = coords_3;
        let _e250 = textureSampleLevel(edgesTex, edgesTex_sampler, _e248.xz, 0f, vec2<i32>(0i, 1i));
        e2_2 = _e250.y;
        let _e252 = sqrt_d_1;
        param_51 = _e252;
        let _e253 = e1_2;
        param_52 = _e253;
        let _e254 = e2_2;
        param_53 = _e254;
        param_54 = 0f;
        let _e255 = SMAAArea_u0028_vf2_u003b_f1_u003b_f1_u003b_f1_u003b((&param_51), (&param_52), (&param_53), (&param_54));
        weights_3[2u] = _e255.x;
        weights_3[3u] = _e255.y;
        let _e261 = texcoord_1[0u];
        coords_3[0u] = _e261;
        let _e263 = weights_3;
        param_55 = _e263.zw;
        let _e265 = coords_3;
        param_56 = _e265.xyxz;
        let _e267 = d_4;
        param_57 = _e267;
        SMAADetectVerticalCornerPattern_u0028_vf2_u003b_vf4_u003b_vf2_u003b((&param_55), (&param_56), (&param_57));
        let _e268 = param_55;
        weights_3[2u] = _e268.x;
        weights_3[3u] = _e268.y;
    }
    let _e273 = weights_3;
    out_weights = _e273;
    return;
}

@fragment 
fn main(@location(0) texcoord: vec2<f32>, @location(2) offset0_: vec4<f32>, @location(4) offset2_: vec4<f32>, @location(3) offset1_: vec4<f32>, @location(1) pixcoord: vec2<f32>) -> @location(0) vec4<f32> {
    texcoord_1 = texcoord;
    offset0_1 = offset0_;
    offset2_1 = offset2_;
    offset1_1 = offset1_;
    pixcoord_1 = pixcoord;
    main_1();
    let _e11 = out_weights;
    return _e11;
}
