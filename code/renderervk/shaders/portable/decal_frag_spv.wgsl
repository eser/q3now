enable wgpu_binding_array;

struct DecalFrame {
    mvp: mat4x4<f32>,
    timeCount: vec4<f32>,
    invMvp: mat4x4<f32>,
    reconParams: vec4<f32>,
}

var<private> fragUV_1: vec2<f32>;
@group(0) @binding(0) 
var<uniform> unnamed: DecalFrame;
var<private> fragNoProject_1: u32;
var<private> gl_FragCoord_1: vec4<f32>;
@group(0) @binding(3) 
var sceneDepthTex: texture_2d<f32>;
@group(0) @binding(35) 
var sceneDepthTex_sampler: sampler;
var<private> fragDecalOrigin_1: vec3<f32>;
var<private> fragDecalRadius_1: f32;
var<private> fragDecalTangent_1: vec3<f32>;
var<private> fragDecalBitangent_1: vec3<f32>;
var<private> fragDecalNormal_1: vec3<f32>;
@group(0) @binding(2) 
var decalTextures: binding_array<texture_2d<f32>, 64>;
@group(0) @binding(34) 
var decalTextures_sampler: binding_array<sampler, 64>;
var<private> fragTextureIndex_1: u32;
var<private> fragColor_1: vec4<f32>;
var<private> outColor: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e40 = (*c);
    (*c) = max(_e40, vec3<f32>(0f, 0f, 0f));
    let _e42 = (*c);
    cutoff = (_e42 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e44 = (*c);
    lo = (_e44 / vec3(12.92f));
    let _e47 = (*c);
    hi = pow(((_e47 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e52 = hi;
    let _e53 = lo;
    let _e54 = cutoff;
    return mix(_e52, _e53, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e54));
}

fn main_1() {
    var decalUV: vec2<f32>;
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var ndc: vec3<f32>;
    var worldH: vec4<f32>;
    var worldPos: vec3<f32>;
    var local: vec3<f32>;
    var invR: f32;
    var local_1: f32;
    var u: f32;
    var v: f32;
    var w: f32;
    var halfThickness: f32;
    var texel: vec4<f32>;
    var rgb: vec3<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;
    var alpha: f32;
    var phi_77_: bool;
    var phi_184_: bool;
    var phi_192_: bool;

    let _e54 = fragUV_1;
    decalUV = _e54;
    let _e57 = unnamed.reconParams[2u];
    let _e58 = (_e57 > 0.5f);
    phi_77_ = _e58;
    if _e58 {
        let _e59 = fragNoProject_1;
        phi_77_ = (_e59 == 0u);
    }
    let _e62 = phi_77_;
    if _e62 {
        let _e63 = gl_FragCoord_1;
        let _e66 = unnamed.reconParams;
        screenUV = (_e63.xy * _e66.xy);
        let _e69 = screenUV;
        let _e70 = textureSample(sceneDepthTex, sceneDepthTex_sampler, _e69);
        sceneDepth = _e70.x;
        let _e72 = sceneDepth;
        if (_e72 <= 0f) {
            discard;
        }
        let _e74 = screenUV;
        let _e77 = ((_e74 * 2f) - vec2(1f));
        let _e78 = sceneDepth;
        ndc = vec3<f32>(_e77.x, _e77.y, _e78);
        let _e83 = unnamed.invMvp;
        let _e84 = ndc;
        worldH = (_e83 * vec4<f32>(_e84.x, _e84.y, _e84.z, 1f));
        let _e90 = worldH;
        let _e93 = worldH[3u];
        worldPos = (_e90.xyz / vec3(_e93));
        let _e96 = worldPos;
        let _e97 = fragDecalOrigin_1;
        local = (_e96 - _e97);
        let _e99 = fragDecalRadius_1;
        if (_e99 > 0f) {
            let _e101 = fragDecalRadius_1;
            local_1 = (1f / _e101);
        } else {
            local_1 = 0f;
        }
        let _e103 = local_1;
        invR = _e103;
        let _e104 = local;
        let _e105 = fragDecalTangent_1;
        let _e107 = invR;
        u = (dot(_e104, _e105) * _e107);
        let _e109 = local;
        let _e110 = fragDecalBitangent_1;
        let _e112 = invR;
        v = (dot(_e109, _e110) * _e112);
        let _e114 = local;
        let _e115 = fragDecalNormal_1;
        w = dot(_e114, _e115);
        let _e117 = fragDecalRadius_1;
        halfThickness = (_e117 * 0.5f);
        let _e119 = u;
        let _e121 = (abs(_e119) > 1f);
        phi_184_ = _e121;
        if !(_e121) {
            let _e123 = v;
            phi_184_ = (abs(_e123) > 1f);
        }
        let _e127 = phi_184_;
        phi_192_ = _e127;
        if !(_e127) {
            let _e129 = w;
            let _e131 = halfThickness;
            phi_192_ = (abs(_e129) > _e131);
        }
        let _e134 = phi_192_;
        if _e134 {
            discard;
        }
        let _e135 = u;
        let _e136 = v;
        decalUV = ((vec2<f32>(_e135, _e136) * 0.5f) + vec2(0.5f));
    }
    let _e141 = fragTextureIndex_1;
    let _e144 = decalUV;
    let _e145 = textureSample(decalTextures[_e141], decalTextures_sampler[_e141], _e144);
    texel = _e145;
    let _e146 = texel;
    param = _e146.xyz;
    let _e148 = sRGBToLinear_u0028_vf3_u003b((&param));
    let _e149 = fragColor_1;
    param_1 = _e149.xyz;
    let _e151 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    rgb = (_e148 * _e151);
    let _e154 = texel[3u];
    let _e156 = fragColor_1[3u];
    alpha = (_e154 * _e156);
    let _e158 = rgb;
    let _e159 = alpha;
    outColor = vec4<f32>(_e158.x, _e158.y, _e158.z, _e159);
    return;
}

@fragment 
fn main(@location(0) fragUV: vec2<f32>, @location(8) @interpolate(flat) fragNoProject: u32, @builtin(position) gl_FragCoord: vec4<f32>, @location(3) @interpolate(flat) fragDecalOrigin: vec3<f32>, @location(7) @interpolate(flat) fragDecalRadius: f32, @location(4) @interpolate(flat) fragDecalTangent: vec3<f32>, @location(5) @interpolate(flat) fragDecalBitangent: vec3<f32>, @location(6) @interpolate(flat) fragDecalNormal: vec3<f32>, @location(2) @interpolate(flat) fragTextureIndex: u32, @location(1) fragColor: vec4<f32>) -> @location(0) vec4<f32> {
    fragUV_1 = fragUV;
    fragNoProject_1 = fragNoProject;
    gl_FragCoord_1 = gl_FragCoord;
    fragDecalOrigin_1 = fragDecalOrigin;
    fragDecalRadius_1 = fragDecalRadius;
    fragDecalTangent_1 = fragDecalTangent;
    fragDecalBitangent_1 = fragDecalBitangent;
    fragDecalNormal_1 = fragDecalNormal;
    fragTextureIndex_1 = fragTextureIndex;
    fragColor_1 = fragColor;
    main_1();
    let _e21 = outColor;
    return _e21;
}
