enable wgpu_binding_array;

struct DlightShadowParams {
    shadowFaceMVP: array<mat4x4<f32>, 24>,
    shadowLights: array<vec4<f32>, 4>,
    shadowMeta: vec4<f32>,
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
    _pad_worldLightParams: vec4<f32>,
    advancedFogColorDensity: vec4<f32>,
    advancedFogTypeFarEnabled: vec4<f32>,
}

struct Light {
    posRadius: vec4<f32>,
    color: vec4<f32>,
    posRadius2_: vec4<f32>,
}

struct DLightParams {
    dlights: array<Light>,
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
@group(0) @binding(0)
var<uniform> unnamed_1: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(2) @binding(5)
var<storage> unnamed_2: DLightParams;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_tex_coord_1: vec2<f32>;
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

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e63 = unnamed_1.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e63 + 0.5f));
    let _e68 = unnamed_1.advancedFogTypeFarEnabled[2u];
    let _e70 = fogType;
    let _e73 = fogType;
    return (((_e68 > 0.5f) && (_e70 >= 1i)) && (_e73 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e63 = wired_advanced_fog_enabled_u0028_();
    if !(_e63) {
        return 0f;
    }
    let _e66 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e66, 0.000001f));
    let _e71 = unnamed_1.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e71 + 0.5f));
    let _e74 = fogType_1;
    if (_e74 == 1i) {
        let _e78 = unnamed_1.advancedFogTypeFarEnabled[1u];
        if (_e78 <= 0f) {
            return 0f;
        }
        let _e80 = viewDepth;
        let _e83 = unnamed_1.advancedFogTypeFarEnabled[1u];
        return clamp((_e80 / _e83), 0f, 1f);
    }
    let _e88 = unnamed_1.advancedFogColorDensity[3u];
    let _e90 = viewDepth;
    opticalDepth = (max(_e88, 0f) * _e90);
    let _e92 = fogType_1;
    if (_e92 == 2i) {
        let _e94 = opticalDepth;
        return clamp((1f - exp(-(_e94))), 0f, 1f);
    }
    let _e99 = opticalDepth;
    let _e100 = opticalDepth;
    return clamp((1f - exp(-((_e99 * _e100)))), 0f, 1f);
}

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
    var phi_59_: bool;
    var phi_141_: bool;
    var phi_148_: bool;
    var phi_155_: bool;
    var phi_162_: bool;

    let _e73 = (*lightToFrag);
    a = abs(_e73);
    let _e76 = a[0u];
    let _e78 = a[1u];
    let _e79 = (_e76 >= _e78);
    phi_59_ = _e79;
    if _e79 {
        let _e81 = a[0u];
        let _e83 = a[2u];
        phi_59_ = (_e81 >= _e83);
    }
    let _e86 = phi_59_;
    if _e86 {
        let _e88 = (*lightToFrag)[0u];
        face = select(1i, 0i, (_e88 >= 0f));
    } else {
        let _e92 = a[1u];
        let _e94 = a[2u];
        if (_e92 >= _e94) {
            let _e97 = (*lightToFrag)[1u];
            face = select(3i, 2i, (_e97 >= 0f));
        } else {
            let _e101 = (*lightToFrag)[2u];
            face = select(5i, 4i, (_e101 >= 0f));
        }
    }
    let _e104 = (*lightSlot);
    let _e106 = face;
    col = ((_e104 * 6i) + _e106);
    let _e108 = col;
    let _e111 = unnamed.shadowFaceMVP[_e108];
    let _e112 = (*worldPos);
    clip = (_e111 * vec4<f32>(_e112.x, _e112.y, _e112.z, 1f));
    let _e119 = clip[3u];
    if (_e119 <= 0f) {
        return 1f;
    }
    let _e121 = clip;
    let _e124 = clip[3u];
    ndc = (_e121.xyz / vec3(_e124));
    let _e128 = ndc[0u];
    let _e129 = (_e128 < -1f);
    phi_141_ = _e129;
    if !(_e129) {
        let _e132 = ndc[0u];
        phi_141_ = (_e132 > 1f);
    }
    let _e135 = phi_141_;
    phi_148_ = _e135;
    if !(_e135) {
        let _e138 = ndc[1u];
        phi_148_ = (_e138 < -1f);
    }
    let _e141 = phi_148_;
    phi_155_ = _e141;
    if !(_e141) {
        let _e144 = ndc[1u];
        phi_155_ = (_e144 > 1f);
    }
    let _e147 = phi_155_;
    phi_162_ = _e147;
    if !(_e147) {
        let _e150 = ndc[2u];
        phi_162_ = (_e150 > 1f);
    }
    let _e153 = phi_162_;
    if _e153 {
        return 1f;
    }
    let _e154 = ndc;
    faceUV = ((_e154.xy * 0.5f) + vec2(0.5f));
    let _e161 = unnamed.shadowMeta[1u];
    cols = max(_e161, 6f);
    let _e163 = col;
    let _e166 = faceUV[0u];
    let _e168 = cols;
    let _e171 = faceUV[1u];
    atlasUV = vec2<f32>(((f32(_e163) + _e166) / _e168), _e171);
    let _e173 = atlasUV;
    let _e174 = textureSample(dlightShadowAtlas, dlightShadowSampler, _e173);
    storedDepth = _e174.x;
    let _e177 = ndc[2u];
    receiverDepth = _e177;
    let _e178 = receiverDepth;
    let _e179 = (*lightSlot);
    let _e183 = unnamed.shadowLights[_e179][2u];
    let _e185 = storedDepth;
    return select(1f, 0f, ((_e178 - _e183) > _e185));
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
    var phi_439_: bool;

    let _e80 = (*li);
    let _e83 = unnamed_2.dlights[_e80];
    L.posRadius = _e83.posRadius;
    L.color = _e83.color;
    L.posRadius2_ = _e83.posRadius2_;
    let _e91 = L.posRadius;
    let _e93 = (*world_pos_1);
    Lvec = (_e91.xyz - _e93);
    let _e97 = L.color[3u];
    falloff = _e97;
    let _e98 = Lvec;
    let _e99 = Lvec;
    let _e101 = falloff;
    intensFactor = (1f - (dot(_e98, _e99) * _e101));
    let _e104 = intensFactor;
    if (_e104 <= 0f) {
        return vec3<f32>(0f, 0f, 0f);
    }
    let _e107 = L.color;
    let _e109 = intensFactor;
    intens = (_e107.xyz * _e109);
    let _e111 = Lvec;
    nL = normalize(_e111);
    let _e113 = (*Np);
    let _e114 = nL;
    diffuse = max(dot(_e113, _e114), 0f);
    let _e117 = (*Np);
    let _e118 = nL;
    let _e119 = (*nV);
    specFactor = max(dot(_e117, normalize((_e118 + _e119))), 0f);
    let _e124 = specFactor;
    let _e128 = (*base);
    spec = ((vec4((pow(_e124, 10f) * 0.25f)) * _e128) * 0.8f);
    occ = 1f;
    let _e133 = unnamed.shadowMeta[0u];
    numShadowLights = i32((_e133 + 0.5f));
    s = 0i;
    loop {
        let _e136 = s;
        let _e137 = numShadowLights;
        if (_e136 < _e137) {
            let _e139 = s;
            let _e143 = unnamed.shadowLights[_e139][1u];
            let _e144 = (_e143 > 0.5f);
            phi_439_ = _e144;
            if _e144 {
                let _e145 = s;
                let _e149 = unnamed.shadowLights[_e145][0u];
                let _e152 = (*li);
                phi_439_ = (u32((_e149 + 0.5f)) == _e152);
            }
            let _e155 = phi_439_;
            if _e155 {
                let _e156 = Lvec;
                let _e158 = (*world_pos_1);
                param = _e158;
                param_1 = -(_e156);
                let _e159 = s;
                param_2 = _e159;
                let _e160 = dlightShadowOcclusion_u0028_vf3_u003b_vf3_u003b_i1_u003b((&param), (&param_1), (&param_2));
                occ = _e160;
                break;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e161 = s;
            s = (_e161 + 1i);
        }
    }
    let _e163 = (*base);
    let _e165 = diffuse;
    let _e167 = spec;
    let _e170 = intens;
    let _e172 = occ;
    return ((((_e163.xyz * _e165) + _e167.xyz) * _e170) * _e172);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e64 = (*c);
    (*c) = max(_e64, vec3<f32>(0f, 0f, 0f));
    let _e66 = (*c);
    cutoff = (_e66 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e68 = (*c);
    lo = (_e68 / vec3(12.92f));
    let _e71 = (*c);
    hi = pow(((_e71 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e76 = hi;
    let _e77 = lo;
    let _e78 = cutoff;
    return mix(_e76, _e77, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e78));
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
    var phi_645_: bool;

    let _e91 = unnamed_1.packed_indices[0i][0u];
    let _e97 = unnamed_1.packed_indices[0i][0u];
    let _e102 = frag_tex_coord_1;
    let _e103 = textureSample(wired_bindless_images[(_e91 & 4095u)], wired_bindless_samplers[((_e97 >> bitcast<u32>(12i)) & 255u)], _e102);
    base_1 = _e103;
    let _e104 = base_1;
    param_3 = _e104.xyz;
    let _e106 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    base_1[0u] = _e106.x;
    base_1[1u] = _e106.y;
    base_1[2u] = _e106.z;
    let _e114 = gl_FragCoord_1[0u];
    tx = (u32(_e114) / 16u);
    let _e118 = gl_FragCoord_1[1u];
    ty = (u32(_e118) / 16u);
    let _e123 = unnamed_3.tileParams[2u];
    tilesX = u32(_e123);
    let _e127 = unnamed_3.tileParams[3u];
    tilesY = u32(_e127);
    let _e129 = tilesX;
    if (_e129 == 0u) {
        tilesX = 1u;
    }
    let _e131 = tx;
    let _e132 = tilesX;
    if (_e131 >= _e132) {
        let _e134 = tilesX;
        tx = (_e134 - 1u);
    }
    let _e136 = ty;
    let _e137 = tilesY;
    if (_e136 >= _e137) {
        let _e139 = tilesY;
        ty = (_e139 - 1u);
    }
    let _e141 = ty;
    let _e142 = tilesX;
    let _e144 = tx;
    tileBase = (((_e141 * _e142) + _e144) * 33u);
    let _e147 = tileBase;
    let _e150 = unnamed_4.tileLights[_e147];
    count = _e150;
    let _e151 = count;
    if (_e151 > 32u) {
        count = 32u;
    }
    let _e153 = world_normal_1;
    Np_1 = normalize(_e153);
    let _e155 = world_view_1;
    nV_1 = normalize(_e155);
    lit = vec3<f32>(0f, 0f, 0f);
    k = 0u;
    loop {
        let _e157 = k;
        let _e158 = count;
        if (_e157 < _e158) {
            let _e160 = tileBase;
            let _e162 = k;
            let _e166 = unnamed_4.tileLights[((_e160 + 1u) + _e162)];
            param_4 = _e166;
            let _e167 = Np_1;
            param_5 = _e167;
            let _e168 = nV_1;
            param_6 = _e168;
            let _e169 = base_1;
            param_7 = _e169;
            let _e170 = world_pos_2;
            param_8 = _e170;
            let _e171 = fp_light_contrib_u0028_u1_u003b_vf3_u003b_vf3_u003b_vf4_u003b_vf3_u003b((&param_4), (&param_5), (&param_6), (&param_7), (&param_8));
            let _e172 = lit;
            lit = (_e172 + _e171);
            continue;
        } else {
            break;
        }
        continuing {
            let _e174 = k;
            k = (_e174 + bitcast<u32>(1i));
        }
    }
    let _e179 = unnamed_5.gridOrigin[3u];
    cs = _e179;
    let _e180 = cs;
    if (_e180 > 0f) {
        let _e182 = world_pos_2;
        let _e184 = unnamed_5.gridOrigin;
        let _e187 = cs;
        cell = vec3<i32>(floor(((_e182 - _e184.xyz) / vec3(_e187))));
        let _e192 = cell;
        let _e194 = all((_e192 >= vec3<i32>(0i, 0i, 0i)));
        phi_645_ = _e194;
        if _e194 {
            let _e195 = cell;
            let _e197 = unnamed_5.gridDims;
            phi_645_ = all((_e195 < _e197.xyz));
        }
        let _e202 = phi_645_;
        if _e202 {
            let _e204 = cell[2u];
            let _e207 = unnamed_5.gridDims[1u];
            let _e210 = cell[1u];
            let _e214 = unnamed_5.gridDims[0u];
            let _e217 = cell[0u];
            ci = ((((_e204 * _e207) + _e210) * _e214) + _e217);
            let _e219 = ci;
            cBase = (bitcast<u32>(_e219) * 33u);
            let _e222 = cBase;
            let _e225 = unnamed_6.clusterLights[_e222];
            cN = _e225;
            let _e226 = cN;
            if (_e226 > 32u) {
                cN = 32u;
            }
            k_1 = 0u;
            loop {
                let _e228 = k_1;
                let _e229 = cN;
                if (_e228 < _e229) {
                    let _e231 = cBase;
                    let _e233 = k_1;
                    let _e237 = unnamed_6.clusterLights[((_e231 + 1u) + _e233)];
                    param_9 = _e237;
                    let _e238 = Np_1;
                    param_10 = _e238;
                    let _e239 = nV_1;
                    param_11 = _e239;
                    let _e240 = base_1;
                    param_12 = _e240;
                    let _e241 = world_pos_2;
                    param_13 = _e241;
                    let _e242 = fp_light_contrib_u0028_u1_u003b_vf3_u003b_vf3_u003b_vf4_u003b_vf3_u003b((&param_9), (&param_10), (&param_11), (&param_12), (&param_13));
                    let _e243 = lit;
                    lit = (_e243 + _e242);
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e245 = k_1;
                    k_1 = (_e245 + bitcast<u32>(1i));
                }
            }
        }
    }
    let _e248 = wired_advanced_fog_amount_u0028_();
    let _e250 = lit;
    lit = (_e250 * (1f - _e248));
    let _e252 = lit;
    let _e254 = base_1[3u];
    out_color = vec4<f32>(_e252.x, _e252.y, _e252.z, _e254);
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_tex_coord: vec2<f32>, @location(2) world_normal: vec3<f32>, @location(3) world_view: vec3<f32>, @location(1) world_pos: vec3<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord_1 = frag_tex_coord;
    world_normal_1 = world_normal;
    world_view_1 = world_view;
    world_pos_2 = world_pos;
    main_1();
    let _e11 = out_color;
    return _e11;
}
