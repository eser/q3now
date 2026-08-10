// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// shaders.manifest.mjs
//
// Single source of truth for shader compilation. Edit this
// file to add/remove shader variants. compile.mjs reads from
// here.
//
// Each entry:
//   stage:   'vert' | 'frag' | 'comp' (extend as needed)
//   source:  filename relative to this directory
//   defines: array of preprocessor defines (without -D prefix);
//            optional, default []
//   output:  C identifier for the embedded SPIR-V byte array
//
// Naming convention: outputs ending in _vert_spv / _frag_spv /
// _comp_spv came from the auto-discovery loops in the legacy
// compile.sh. Outputs without that suffix (vert_light, frag_tx0,
// etc.) came from explicit invocations in the legacy script and
// are referenced by those exact names in vk.c — do not rename
// without updating vk.c in lockstep.

export default [
	// ── Auto-discovered single-file shaders (legacy compile.sh
	//    `for f in *.vert/*.frag` loops, after filtering smaa_*,
	//    msdf.*, iqm_skinning.*) ─────────────────────────────
	{ stage: 'vert', source: 'color.vert',        output: 'color_vert_spv'        },
	{ stage: 'vert', source: 'fog.vert',          output: 'fog_vert_spv'          },
	{ stage: 'vert', source: 'gamma.vert',        output: 'gamma_vert_spv'        },
	{ stage: 'vert', source: 'overlay.vert',      output: 'overlay_vert_spv'      },
	{ stage: 'vert', source: 'q1_ls.vert',        output: 'q1_ls_vert_spv'        },
	{ stage: 'vert', source: 'shadow_depth.vert', output: 'shadow_depth_vert_spv' },
	{ stage: 'vert', source: 'shadow_depth_skinned.vert', output: 'shadow_depth_skinned_vert_spv' },
	{ stage: 'vert', source: 'shadow_depth_atest.vert', output: 'shadow_depth_atest_vert_spv' },
	{ stage: 'vert', source: 'forwardplus_lit.vert', output: 'forwardplus_lit_vert_spv' },
	{ stage: 'vert', source: 'ribbon.vert',       output: 'ribbon_vert_spv'       },
	{ stage: 'vert', source: 'ribbon_spiral.vert', output: 'ribbon_spiral_vert_spv' },
	{ stage: 'vert', source: 'sprite.vert',       output: 'sprite_vert_spv'       },
	{ stage: 'vert', source: 'beam.vert',         output: 'beam_vert_spv'         },
	{ stage: 'vert', source: 'particle.vert',     output: 'particle_vert_spv'     },
	{ stage: 'vert', source: 'decal.vert',        output: 'decal_vert_spv'        },
	{ stage: 'vert', source: 'atmospheric.vert',  output: 'atmospheric_vert_spv'  },

	{ stage: 'frag', source: 'bloom.frag',        output: 'bloom_frag_spv'        },
	{ stage: 'frag', source: 'downsample.frag',   output: 'downsample_frag_spv'   },
	{ stage: 'frag', source: 'upsample.frag',     output: 'upsample_frag_spv'     },
	{ stage: 'frag', source: 'bloom_composite.frag', output: 'bloom_composite_frag_spv' },
	{ stage: 'frag', source: 'color.frag',        output: 'color_frag_spv'        },
	{ stage: 'frag', source: 'fog.frag',          output: 'fog_frag_spv'          },
	// fxaa.frag removed (SMAA replaces it).
	{ stage: 'frag', source: 'gamma.frag',        output: 'gamma_frag_spv'        },
	{ stage: 'frag', source: 'overlay.frag',      output: 'overlay_frag_spv'      },
	{ stage: 'frag', source: 'tonemap.frag',      output: 'tonemap_frag_spv'      },
	{ stage: 'frag', source: 'q1_ls.frag',        output: 'q1_ls_frag_spv'        },
	{ stage: 'frag', source: 'q1_ls_array.frag',  output: 'q1_ls_array_frag_spv'  },
	{ stage: 'frag', source: 'shadow_depth.frag', output: 'shadow_depth_frag_spv' },
	{ stage: 'frag', source: 'shadow_depth_atest.frag', output: 'shadow_depth_atest_frag_spv' },
	{ stage: 'frag', source: 'forwardplus_lit.frag', output: 'forwardplus_lit_frag_spv' },
	{ stage: 'frag', source: 'water.frag',        output: 'water_frag_spv'        },
	{ stage: 'frag', source: 'ribbon.frag',       output: 'ribbon_frag_spv'       },
	{ stage: 'frag', source: 'sprite.frag',       output: 'sprite_frag_spv'       },
	{ stage: 'frag', source: 'beam.frag',         output: 'beam_frag_spv'         },
	{ stage: 'frag', source: 'particle.frag',     output: 'particle_frag_spv'     },
	{ stage: 'frag', source: 'decal.frag',        output: 'decal_frag_spv'        },
	{ stage: 'frag', source: 'atmospheric.frag',  output: 'atmospheric_frag_spv'  },
	{ stage: 'comp', source: 'particle_integrate.comp', output: 'particle_integrate_comp_spv' },
	{ stage: 'comp', source: 'atmospheric_integrate.comp', output: 'atmospheric_integrate_comp_spv' },
	{ stage: 'comp', source: 'hdr_histogram.comp',      output: 'hdr_histogram_comp_spv'      },
	{ stage: 'comp', source: 'exposure.comp',           output: 'exposure_comp_spv'           },
	{ stage: 'comp', source: 'brdf_lut.comp',           output: 'brdf_lut_comp_spv'           },
	{ stage: 'comp', source: 'source_sky.comp',         output: 'source_sky_comp_spv'         },
	{ stage: 'comp', source: 'convolve_irradiance.comp', output: 'convolve_irradiance_comp_spv' },
	{ stage: 'comp', source: 'prefilter_radiance.comp',  output: 'prefilter_radiance_comp_spv'  },
	{ stage: 'comp', source: 'cull.comp',                output: 'cull_comp_spv'                },
	{ stage: 'comp', source: 'gtao.comp',                output: 'gtao_comp_spv'                },
	{ stage: 'comp', source: 'gtao_denoise.comp',        output: 'gtao_denoise_comp_spv'        },
	{ stage: 'comp', source: 'forwardplus_tile.comp',    output: 'forwardplus_tile_comp_spv'    },
	{ stage: 'comp', source: 'forwardplus_depth_reduce.comp', output: 'forwardplus_depth_reduce_comp_spv' },
	{ stage: 'comp', source: 'lens_occlusion.comp',      output: 'lens_occlusion_comp_spv'      },

	// ── Tonemap post-process variants ─────────────────────────
	// scene-radiance effects (tonemap operator, colour grading, sunrays) live on
	// tonemap.frag now. gamma.frag is the thin display-encoding pass.
	// USE_SSAO variants removed: the legacy per-pixel tonemap SSAO path is fully
	// retired (GTAO is the sole AO path), so the TONEMAP_VAR_SSAO bit is never set
	// and its variants are never selected. Only BASE / CG / SUNRAYS combos remain.
	{ stage: 'frag', source: 'tonemap.frag', defines: ['USE_TONEMAP'],                                             output: 'tonemap_tonemap_frag_spv'        },
	{ stage: 'frag', source: 'tonemap.frag', defines: ['USE_COLOR_GRADING'],                                       output: 'tonemap_colorgrade_frag_spv'     },
	{ stage: 'frag', source: 'tonemap.frag', defines: ['USE_TONEMAP', 'USE_COLOR_GRADING'],                        output: 'tonemap_tonemap_cg_frag_spv'     },
	// USE_FXAA variants removed (SMAA replaces it).
	{ stage: 'frag', source: 'tonemap.frag', defines: ['USE_SUNRAYS'],                                             output: 'tonemap_sunrays_frag_spv'        },
	// the remaining TONEMAP_VAR_* bitmask combos (varIdx 10,12,14). The three
	// surviving feature cvars (r_tonemap=2 / r_colorGrading=4 / r_sunRays=8) are
	// INDEPENDENT toggles, so vk_create_post_process_pipeline(5) can compute any
	// varIdx in {0,2,4,6,8,10,12,14} and index vk.tonemap_variant_fs[varIdx]
	// (odd indices — the SSAO bit — are never produced). varIdx 0 is the separate
	// "default" pipeline at case 6. tonemap.frag's feature defines are independent
	// #ifdefs, so every combo compiles.
	{ stage: 'frag', source: 'tonemap.frag', defines: ['USE_TONEMAP', 'USE_SUNRAYS'],                             output: 'tonemap_sunrays_tm_frag_spv'      },  // varIdx 10
	{ stage: 'frag', source: 'tonemap.frag', defines: ['USE_COLOR_GRADING', 'USE_SUNRAYS'],                       output: 'tonemap_sunrays_cg_frag_spv'      },  // varIdx 12
	{ stage: 'frag', source: 'tonemap.frag', defines: ['USE_TONEMAP', 'USE_COLOR_GRADING', 'USE_SUNRAYS'],        output: 'tonemap_sunrays_tm_cg_frag_spv'   },  // varIdx 14

	// ── Lighting template variants (light_vert.tmpl, light_frag.tmpl) ──
	// legacy-mainpath-retire STEP 3: the legacy non-bindless frag entries
	// here (16 entries: base + parallax + shadow + PBR × {plain, fog, line,
	// line+fog} subsets) are gone. The light_frag.tmpl now compiles
	// unconditionally as bindless; the `_bindless` outputs below are the
	// canonical SPIR-V modules. Vert entries are unchanged (no sampler
	// references; same SPIR-V serves both ex-paths).
	{ stage: 'vert', source: 'light_vert.tmpl',                                                  output: 'vert_light'                       },
	{ stage: 'vert', source: 'light_vert.tmpl', defines: ['USE_FOG'],                            output: 'vert_light_fog'                   },
	// parallax mapping
	{ stage: 'vert', source: 'light_vert.tmpl', defines: ['USE_PARALLAX'],                       output: 'vert_light_parallax'              },
	{ stage: 'vert', source: 'light_vert.tmpl', defines: ['USE_PARALLAX', 'USE_FOG'],            output: 'vert_light_parallax_fog'          },
	// shadow mapping
	{ stage: 'vert', source: 'light_vert.tmpl', defines: ['USE_SHADOWMAP'],                      output: 'vert_light_shadow'                },
	{ stage: 'vert', source: 'light_vert.tmpl', defines: ['USE_SHADOWMAP', 'USE_FOG'],           output: 'vert_light_shadow_fog'            },

	// ── Generic vertex (gen_vert.tmpl) ────────────────────────
	// single-texture
	{ stage: 'vert', source: 'gen_vert.tmpl',                                                                output: 'vert_tx0'                  },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_FOG'],                                          output: 'vert_tx0_fog'              },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_ENV'],                                          output: 'vert_tx0_env'              },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_FOG', 'USE_ENV'],                               output: 'vert_tx0_env_fog'          },
	// single-texture, identity colors
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CLX_IDENT'],                                    output: 'vert_tx0_ident1'           },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CLX_IDENT', 'USE_FOG'],                         output: 'vert_tx0_ident1_fog'       },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CLX_IDENT', 'USE_ENV'],                         output: 'vert_tx0_ident1_env'       },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CLX_IDENT', 'USE_FOG', 'USE_ENV'],              output: 'vert_tx0_ident1_env_fog'   },
	// single-texture, fixed colors
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_FIXED_COLOR'],                                  output: 'vert_tx0_fixed'            },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_FIXED_COLOR', 'USE_FOG'],                       output: 'vert_tx0_fixed_fog'        },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_FIXED_COLOR', 'USE_ENV'],                       output: 'vert_tx0_fixed_env'        },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_FIXED_COLOR', 'USE_FOG', 'USE_ENV'],            output: 'vert_tx0_fixed_env_fog'    },
	// double-texture
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_TX1'],                                          output: 'vert_tx1'                  },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_TX1', 'USE_FOG'],                               output: 'vert_tx1_fog'              },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_TX1', 'USE_ENV'],                               output: 'vert_tx1_env'              },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_TX1', 'USE_FOG', 'USE_ENV'],                    output: 'vert_tx1_env_fog'          },
	// double-texture, identity colors
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CLX_IDENT', 'USE_TX1'],                         output: 'vert_tx1_ident1'           },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CLX_IDENT', 'USE_TX1', 'USE_FOG'],              output: 'vert_tx1_ident1_fog'       },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CLX_IDENT', 'USE_TX1', 'USE_ENV'],              output: 'vert_tx1_ident1_env'       },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CLX_IDENT', 'USE_TX1', 'USE_FOG', 'USE_ENV'],   output: 'vert_tx1_ident1_env_fog'   },
	// double-texture, fixed colors
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_FIXED_COLOR', 'USE_TX1'],                       output: 'vert_tx1_fixed'            },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_FIXED_COLOR', 'USE_TX1', 'USE_FOG'],            output: 'vert_tx1_fixed_fog'        },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_FIXED_COLOR', 'USE_TX1', 'USE_ENV'],            output: 'vert_tx1_fixed_env'        },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_FIXED_COLOR', 'USE_TX1', 'USE_FOG', 'USE_ENV'], output: 'vert_tx1_fixed_env_fog'    },
	// double-texture, non-identical colors
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CL1', 'USE_TX1'],                               output: 'vert_tx1_cl'               },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CL1', 'USE_TX1', 'USE_FOG'],                    output: 'vert_tx1_cl_fog'           },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CL1', 'USE_TX1', 'USE_ENV'],                    output: 'vert_tx1_cl_env'           },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CL1', 'USE_TX1', 'USE_ENV', 'USE_FOG'],         output: 'vert_tx1_cl_env_fog'       },
	// triple-texture
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_TX2'],                                          output: 'vert_tx2'                  },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_TX2', 'USE_FOG'],                               output: 'vert_tx2_fog'              },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_TX2', 'USE_ENV'],                               output: 'vert_tx2_env'              },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_TX2', 'USE_ENV', 'USE_FOG'],                    output: 'vert_tx2_env_fog'          },
	// triple-texture, non-identical colors
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CL2', 'USE_TX2'],                               output: 'vert_tx2_cl'               },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CL2', 'USE_TX2', 'USE_FOG'],                    output: 'vert_tx2_cl_fog'           },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CL2', 'USE_TX2', 'USE_ENV'],                    output: 'vert_tx2_cl_env'           },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CL2', 'USE_TX2', 'USE_ENV', 'USE_FOG'],         output: 'vert_tx2_cl_env_fog'       },
	// sun-shadow receiver variants (USE_SHADOWMAP) ──
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_TX1', 'USE_SHADOWMAP'],                         output: 'vert_tx1_shadow'           },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_TX1', 'USE_FOG', 'USE_SHADOWMAP'],              output: 'vert_tx1_shadow_fog'       },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CL1', 'USE_TX1', 'USE_SHADOWMAP'],              output: 'vert_tx1_cl_shadow'        },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CL1', 'USE_TX1', 'USE_FOG', 'USE_SHADOWMAP'],   output: 'vert_tx1_cl_shadow_fog'    },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_TX2', 'USE_SHADOWMAP'],                         output: 'vert_tx2_shadow'           },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_TX2', 'USE_FOG', 'USE_SHADOWMAP'],              output: 'vert_tx2_shadow_fog'       },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CL2', 'USE_TX2', 'USE_SHADOWMAP'],              output: 'vert_tx2_cl_shadow'        },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CL2', 'USE_TX2', 'USE_FOG', 'USE_SHADOWMAP'],   output: 'vert_tx2_cl_shadow_fog'    },
	// Path PA Fault 2 — IDENTITY / FIXED_COLOR colour-mode sun-shadow vertex variants (MUL2 only)
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CLX_IDENT', 'USE_TX1', 'USE_SHADOWMAP'],               output: 'vert_tx1_ident1_shadow'     },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CLX_IDENT', 'USE_TX1', 'USE_FOG', 'USE_SHADOWMAP'],    output: 'vert_tx1_ident1_shadow_fog' },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_FIXED_COLOR', 'USE_TX1', 'USE_SHADOWMAP'],             output: 'vert_tx1_fixed_shadow'      },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_FIXED_COLOR', 'USE_TX1', 'USE_FOG', 'USE_SHADOWMAP'],  output: 'vert_tx1_fixed_shadow_fog'  },
	// Shadow-Unification Part 1 — single-texture (tx0) sun-shadow vertex variants. The
	// separate-lightmap-pass inset carries its lightmap as a lone single-texture stage
	// (bundle[0]); these let that stage be a sun-shadow receiver (plain vertex-colour,
	// IDENTITY, FIXED_COLOR — the single-texture types the lightmap stage can take).
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_SHADOWMAP'],                                          output: 'vert_tx0_shadow'            },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_FOG', 'USE_SHADOWMAP'],                               output: 'vert_tx0_shadow_fog'        },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CLX_IDENT', 'USE_SHADOWMAP'],                         output: 'vert_tx0_ident1_shadow'     },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CLX_IDENT', 'USE_FOG', 'USE_SHADOWMAP'],              output: 'vert_tx0_ident1_shadow_fog' },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_FIXED_COLOR', 'USE_SHADOWMAP'],                       output: 'vert_tx0_fixed_shadow'      },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_FIXED_COLOR', 'USE_FOG', 'USE_SHADOWMAP'],            output: 'vert_tx0_fixed_shadow_fog'  },
	// base-pass IBL receiver variants (USE_IBL) — always-on environment ambient
	// for pbrMap worldspawn lightmap surfaces. Worldspawn lightmap-modulate only
	// (the same subset the sun-shadow variants cover); USE_IBL is exclusive with
	// USE_ATEST / USE_DEPTH_FADE / USE_ENT_COLOR / USE_FIXED_COLOR / USE_ENV.
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_TX1', 'USE_IBL'],                                output: 'vert_tx1_ibl'              },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_TX1', 'USE_FOG', 'USE_IBL'],                     output: 'vert_tx1_ibl_fog'          },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CL1', 'USE_TX1', 'USE_IBL'],                     output: 'vert_tx1_cl_ibl'           },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CL1', 'USE_TX1', 'USE_FOG', 'USE_IBL'],          output: 'vert_tx1_cl_ibl_fog'       },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_TX2', 'USE_IBL'],                                output: 'vert_tx2_ibl'              },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_TX2', 'USE_FOG', 'USE_IBL'],                     output: 'vert_tx2_ibl_fog'          },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CL2', 'USE_TX2', 'USE_IBL'],                     output: 'vert_tx2_cl_ibl'           },
	{ stage: 'vert', source: 'gen_vert.tmpl', defines: ['USE_CL2', 'USE_TX2', 'USE_FOG', 'USE_IBL'],          output: 'vert_tx2_cl_ibl_fog'       },

	// ── Generic fragment (gen_frag.tmpl) ──────────────────────
	// legacy-mainpath-retire STEP 3: the legacy non-bindless gen_frag entries
	// (29 base + dfade entries) are gone. gen_frag.tmpl compiles
	// unconditionally as bindless; the `_bindless` outputs further below are
	// the canonical SPIR-V modules.

	// ── SMAA ──────────────────────────────────────────────────
	{ stage: 'vert', source: 'smaa_edge.vert',    output: 'smaa_edge_vert_spv'    },
	{ stage: 'frag', source: 'smaa_edge.frag',    output: 'smaa_edge_frag_spv'    },
	{ stage: 'vert', source: 'smaa_blend.vert',   output: 'smaa_blend_vert_spv'   },
	{ stage: 'frag', source: 'smaa_blend.frag',   output: 'smaa_blend_frag_spv'   },
	{ stage: 'vert', source: 'smaa_resolve.vert', output: 'smaa_resolve_vert_spv' },
	{ stage: 'frag', source: 'smaa_resolve.frag', output: 'smaa_resolve_frag_spv' },

	// ── IQM GPU skinning ──────────────────────────────────────
	{ stage: 'vert', source: 'iqm_skinning.vert', output: 'iqm_skinning_vert_spv' },
	{ stage: 'frag', source: 'iqm_skinning.frag', output: 'iqm_skinning_frag_spv' },

	// ── MSDF text ─────────────────────────────────────────────
	{ stage: 'vert', source: 'msdf.vert',         output: 'msdf_vert_spv'         },
	{ stage: 'frag', source: 'msdf.frag',         output: 'msdf_frag_spv'         },
	{ stage: 'frag', source: 'menubg.frag',       output: 'menubg_frag_spv'       },  // WiredUI SCENE procedural backdrop

	// ── Canonical gen_frag / light_frag SPIR-V variants ───────
	// legacy-mainpath-retire STEP 3: gen_frag.tmpl / light_frag.tmpl now
	// compile unconditionally as bindless (the legacy `#if USE_BINDLESS /
	// #else` branches were collapsed), so the USE_BINDLESS define is dead
	// and was removed from the defines arrays below. The `_bindless`
	// output-name suffix is intentionally kept — renaming the 29+16 files
	// and the SHADER_MODULE call sites in vk.c is pure cosmetic churn.
	// Vert shaders unchanged (no sampler refs); the existing vert_* /
	// vert_light_* SPIR-V modules pair with these frag modules at
	// pipeline-create time.
	// gen_frag bindless variants (29 entries) ─────────────────
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_ATEST'],                                                  output: 'frag_tx0_bindless'                  },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_ATEST', 'USE_FOG'],                                       output: 'frag_tx0_fog_bindless'              },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CLX_IDENT', 'USE_ATEST'],                                 output: 'frag_tx0_ident1_bindless'           },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CLX_IDENT', 'USE_ATEST', 'USE_FOG'],                      output: 'frag_tx0_ident1_fog_bindless'       },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_FIXED_COLOR', 'USE_ATEST'],                               output: 'frag_tx0_fixed_bindless'            },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_FIXED_COLOR', 'USE_ATEST', 'USE_FOG'],                    output: 'frag_tx0_fixed_fog_bindless'        },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_ENT_COLOR', 'USE_ATEST'],                                 output: 'frag_tx0_ent_bindless'              },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_ENT_COLOR', 'USE_ATEST', 'USE_FOG'],                      output: 'frag_tx0_ent_fog_bindless'          },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CLX_IDENT', 'USE_ATEST', 'USE_DF'],                       output: 'frag_tx0_df_bindless'               },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_TX1'],                                                    output: 'frag_tx1_bindless'                  },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_TX1', 'USE_FOG'],                                         output: 'frag_tx1_fog_bindless'              },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CLX_IDENT', 'USE_TX1'],                                   output: 'frag_tx1_ident1_bindless'           },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CLX_IDENT', 'USE_TX1', 'USE_FOG'],                        output: 'frag_tx1_ident1_fog_bindless'       },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_FIXED_COLOR', 'USE_TX1'],                                 output: 'frag_tx1_fixed_bindless'            },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_FIXED_COLOR', 'USE_TX1', 'USE_FOG'],                      output: 'frag_tx1_fixed_fog_bindless'        },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CL1', 'USE_TX1'],                                         output: 'frag_tx1_cl_bindless'               },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CL1', 'USE_TX1', 'USE_FOG'],                              output: 'frag_tx1_cl_fog_bindless'           },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_TX2'],                                                    output: 'frag_tx2_bindless'                  },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_TX2', 'USE_FOG'],                                         output: 'frag_tx2_fog_bindless'              },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CL2', 'USE_TX2'],                                         output: 'frag_tx2_cl_bindless'               },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CL2', 'USE_TX2', 'USE_FOG'],                              output: 'frag_tx2_cl_fog_bindless'           },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_DEPTH_FADE', 'USE_ATEST'],                                output: 'frag_tx0_dfade_bindless'            },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_DEPTH_FADE', 'USE_ATEST', 'USE_FOG'],                     output: 'frag_tx0_dfade_fog_bindless'        },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_DEPTH_FADE', 'USE_CLX_IDENT', 'USE_ATEST'],               output: 'frag_tx0_ident1_dfade_bindless'     },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_DEPTH_FADE', 'USE_CLX_IDENT', 'USE_ATEST', 'USE_FOG'],    output: 'frag_tx0_ident1_dfade_fog_bindless' },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_DEPTH_FADE', 'USE_FIXED_COLOR', 'USE_ATEST'],             output: 'frag_tx0_fixed_dfade_bindless'      },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_DEPTH_FADE', 'USE_FIXED_COLOR', 'USE_ATEST', 'USE_FOG'],  output: 'frag_tx0_fixed_dfade_fog_bindless'  },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_DEPTH_FADE', 'USE_ENT_COLOR', 'USE_ATEST'],               output: 'frag_tx0_ent_dfade_bindless'        },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_DEPTH_FADE', 'USE_ENT_COLOR', 'USE_ATEST', 'USE_FOG'],    output: 'frag_tx0_ent_dfade_fog_bindless'    },

	// sun-shadow receiver variants (USE_SHADOWMAP). gen_frag's
	// world-lightmap modulate branches gain CSM sun modulation; these are new
	// SEPARATE variants — existing non-shadow gen pipelines are untouched.
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_TX1', 'USE_SHADOWMAP'],                                  output: 'frag_tx1_shadow_bindless'           },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_TX1', 'USE_FOG', 'USE_SHADOWMAP'],                       output: 'frag_tx1_shadow_fog_bindless'       },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CL1', 'USE_TX1', 'USE_SHADOWMAP'],                       output: 'frag_tx1_cl_shadow_bindless'        },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CL1', 'USE_TX1', 'USE_FOG', 'USE_SHADOWMAP'],            output: 'frag_tx1_cl_shadow_fog_bindless'    },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_TX2', 'USE_SHADOWMAP'],                                  output: 'frag_tx2_shadow_bindless'           },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_TX2', 'USE_FOG', 'USE_SHADOWMAP'],                       output: 'frag_tx2_shadow_fog_bindless'       },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CL2', 'USE_TX2', 'USE_SHADOWMAP'],                       output: 'frag_tx2_cl_shadow_bindless'        },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CL2', 'USE_TX2', 'USE_FOG', 'USE_SHADOWMAP'],            output: 'frag_tx2_cl_shadow_fog_bindless'    },
	// Path PA Fault 2 — IDENTITY / FIXED_COLOR colour-mode sun-shadow fragment variants (MUL2 only)
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CLX_IDENT', 'USE_TX1', 'USE_SHADOWMAP'],                output: 'frag_tx1_ident1_shadow_bindless'      },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CLX_IDENT', 'USE_TX1', 'USE_FOG', 'USE_SHADOWMAP'],     output: 'frag_tx1_ident1_shadow_fog_bindless'  },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_FIXED_COLOR', 'USE_TX1', 'USE_SHADOWMAP'],              output: 'frag_tx1_fixed_shadow_bindless'       },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_FIXED_COLOR', 'USE_TX1', 'USE_FOG', 'USE_SHADOWMAP'],   output: 'frag_tx1_fixed_shadow_fog_bindless'   },
	// Shadow-Unification Part 1 — single-texture (tx0) sun-shadow fragment variants. The
	// operand-keyed wired_apply_sun_shadow_operand (gen_frag) samples the sun shadow on
	// the single-texture lightmap operand (lightmap_slot == 1 -> frag_tex_coord0), so the
	// separate-lightmap-pass inset's lone lightmap stage finally receives the sun shadow.
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_SHADOWMAP'],                                          output: 'frag_tx0_shadow_bindless'            },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_FOG', 'USE_SHADOWMAP'],                               output: 'frag_tx0_shadow_fog_bindless'        },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CLX_IDENT', 'USE_SHADOWMAP'],                         output: 'frag_tx0_ident1_shadow_bindless'     },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CLX_IDENT', 'USE_FOG', 'USE_SHADOWMAP'],              output: 'frag_tx0_ident1_shadow_fog_bindless' },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_FIXED_COLOR', 'USE_SHADOWMAP'],                       output: 'frag_tx0_fixed_shadow_bindless'      },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_FIXED_COLOR', 'USE_FOG', 'USE_SHADOWMAP'],            output: 'frag_tx0_fixed_shadow_fog_bindless'  },

	// base-pass IBL receiver variants (USE_IBL) — always-on environment ambient
	// for pbrMap worldspawn lightmap surfaces. Paired with the vert_*_ibl modules
	// above; gen_frag adds the iblAmbient term (role-8 pbrMap, set-2 probe cubes).
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_TX1', 'USE_IBL'],                                output: 'frag_tx1_ibl_bindless'              },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_TX1', 'USE_FOG', 'USE_IBL'],                     output: 'frag_tx1_ibl_fog_bindless'          },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CL1', 'USE_TX1', 'USE_IBL'],                     output: 'frag_tx1_cl_ibl_bindless'           },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CL1', 'USE_TX1', 'USE_FOG', 'USE_IBL'],          output: 'frag_tx1_cl_ibl_fog_bindless'       },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_TX2', 'USE_IBL'],                                output: 'frag_tx2_ibl_bindless'              },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_TX2', 'USE_FOG', 'USE_IBL'],                     output: 'frag_tx2_ibl_fog_bindless'          },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CL2', 'USE_TX2', 'USE_IBL'],                     output: 'frag_tx2_cl_ibl_bindless'           },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CL2', 'USE_TX2', 'USE_FOG', 'USE_IBL'],          output: 'frag_tx2_cl_ibl_fog_bindless'       },

	// light_frag bindless variants (16 entries) ───────────────
	{ stage: 'frag', source: 'light_frag.tmpl', defines: [],                                                  output: 'frag_light_bindless'                       },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_FOG'],                                       output: 'frag_light_fog_bindless'                   },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_LINE'],                                      output: 'frag_light_line_bindless'                  },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_LINE', 'USE_FOG'],                           output: 'frag_light_line_fog_bindless'              },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_PARALLAX'],                                  output: 'frag_light_parallax_bindless'              },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_PARALLAX', 'USE_FOG'],                       output: 'frag_light_parallax_fog_bindless'          },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_PARALLAX', 'USE_LINE'],                      output: 'frag_light_parallax_line_bindless'         },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_PARALLAX', 'USE_LINE', 'USE_FOG'],           output: 'frag_light_parallax_line_fog_bindless'     },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_SHADOWMAP'],                                 output: 'frag_light_shadow_bindless'                },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_SHADOWMAP', 'USE_FOG'],                      output: 'frag_light_shadow_fog_bindless'            },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_SHADOWMAP', 'USE_LINE'],                     output: 'frag_light_shadow_line_bindless'           },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_SHADOWMAP', 'USE_LINE', 'USE_FOG'],          output: 'frag_light_shadow_line_fog_bindless'       },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_PBR'],                                       output: 'frag_light_pbr_bindless'                   },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_PBR', 'USE_FOG'],                            output: 'frag_light_pbr_fog_bindless'               },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_PBR', 'USE_LINE'],                           output: 'frag_light_pbr_line_bindless'              },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_PBR', 'USE_LINE', 'USE_FOG'],                output: 'frag_light_pbr_line_fog_bindless'          },
];
