enable wgpu_binding_array;

struct DlightShadowParams {
    shadowFaceMVP: array<mat4x4<f32>, 24>,
    shadowLights: array<vec4<f32>, 4>,
    shadowMeta: vec4<f32>,
}

struct Light {
    posRadius: vec4<f32>,
    color: vec4<f32>,
    posRadius2_: vec4<f32>,
}

struct DLightParams {
    dlights: array<Light>,
}

struct UBO {
    eyePos: vec4<f32>,
    _pad_light: array<vec4<f32>, 3>,
    fogDistanceVector: vec4<f32>,
    fogDepthVector: vec4<f32>,
    fogEyeT: vec4<f32>,
    fogColor: vec4<f32>,
    _pad_to_packed_indices: array<vec4<f32>, 26>,
    packed_indices: array<vec4<u32>, 3>,
}

struct TileParams {
    tileParams: vec4<f32>,
}

struct TileLights {
    tileLights: array<u32>,
}

struct ClusterGridParams {
    gridOrigin: vec4<f32>,
    gridDims: vec4<i32>,
}

struct ClusterLights {
    clusterLights: array<u32>,
}

@group(2) @binding(9) 
var<uniform> unnamed: DlightShadowParams;
@group(2) @binding(7) 
var dlightShadowAtlas: texture_2d<f32>;
@group(2) @binding(8) 
var dlightShadowSampler: sampler;
@group(2) @binding(5) 
var<storage> unnamed_1: DLightParams;
@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed_2: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_tex_coord_1: vec2<f32>;
var<private> gl_FragCoord_1: vec4<f32>;
@group(2) @binding(6) 
var<uniform> unnamed_3: TileParams;
@group(2) @binding(4) 
var<storage> unnamed_4: TileLights;
var<private> world_normal_1: vec3<f32>;
var<private> world_view_1: vec3<f32>;
var<private> world_pos_2: vec3<f32>;
@group(2) @binding(11) 
var<uniform> unnamed_5: ClusterGridParams;
@group(2) @binding(10) 
var<storage> unnamed_6: ClusterLights;
var<private> out_color: vec4<f32>;

fn dlightShadowOcclusion_u0028_vf3_u003b_vf3_u003b_i1_u003b(worldPos: ptr<function, vec3<f32>>, lightToFrag: ptr<function, vec3<f32>>, lightSlot: ptr<function, i32>) -> f32 {
    var a: vec3<f32>;
    var face: i32;
    var col: i32;
    var clip: vec4<f32>;
    var ndc: vec3<f32>;
    var faceUV: vec2<f32>;
    var cols: f32;
    var atlasUV: vec2<f32>;
    var storedDepth: f32;
    var receiverDepth: f32;
    var phi_53_: bool;
    var phi_135_: bool;
    var phi_142_: bool;
    var phi_149_: bool;
    var phi_156_: bool;

    let _e70 = (*lightToFrag);
    a = abs(_e70);
    let _e73 = a[0u];
    let _e75 = a[1u];
    let _e76 = (_e73 >= _e75);
    phi_53_ = _e76;
    if _e76 {
        let _e78 = a[0u];
        let _e80 = a[2u];
        phi_53_ = (_e78 >= _e80);
    }
    let _e83 = phi_53_;
    if _e83 {
        let _e85 = (*lightToFrag)[0u];
        face = select(1i, 0i, (_e85 >= 0f));
    } else {
        let _e89 = a[1u];
        let _e91 = a[2u];
        if (_e89 >= _e91) {
            let _e94 = (*lightToFrag)[1u];
            face = select(3i, 2i, (_e94 >= 0f));
        } else {
            let _e98 = (*lightToFrag)[2u];
            face = select(5i, 4i, (_e98 >= 0f));
        }
    }
    let _e101 = (*lightSlot);
    let _e103 = face;
    col = ((_e101 * 6i) + _e103);
    let _e105 = col;
    let _e108 = unnamed.shadowFaceMVP[_e105];
    let _e109 = (*worldPos);
    clip = (_e108 * vec4<f32>(_e109.x, _e109.y, _e109.z, 1f));
    let _e116 = clip[3u];
    if (_e116 <= 0f) {
        return 1f;
    }
    let _e118 = clip;
    let _e121 = clip[3u];
    ndc = (_e118.xyz / vec3(_e121));
    let _e125 = ndc[0u];
    let _e126 = (_e125 < -1f);
    phi_135_ = _e126;
    if !(_e126) {
        let _e129 = ndc[0u];
        phi_135_ = (_e129 > 1f);
    }
    let _e132 = phi_135_;
    phi_142_ = _e132;
    if !(_e132) {
        let _e135 = ndc[1u];
        phi_142_ = (_e135 < -1f);
    }
    let _e138 = phi_142_;
    phi_149_ = _e138;
    if !(_e138) {
        let _e141 = ndc[1u];
        phi_149_ = (_e141 > 1f);
    }
    let _e144 = phi_149_;
    phi_156_ = _e144;
    if !(_e144) {
        let _e147 = ndc[2u];
        phi_156_ = (_e147 > 1f);
    }
    let _e150 = phi_156_;
    if _e150 {
        return 1f;
    }
    let _e151 = ndc;
    faceUV = ((_e151.xy * 0.5f) + vec2(0.5f));
    let _e158 = unnamed.shadowMeta[1u];
    cols = max(_e158, 6f);
    let _e160 = col;
    let _e163 = faceUV[0u];
    let _e165 = cols;
    let _e168 = faceUV[1u];
    atlasUV = vec2<f32>(((f32(_e160) + _e163) / _e165), _e168);
    let _e170 = atlasUV;
    let _e171 = textureSample(dlightShadowAtlas, dlightShadowSampler, _e170);
    storedDepth = _e171.x;
    let _e174 = ndc[2u];
    receiverDepth = _e174;
    let _e175 = receiverDepth;
    let _e176 = (*lightSlot);
    let _e180 = unnamed.shadowLights[_e176][2u];
    let _e182 = storedDepth;
    return select(1f, 0f, ((_e175 - _e180) > _e182));
}

