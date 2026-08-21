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

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_3_: bool = (tex_mode == 1i);
override override_type_3_1: bool = (tex_mode == 2i);
override override_type_3_2: bool = (override_type_3_ || override_type_3_1);
override override_type_3_3: bool = (tex_mode == 3i);
override override_type_3_4: bool = (tex_mode == 4i);
override override_type_3_5: bool = (tex_mode == 5i);
override override_type_3_6: bool = (tex_mode == 6i);
override override_type_3_7: bool = (tex_mode == 7i);
@id(10) override acff: i32 = 0i;
override override_type_3_8: bool = (acff == 1i);
override override_type_3_9: bool = (acff == 2i);
override override_type_3_10: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_11: bool = (discard_mode == 1i);
override override_type_3_12: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
var<private> temporalOutcome_1: u32;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_66_: bool;

    let _e74 = (*value);
    let _e75 = (*value);
    let _e77 = all((_e74 == _e75));
    phi_66_ = _e77;
    if _e77 {
        let _e78 = (*value);
        phi_66_ = all((abs(_e78) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e83 = phi_66_;
    return _e83;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_51_: bool;

    let _e74 = (*value_1);
    let _e75 = (*value_1);
    let _e77 = all((_e74 == _e75));
    phi_51_ = _e77;
    if _e77 {
        let _e78 = (*value_1);
        phi_51_ = all((abs(_e78) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e83 = phi_51_;
    return _e83;
}

fn wiredTemporalWriteAux_u0028_() {
    var param: vec4<f32>;
    var param_1: vec4<f32>;
    var currentNdc: vec2<f32>;
    var previousNdc: vec2<f32>;
    var param_2: vec2<f32>;
    var param_3: vec2<f32>;
    var currentUv: vec2<f32>;
    var previousUv: vec2<f32>;
    var velocity: vec2<f32>;
    var param_4: vec2<f32>;
    var phi_89_: bool;
    var phi_98_: bool;
    var phi_108_: bool;
    var phi_115_: bool;
    var phi_144_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e83 = temporalOutcome_1;
    let _e84 = (_e83 != 1u);
    phi_89_ = _e84;
    if !(_e84) {
        let _e86 = temporalCurrentClip_1;
        param = _e86;
        let _e87 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_89_ = !(_e87);
    }
    let _e90 = phi_89_;
    phi_98_ = _e90;
    if !(_e90) {
        let _e92 = temporalPreviousClip_1;
        param_1 = _e92;
        let _e93 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_98_ = !(_e93);
    }
    let _e96 = phi_98_;
    phi_108_ = _e96;
    if !(_e96) {
        let _e99 = temporalCurrentClip_1[3u];
        phi_108_ = (_e99 <= 0.000001f);
    }
    let _e102 = phi_108_;
    phi_115_ = _e102;
    if !(_e102) {
        let _e105 = temporalPreviousClip_1[3u];
        phi_115_ = (_e105 <= 0.000001f);
    }
    let _e108 = phi_115_;
    if _e108 {
        return;
    }
    let _e109 = temporalCurrentClip_1;
    let _e112 = temporalCurrentClip_1[3u];
    currentNdc = (_e109.xy / vec2(_e112));
    let _e115 = temporalPreviousClip_1;
    let _e118 = temporalPreviousClip_1[3u];
    previousNdc = (_e115.xy / vec2(_e118));
    let _e121 = currentNdc;
    param_2 = _e121;
    let _e122 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e123 = !(_e122);
    phi_144_ = _e123;
    if !(_e123) {
        let _e125 = previousNdc;
        param_3 = _e125;
        let _e126 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_144_ = !(_e126);
    }
    let _e129 = phi_144_;
    if _e129 {
        return;
    }
    let _e130 = currentNdc;
    currentUv = ((_e130 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e133 = previousNdc;
    previousUv = ((_e133 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e136 = currentUv;
    let _e137 = previousUv;
    velocity = (_e136 - _e137);
    let _e139 = velocity;
    param_4 = _e139;
    let _e140 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e140) {
        return;
    }
    let _e142 = velocity;
    out_temporal_velocity = _e142;
    out_temporal_validity = 1f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e77 = (*c);
    (*c) = max(_e77, vec3<f32>(0f, 0f, 0f));
    let _e79 = (*c);
    cutoff = (_e79 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e81 = (*c);
    lo = (_e81 / vec3(12.92f));
    let _e84 = (*c);
    hi = pow(((_e84 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e89 = hi;
    let _e90 = lo;
    let _e91 = cutoff;
    return mix(_e89, _e90, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e91));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e78 = (*role);
    let _e80 = (*role);
    let _e85 = unnamed.packed_indices[(_e78 / 4u)][(_e80 % 4u)];
    let _e88 = (*role);
    let _e90 = (*role);
    let _e95 = unnamed.packed_indices[(_e88 / 4u)][(_e90 % 4u)];
    let _e100 = (*uv);
    let _e101 = textureSample(wired_bindless_images[(_e85 & 4095u)], wired_bindless_samplers[((_e95 >> bitcast<u32>(12i)) & 255u)], _e100);
    c_1 = _e101;
    let _e102 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e102))) == 0i) {
        let _e107 = c_1;
        param_5 = _e107.xyz;
        let _e109 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e109.x;
        c_1[1u] = _e109.y;
        c_1[2u] = _e109.z;
    }
    let _e116 = (*slot);
    if (lightmap_slot == (_e116 + 1i)) {
        let _e121 = unnamed.worldLightParams[0u];
        let _e122 = c_1;
        let _e124 = (_e122.xyz * _e121);
        c_1[0u] = _e124.x;
        c_1[1u] = _e124.y;
        c_1[2u] = _e124.z;
    }
    let _e131 = c_1;
    return _e131;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_6: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_7: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_8: vec3<f32>;
    var color0_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var color1_: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color2_: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color2_1: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var color1_2: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;
    var color2_2: vec4<f32>;
    var param_27: u32;
    var param_28: vec2<f32>;
    var param_29: i32;
    var color1_3: vec4<f32>;
    var param_30: u32;
    var param_31: vec2<f32>;
    var param_32: i32;
    var color2_3: vec4<f32>;
    var param_33: u32;
    var param_34: vec2<f32>;
    var param_35: i32;
    var color1_4: vec4<f32>;
    var param_36: u32;
    var param_37: vec2<f32>;
    var param_38: i32;
    var color2_4: vec4<f32>;
    var param_39: u32;
    var param_40: vec2<f32>;
    var param_41: i32;
    var color1_5: vec4<f32>;
    var param_42: u32;
    var param_43: vec2<f32>;
    var param_44: i32;
    var color2_5: vec4<f32>;
    var param_45: u32;
    var param_46: vec2<f32>;
    var param_47: i32;
    var color1_6: vec4<f32>;
    var param_48: u32;
    var param_49: vec2<f32>;
    var param_50: i32;
    var color2_6: vec4<f32>;
    var param_51: u32;
    var param_52: vec2<f32>;
    var param_53: i32;

    let _e144 = unnamed.packed_indices[0i][3u];
    let _e150 = unnamed.packed_indices[0i][3u];
    let _e155 = fog_tex_coord_1;
    let _e156 = textureSample(wired_bindless_images[(_e144 & 4095u)], wired_bindless_samplers[((_e150 >> bitcast<u32>(12i)) & 255u)], _e155);
    fog = _e156;
    let _e157 = frag_color0In_1;
    param_6 = _e157.xyz;
    let _e159 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e161 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e159.x, _e159.y, _e159.z, _e161);
    let _e166 = frag_color1In_1;
    param_7 = _e166.xyz;
    let _e168 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e170 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e168.x, _e168.y, _e168.z, _e170);
    let _e175 = frag_color2In_1;
    param_8 = _e175.xyz;
    let _e177 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    let _e179 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e177.x, _e177.y, _e177.z, _e179);
    param_9 = 0u;
    let _e184 = frag_tex_coord0_1;
    param_10 = _e184;
    param_11 = 0i;
    let _e185 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    let _e186 = frag_color0_;
    color0_ = (_e185 * _e186);
    if override_type_3_2 {
        param_12 = 1u;
        let _e188 = frag_tex_coord1_1;
        param_13 = _e188;
        param_14 = 1i;
        let _e189 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
        let _e190 = frag_color1_;
        color1_ = (_e189 * _e190);
        param_15 = 2u;
        let _e192 = frag_tex_coord2_1;
        param_16 = _e192;
        param_17 = 2i;
        let _e193 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
        let _e194 = frag_color2_;
        color2_ = (_e193 * _e194);
        let _e196 = color0_;
        let _e198 = color1_;
        let _e201 = color2_;
        let _e203 = ((_e196.xyz + _e198.xyz) + _e201.xyz);
        let _e205 = color0_[3u];
        let _e207 = color1_[3u];
        let _e210 = color2_[3u];
        base = vec4<f32>(_e203.x, _e203.y, _e203.z, ((_e205 * _e207) * _e210));
    } else {
        if override_type_3_3 {
            param_18 = 1u;
            let _e216 = frag_tex_coord1_1;
            param_19 = _e216;
            param_20 = 1i;
            let _e217 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
            let _e218 = frag_color1_;
            color1_1 = (_e217 * _e218);
            param_21 = 2u;
            let _e220 = frag_tex_coord2_1;
            param_22 = _e220;
            param_23 = 2i;
            let _e221 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
            let _e222 = frag_color2_;
            color2_1 = (_e221 * _e222);
            let _e225 = color0_[3u];
            let _e226 = color0_;
            color0_ = (_e226 * _e225);
            let _e229 = color1_1[3u];
            let _e230 = color1_1;
            color1_1 = (_e230 * _e229);
            let _e233 = color2_1[3u];
            let _e234 = color2_1;
            color2_1 = (_e234 * _e233);
            let _e236 = color0_;
            let _e238 = color1_1;
            let _e241 = color2_1;
            let _e243 = ((_e236.xyz + _e238.xyz) + _e241.xyz);
            let _e245 = color0_[3u];
            let _e247 = color1_1[3u];
            let _e250 = color2_1[3u];
            base = vec4<f32>(_e243.x, _e243.y, _e243.z, ((_e245 * _e247) * _e250));
        } else {
            if override_type_3_4 {
                param_24 = 1u;
                let _e256 = frag_tex_coord1_1;
                param_25 = _e256;
                param_26 = 1i;
                let _e257 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                let _e258 = frag_color1_;
                color1_2 = (_e257 * _e258);
                param_27 = 2u;
                let _e260 = frag_tex_coord2_1;
                param_28 = _e260;
                param_29 = 2i;
                let _e261 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_27), (&param_28), (&param_29));
                let _e262 = frag_color2_;
                color2_2 = (_e261 * _e262);
                let _e265 = color0_[3u];
                let _e267 = color0_;
                color0_ = (_e267 * (1f - _e265));
                let _e270 = color1_2[3u];
                let _e272 = color1_2;
                color1_2 = (_e272 * (1f - _e270));
                let _e275 = color2_2[3u];
                let _e277 = color2_2;
                color2_2 = (_e277 * (1f - _e275));
                let _e279 = color0_;
                let _e281 = color1_2;
                let _e284 = color2_2;
                let _e286 = ((_e279.xyz + _e281.xyz) + _e284.xyz);
                let _e288 = color0_[3u];
                let _e290 = color1_2[3u];
                let _e293 = color2_2[3u];
                base = vec4<f32>(_e286.x, _e286.y, _e286.z, ((_e288 * _e290) * _e293));
            } else {
                if override_type_3_5 {
                    param_30 = 1u;
                    let _e299 = frag_tex_coord1_1;
                    param_31 = _e299;
                    param_32 = 1i;
                    let _e300 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_30), (&param_31), (&param_32));
                    let _e301 = frag_color1_;
                    color1_3 = (_e300 * _e301);
                    param_33 = 2u;
                    let _e303 = frag_tex_coord2_1;
                    param_34 = _e303;
                    param_35 = 2i;
                    let _e304 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_33), (&param_34), (&param_35));
                    let _e305 = frag_color2_;
                    color2_3 = (_e304 * _e305);
                    let _e307 = color0_;
                    let _e308 = color1_3;
                    let _e310 = color1_3[3u];
                    let _e313 = color2_3;
                    let _e315 = color2_3[3u];
                    base = mix(mix(_e307, _e308, vec4(_e310)), _e313, vec4(_e315));
                } else {
                    if override_type_3_6 {
                        param_36 = 1u;
                        let _e318 = frag_tex_coord1_1;
                        param_37 = _e318;
                        param_38 = 1i;
                        let _e319 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_36), (&param_37), (&param_38));
                        let _e320 = frag_color1_;
                        color1_4 = (_e319 * _e320);
                        param_39 = 2u;
                        let _e322 = frag_tex_coord2_1;
                        param_40 = _e322;
                        param_41 = 2i;
                        let _e323 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_39), (&param_40), (&param_41));
                        let _e324 = frag_color2_;
                        color2_4 = (_e323 * _e324);
                        let _e326 = color2_4;
                        let _e327 = color1_4;
                        let _e328 = color0_;
                        let _e330 = color1_4[3u];
                        let _e334 = color2_4[3u];
                        base = mix(_e326, mix(_e327, _e328, vec4(_e330)), vec4(_e334));
                    } else {
                        if override_type_3_7 {
                            param_42 = 1u;
                            let _e337 = frag_tex_coord1_1;
                            param_43 = _e337;
                            param_44 = 1i;
                            let _e338 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_42), (&param_43), (&param_44));
                            let _e339 = frag_color1_;
                            color1_5 = (_e338 * _e339);
                            param_45 = 2u;
                            let _e341 = frag_tex_coord2_1;
                            param_46 = _e341;
                            param_47 = 2i;
                            let _e342 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_45), (&param_46), (&param_47));
                            let _e343 = frag_color2_;
                            color2_5 = (_e342 * _e343);
                            let _e345 = color2_5;
                            let _e347 = color2_5[3u];
                            let _e350 = color1_5;
                            let _e352 = color1_5[3u];
                            let _e356 = color0_;
                            base = (((_e345 + vec4(_e347)) * (_e350 + vec4(_e352))) * _e356);
                        } else {
                            param_48 = 1u;
                            let _e358 = frag_tex_coord1_1;
                            param_49 = _e358;
                            param_50 = 1i;
                            let _e359 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_48), (&param_49), (&param_50));
                            let _e360 = frag_color1_;
                            color1_6 = (_e359 * _e360);
                            param_51 = 2u;
                            let _e362 = frag_tex_coord2_1;
                            param_52 = _e362;
                            param_53 = 2i;
                            let _e363 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_51), (&param_52), (&param_53));
                            let _e364 = frag_color2_;
                            color2_6 = (_e363 * _e364);
                            let _e366 = color0_;
                            let _e368 = color1_6;
                            let _e371 = color2_6;
                            let _e373 = ((_e366.xyz * _e368.xyz) * _e371.xyz);
                            base[0u] = _e373.x;
                            base[1u] = _e373.y;
                            base[2u] = _e373.z;
                            let _e381 = color0_[3u];
                            let _e383 = color1_6[3u];
                            let _e386 = color2_6[3u];
                            base[3u] = ((_e381 * _e383) * _e386);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e389 = base;
        let _e392 = fog[3u];
        let _e394 = (_e389.xyz * (1f - _e392));
        base[0u] = _e394.x;
        base[1u] = _e394.y;
        base[2u] = _e394.z;
    } else {
        if override_type_3_9 {
            let _e401 = base;
            let _e403 = fog[3u];
            base = (_e401 * (1f - _e403));
        } else {
            if override_type_3_10 {
                let _e407 = base[3u];
                let _e409 = fog[3u];
                base[3u] = (_e407 * (1f - _e409));
            } else {
                let _e413 = base;
                let _e414 = fog;
                let _e416 = unnamed.fogColor;
                let _e419 = fog[3u];
                base = mix(_e413, (_e414 * _e416), vec4(_e419));
            }
        }
    }
    if override_type_3_11 {
        let _e423 = base[3u];
        if (_e423 == 0f) {
            discard;
        }
    } else {
        if override_type_3_12 {
            let _e425 = base;
            let _e427 = base;
            if (dot(_e425.xyz, _e427.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e431 = base;
    out_color = _e431;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e23 = out_temporal_velocity;
    let _e24 = out_temporal_validity;
    let _e25 = out_color;
    return FragmentOutput(_e23, _e24, _e25);
}
