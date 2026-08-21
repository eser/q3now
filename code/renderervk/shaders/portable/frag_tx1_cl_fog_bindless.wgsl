enable wgpu_binding_array;

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    fogDistanceVector: vec4<f32>,
    fogDepthVector: vec4<f32>,
    fogEyeT: vec4<f32>,
    fogColor: vec4<f32>,
    _pad_to_packed_indices: array<vec4<f32>, 26>,
    packed_indices: array<vec4<u32>, 3>,
    worldLightParams: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_10_: bool = (tex_mode == 1i);
override override_type_10_1: bool = (tex_mode == 2i);
override override_type_10_2: bool = (override_type_10_ || override_type_10_1);
override override_type_10_3: bool = (tex_mode == 3i);
override override_type_10_4: bool = (tex_mode == 4i);
override override_type_10_5: bool = (tex_mode == 5i);
override override_type_10_6: bool = (tex_mode == 6i);
override override_type_10_7: bool = (tex_mode == 7i);
@id(10) override acff: i32 = 0i;
override override_type_10_8: bool = (acff == 1i);
override override_type_10_9: bool = (acff == 2i);
override override_type_10_10: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_10_11: bool = (discard_mode == 1i);
override override_type_10_12: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e63 = (*c);
    (*c) = max(_e63, vec3<f32>(0f, 0f, 0f));
    let _e65 = (*c);
    cutoff = (_e65 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e67 = (*c);
    lo = (_e67 / vec3(12.92f));
    let _e70 = (*c);
    hi = pow(((_e70 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e75 = hi;
    let _e76 = lo;
    let _e77 = cutoff;
    return mix(_e75, _e76, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e77));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e64 = (*role);
    let _e66 = (*role);
    let _e71 = unnamed.packed_indices[(_e64 / 4u)][(_e66 % 4u)];
    let _e74 = (*role);
    let _e76 = (*role);
    let _e81 = unnamed.packed_indices[(_e74 / 4u)][(_e76 % 4u)];
    let _e86 = (*uv);
    let _e87 = textureSample(wired_bindless_images[(_e71 & 4095u)], wired_bindless_samplers[((_e81 >> bitcast<u32>(12i)) & 255u)], _e86);
    c_1 = _e87;
    let _e88 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e88))) == 0i) {
        let _e93 = c_1;
        param = _e93.xyz;
        let _e95 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e95.x;
        c_1[1u] = _e95.y;
        c_1[2u] = _e95.z;
    }
    let _e102 = (*slot);
    if (lightmap_slot == (_e102 + 1i)) {
        let _e107 = unnamed.worldLightParams[0u];
        let _e108 = c_1;
        let _e110 = (_e108.xyz * _e107);
        c_1[0u] = _e110.x;
        c_1[1u] = _e110.y;
        c_1[2u] = _e110.z;
    }
    let _e117 = c_1;
    return _e117;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_2: vec3<f32>;
    var color0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var color1_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var color1_2: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color1_3: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var color1_4: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color1_5: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var color1_6: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;

    let _e100 = unnamed.packed_indices[0i][3u];
    let _e106 = unnamed.packed_indices[0i][3u];
    let _e111 = fog_tex_coord_1;
    let _e112 = textureSample(wired_bindless_images[(_e100 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    fog = _e112;
    let _e113 = frag_color0In_1;
    param_1 = _e113.xyz;
    let _e115 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e117 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e115.x, _e115.y, _e115.z, _e117);
    let _e122 = frag_color1In_1;
    param_2 = _e122.xyz;
    let _e124 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e126 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e124.x, _e124.y, _e124.z, _e126);
    param_3 = 0u;
    let _e131 = frag_tex_coord0_1;
    param_4 = _e131;
    param_5 = 0i;
    let _e132 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e133 = frag_color0_;
    color0_ = (_e132 * _e133);
    if override_type_10_2 {
        param_6 = 1u;
        let _e135 = frag_tex_coord1_1;
        param_7 = _e135;
        param_8 = 1i;
        let _e136 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        let _e137 = frag_color1_;
        color1_ = (_e136 * _e137);
        let _e139 = color0_;
        let _e141 = color1_;
        let _e143 = (_e139.xyz + _e141.xyz);
        let _e145 = color0_[3u];
        let _e147 = color1_[3u];
        base = vec4<f32>(_e143.x, _e143.y, _e143.z, (_e145 * _e147));
    } else {
        if override_type_10_3 {
            param_9 = 1u;
            let _e153 = frag_tex_coord1_1;
            param_10 = _e153;
            param_11 = 1i;
            let _e154 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
            let _e155 = frag_color1_;
            color1_1 = (_e154 * _e155);
            let _e158 = color0_[3u];
            let _e159 = color0_;
            color0_ = (_e159 * _e158);
            let _e162 = color1_1[3u];
            let _e163 = color1_1;
            color1_1 = (_e163 * _e162);
            let _e165 = color0_;
            let _e167 = color1_1;
            let _e169 = (_e165.xyz + _e167.xyz);
            let _e171 = color0_[3u];
            let _e173 = color1_1[3u];
            base = vec4<f32>(_e169.x, _e169.y, _e169.z, (_e171 * _e173));
        } else {
            if override_type_10_4 {
                param_12 = 1u;
                let _e179 = frag_tex_coord1_1;
                param_13 = _e179;
                param_14 = 1i;
                let _e180 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
                let _e181 = frag_color1_;
                color1_2 = (_e180 * _e181);
                let _e184 = color0_[3u];
                let _e186 = color0_;
                color0_ = (_e186 * (1f - _e184));
                let _e189 = color1_2[3u];
                let _e191 = color1_2;
                color1_2 = (_e191 * (1f - _e189));
                let _e193 = color0_;
                let _e195 = color1_2;
                let _e197 = (_e193.xyz + _e195.xyz);
                let _e199 = color0_[3u];
                let _e201 = color1_2[3u];
                base = vec4<f32>(_e197.x, _e197.y, _e197.z, (_e199 * _e201));
            } else {
                if override_type_10_5 {
                    param_15 = 1u;
                    let _e207 = frag_tex_coord1_1;
                    param_16 = _e207;
                    param_17 = 1i;
                    let _e208 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
                    let _e209 = frag_color1_;
                    color1_3 = (_e208 * _e209);
                    let _e211 = color0_;
                    let _e212 = color1_3;
                    let _e214 = color1_3[3u];
                    base = mix(_e211, _e212, vec4(_e214));
                } else {
                    if override_type_10_6 {
                        param_18 = 1u;
                        let _e217 = frag_tex_coord1_1;
                        param_19 = _e217;
                        param_20 = 1i;
                        let _e218 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
                        let _e219 = frag_color1_;
                        color1_4 = (_e218 * _e219);
                        let _e221 = color1_4;
                        let _e222 = color0_;
                        let _e224 = color1_4[3u];
                        base = mix(_e221, _e222, vec4(_e224));
                    } else {
                        if override_type_10_7 {
                            param_21 = 1u;
                            let _e227 = frag_tex_coord1_1;
                            param_22 = _e227;
                            param_23 = 1i;
                            let _e228 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
                            let _e229 = frag_color1_;
                            color1_5 = (_e228 * _e229);
                            let _e231 = color1_5;
                            let _e233 = color1_5[3u];
                            let _e236 = color0_;
                            base = ((_e231 + vec4(_e233)) * _e236);
                        } else {
                            param_24 = 1u;
                            let _e238 = frag_tex_coord1_1;
                            param_25 = _e238;
                            param_26 = 1i;
                            let _e239 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                            let _e240 = frag_color1_;
                            color1_6 = (_e239 * _e240);
                            let _e242 = color0_;
                            let _e244 = color1_6;
                            let _e246 = (_e242.xyz * _e244.xyz);
                            base[0u] = _e246.x;
                            base[1u] = _e246.y;
                            base[2u] = _e246.z;
                            let _e254 = color0_[3u];
                            let _e256 = color1_6[3u];
                            base[3u] = (_e254 * _e256);
                        }
                    }
                }
            }
        }
    }
    if override_type_10_8 {
        let _e259 = base;
        let _e262 = fog[3u];
        let _e264 = (_e259.xyz * (1f - _e262));
        base[0u] = _e264.x;
        base[1u] = _e264.y;
        base[2u] = _e264.z;
    } else {
        if override_type_10_9 {
            let _e271 = base;
            let _e273 = fog[3u];
            base = (_e271 * (1f - _e273));
        } else {
            if override_type_10_10 {
                let _e277 = base[3u];
                let _e279 = fog[3u];
                base[3u] = (_e277 * (1f - _e279));
            } else {
                let _e283 = base;
                let _e284 = fog;
                let _e286 = unnamed.fogColor;
                let _e289 = fog[3u];
                base = mix(_e283, (_e284 * _e286), vec4(_e289));
            }
        }
    }
    if override_type_10_11 {
        let _e293 = base[3u];
        if (_e293 == 0f) {
            discard;
        }
    } else {
        if override_type_10_12 {
            let _e295 = base;
            let _e297 = base;
            if (dot(_e295.xyz, _e297.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e301 = base;
    out_color = _e301;
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e11 = out_color;
    return _e11;
}