fn fp_light_contrib_u0028_u1_u003b_vf3_u003b_vf3_u003b_vf4_u003b_vf3_u003b(li: ptr<function, u32>, Np: ptr<function, vec3<f32>>, nV: ptr<function, vec3<f32>>, base: ptr<function, vec4<f32>>, world_pos_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var L: Light;
    var Lvec: vec3<f32>;
    var falloff: f32;
    var intensFactor: f32;
    var intens: vec3<f32>;
    var nL: vec3<f32>;
    var diffuse: f32;
    var specFactor: f32;
    var spec: vec4<f32>;
    var occ: f32;
    var numShadowLights: i32;
    var s: i32;
    var param: vec3<f32>;
    var param_1: vec3<f32>;
    var param_2: i32;
    var phi_347_: bool;

    let _e77 = (*li);
    let _e80 = unnamed_1.dlights[_e77];
    L.posRadius = _e80.posRadius;
    L.color = _e80.color;
    L.posRadius2_ = _e80.posRadius2_;
    let _e88 = L.posRadius;
    let _e90 = (*world_pos_1);
    Lvec = (_e88.xyz - _e90);
    let _e94 = L.color[3u];
    falloff = _e94;
    let _e95 = Lvec;
    let _e96 = Lvec;
    let _e98 = falloff;
    intensFactor = (1f - (dot(_e95, _e96) * _e98));
    let _e101 = intensFactor;
    if (_e101 <= 0f) {
        return vec3<f32>(0f, 0f, 0f);
    }
    let _e104 = L.color;
    let _e106 = intensFactor;
    intens = (_e104.xyz * _e106);
    let _e108 = Lvec;
    nL = normalize(_e108);
    let _e110 = (*Np);
    let _e111 = nL;
    diffuse = max(dot(_e110, _e111), 0f);
    let _e114 = (*Np);
    let _e115 = nL;
    let _e116 = (*nV);
    specFactor = max(dot(_e114, normalize((_e115 + _e116))), 0f);
    let _e121 = specFactor;
    let _e125 = (*base);
    spec = ((vec4((pow(_e121, 10f) * 0.25f)) * _e125) * 0.8f);
    occ = 1f;
    let _e130 = unnamed.shadowMeta[0u];
    numShadowLights = i32((_e130 + 0.5f));
    s = 0i;
    loop {
        let _e133 = s;
        let _e134 = numShadowLights;
        if (_e133 < _e134) {
            let _e136 = s;
            let _e140 = unnamed.shadowLights[_e136][1u];
            let _e141 = (_e140 > 0.5f);
            phi_347_ = _e141;
            if _e141 {
                let _e142 = s;
                let _e146 = unnamed.shadowLights[_e142][0u];
                let _e149 = (*li);
                phi_347_ = (u32((_e146 + 0.5f)) == _e149);
            }
            let _e152 = phi_347_;
            if _e152 {
                let _e153 = Lvec;
                let _e155 = (*world_pos_1);
                param = _e155;
                param_1 = -(_e153);
                let _e156 = s;
                param_2 = _e156;
                let _e157 = dlightShadowOcclusion_u0028_vf3_u003b_vf3_u003b_i1_u003b((&param), (&param_1), (&param_2));
                occ = _e157;
                break;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e158 = s;
            s = (_e158 + 1i);
        }
    }
    let _e160 = (*base);
    let _e162 = diffuse;
    let _e164 = spec;
    let _e167 = intens;
    let _e169 = occ;
    return ((((_e160.xyz * _e162) + _e164.xyz) * _e167) * _e169);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e61 = (*c);
    (*c) = max(_e61, vec3<f32>(0f, 0f, 0f));
    let _e63 = (*c);
    cutoff = (_e63 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e65 = (*c);
    lo = (_e65 / vec3(12.92f));
    let _e68 = (*c);
    hi = pow(((_e68 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e73 = hi;
    let _e74 = lo;
    let _e75 = cutoff;
    return mix(_e73, _e74, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e75));
}

fn main_1() {
    var base_1: vec4<f32>;
    var param_3: vec3<f32>;
    var tx: u32;
    var ty: u32;
    var tilesX: u32;
    var tilesY: u32;
    var tileBase: u32;
    var count: u32;
    var Np_1: vec3<f32>;
    var nV_1: vec3<f32>;
    var lit: vec3<f32>;
    var k: u32;
    var param_4: u32;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;
    var param_7: vec4<f32>;
    var param_8: vec3<f32>;
    var cs: f32;
    var cell: vec3<i32>;
    var ci: i32;
    var cBase: u32;
    var cN: u32;
    var k_1: u32;
    var param_9: u32;
    var param_10: vec3<f32>;
    var param_11: vec3<f32>;
    var param_12: vec4<f32>;
    var param_13: vec3<f32>;
    var phi_564_: bool;

    let _e88 = unnamed_2.packed_indices[0i][0u];
    let _e94 = unnamed_2.packed_indices[0i][0u];
    let _e99 = frag_tex_coord_1;
    let _e100 = textureSample(wired_bindless_images[(_e88 & 4095u)], wired_bindless_samplers[((_e94 >> bitcast<u32>(12i)) & 255u)], _e99);
    base_1 = _e100;
    let _e101 = base_1;
    param_3 = _e101.xyz;
    let _e103 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    base_1[0u] = _e103.x;
    base_1[1u] = _e103.y;
    base_1[2u] = _e103.z;
    let _e111 = gl_FragCoord_1[0u];
    tx = (u32(_e111) / 16u);
    let _e115 = gl_FragCoord_1[1u];
    ty = (u32(_e115) / 16u);
    let _e120 = unnamed_3.tileParams[2u];
    tilesX = u32(_e120);
    let _e124 = unnamed_3.tileParams[3u];
    tilesY = u32(_e124);
    let _e126 = tilesX;
    if (_e126 == 0u) {
        tilesX = 1u;
    }
    let _e128 = tx;
    let _e129 = tilesX;
    if (_e128 >= _e129) {
        let _e131 = tilesX;
        tx = (_e131 - 1u);
    }
    let _e133 = ty;
    let _e134 = tilesY;
    if (_e133 >= _e134) {
        let _e136 = tilesY;
        ty = (_e136 - 1u);
    }
    let _e138 = ty;
    let _e139 = tilesX;
    let _e141 = tx;
    tileBase = (((_e138 * _e139) + _e141) * 33u);
    let _e144 = tileBase;
    let _e147 = unnamed_4.tileLights[_e144];
    count = _e147;
    let _e148 = count;
    if (_e148 > 32u) {
        count = 32u;
    }
    let _e150 = world_normal_1;
    Np_1 = normalize(_e150);
    let _e152 = world_view_1;
    nV_1 = normalize(_e152);
    lit = vec3<f32>(0f, 0f, 0f);
    k = 0u;
    loop {
        let _e154 = k;
        let _e155 = count;
        if (_e154 < _e155) {
            let _e157 = tileBase;
            let _e159 = k;
            let _e163 = unnamed_4.tileLights[((_e157 + 1u) + _e159)];
            param_4 = _e163;
            let _e164 = Np_1;
            param_5 = _e164;
            let _e165 = nV_1;
            param_6 = _e165;
            let _e166 = base_1;
            param_7 = _e166;
            let _e167 = world_pos_2;
            param_8 = _e167;
            let _e168 = fp_light_contrib_u0028_u1_u003b_vf3_u003b_vf3_u003b_vf4_u003b_vf3_u003b((&param_4), (&param_5), (&param_6), (&param_7), (&param_8));
            let _e169 = lit;
            lit = (_e169 + _e168);
            continue;
        } else {
            break;
        }
        continuing {
            let _e171 = k;
            k = (_e171 + bitcast<u32>(1i));
        }
    }
    let _e176 = unnamed_5.gridOrigin[3u];
    cs = _e176;
    let _e177 = cs;
    if (_e177 > 0f) {
        let _e179 = world_pos_2;
        let _e181 = unnamed_5.gridOrigin;
        let _e184 = cs;
        cell = vec3<i32>(floor(((_e179 - _e181.xyz) / vec3(_e184))));
        let _e189 = cell;
        let _e191 = all((_e189 >= vec3<i32>(0i, 0i, 0i)));
        phi_564_ = _e191;
        if _e191 {
            let _e192 = cell;
            let _e194 = unnamed_5.gridDims;
            phi_564_ = all((_e192 < _e194.xyz));
        }
        let _e199 = phi_564_;
        if _e199 {
            let _e201 = cell[2u];
            let _e204 = unnamed_5.gridDims[1u];
            let _e207 = cell[1u];
            let _e211 = unnamed_5.gridDims[0u];
            let _e214 = cell[0u];
            ci = ((((_e201 * _e204) + _e207) * _e211) + _e214);
            let _e216 = ci;
            cBase = (bitcast<u32>(_e216) * 33u);
            let _e219 = cBase;
            let _e222 = unnamed_6.clusterLights[_e219];
            cN = _e222;
            let _e223 = cN;
            if (_e223 > 32u) {
                cN = 32u;
            }
            k_1 = 0u;
            loop {
                let _e225 = k_1;
                let _e226 = cN;
                if (_e225 < _e226) {
                    let _e228 = cBase;
                    let _e230 = k_1;
                    let _e234 = unnamed_6.clusterLights[((_e228 + 1u) + _e230)];
                    param_9 = _e234;
                    let _e235 = Np_1;
                    param_10 = _e235;
                    let _e236 = nV_1;
                    param_11 = _e236;
                    let _e237 = base_1;
                    param_12 = _e237;
                    let _e238 = world_pos_2;
                    param_13 = _e238;
                    let _e239 = fp_light_contrib_u0028_u1_u003b_vf3_u003b_vf3_u003b_vf4_u003b_vf3_u003b((&param_9), (&param_10), (&param_11), (&param_12), (&param_13));
                    let _e240 = lit;
                    lit = (_e240 + _e239);
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e242 = k_1;
                    k_1 = (_e242 + bitcast<u32>(1i));
                }
            }
        }
    }
    let _e245 = lit;
    let _e247 = base_1[3u];
    out_color = vec4<f32>(_e245.x, _e245.y, _e245.z, _e247);
    return;
}

@fragment 
fn main(@location(0) frag_tex_coord: vec2<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(2) world_normal: vec3<f32>, @location(3) world_view: vec3<f32>, @location(1) world_pos: vec3<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    gl_FragCoord_1 = gl_FragCoord;
    world_normal_1 = world_normal;
    world_view_1 = world_view;
    world_pos_2 = world_pos;
    main_1();
    let _e11 = out_color;
    return _e11;
}
