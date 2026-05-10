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
	{ stage: 'vert', source: 'dot.vert',          output: 'dot_vert_spv'          },
	{ stage: 'vert', source: 'fog.vert',          output: 'fog_vert_spv'          },
	{ stage: 'vert', source: 'gamma.vert',        output: 'gamma_vert_spv'        },
	{ stage: 'vert', source: 'q1_ls.vert',        output: 'q1_ls_vert_spv'        },
	{ stage: 'vert', source: 'shadow_depth.vert', output: 'shadow_depth_vert_spv' },
	{ stage: 'vert', source: 'ribbon.vert',       output: 'ribbon_vert_spv'       },
	{ stage: 'vert', source: 'sprite.vert',       output: 'sprite_vert_spv'       },
	{ stage: 'vert', source: 'beam.vert',         output: 'beam_vert_spv'         },
	{ stage: 'vert', source: 'particle.vert',     output: 'particle_vert_spv'     },

	{ stage: 'frag', source: 'blend.frag',        output: 'blend_frag_spv'        },
	{ stage: 'frag', source: 'bloom.frag',        output: 'bloom_frag_spv'        },
	{ stage: 'frag', source: 'blur.frag',         output: 'blur_frag_spv'         },
	{ stage: 'frag', source: 'boost.frag',        output: 'boost_frag_spv'        },
	{ stage: 'frag', source: 'color.frag',        output: 'color_frag_spv'        },
	{ stage: 'frag', source: 'dot.frag',          output: 'dot_frag_spv'          },
	{ stage: 'frag', source: 'fog.frag',          output: 'fog_frag_spv'          },
	// fxaa.frag is built standalone here for parity with the legacy
	// compile.sh, but vk.c does not reference fxaa_frag_spv directly —
	// FXAA is reached via the gamma_fxaa_* variants below. Kept for
	// byte-identity with the legacy output; safe to drop in a future
	// cleanup if confirmed unused.
	{ stage: 'frag', source: 'fxaa.frag',         output: 'fxaa_frag_spv'         },
	{ stage: 'frag', source: 'gamma.frag',        output: 'gamma_frag_spv'        },
	{ stage: 'frag', source: 'q1_ls.frag',        output: 'q1_ls_frag_spv'        },
	{ stage: 'frag', source: 'q1_ls_array.frag',  output: 'q1_ls_array_frag_spv'  },
	{ stage: 'frag', source: 'shadow_depth.frag', output: 'shadow_depth_frag_spv' },
	{ stage: 'frag', source: 'water.frag',        output: 'water_frag_spv'        },
	{ stage: 'frag', source: 'ribbon.frag',       output: 'ribbon_frag_spv'       },
	{ stage: 'frag', source: 'sprite.frag',       output: 'sprite_frag_spv'       },
	{ stage: 'frag', source: 'beam.frag',         output: 'beam_frag_spv'         },
	{ stage: 'frag', source: 'particle.frag',     output: 'particle_frag_spv'     },
	{ stage: 'comp', source: 'particle_integrate.comp', output: 'particle_integrate_comp_spv' },

	// ── Gamma post-process variants ───────────────────────────
	{ stage: 'frag', source: 'gamma.frag', defines: ['USE_SSAO'],                                                output: 'gamma_ssao_frag_spv'           },
	{ stage: 'frag', source: 'gamma.frag', defines: ['USE_TONEMAP'],                                             output: 'gamma_tonemap_frag_spv'        },
	{ stage: 'frag', source: 'gamma.frag', defines: ['USE_COLOR_GRADING'],                                       output: 'gamma_colorgrade_frag_spv'     },
	{ stage: 'frag', source: 'gamma.frag', defines: ['USE_SSAO', 'USE_TONEMAP'],                                 output: 'gamma_ssao_tonemap_frag_spv'   },
	{ stage: 'frag', source: 'gamma.frag', defines: ['USE_SSAO', 'USE_TONEMAP', 'USE_COLOR_GRADING'],            output: 'gamma_full_frag_spv'           },
	{ stage: 'frag', source: 'gamma.frag', defines: ['USE_TONEMAP', 'USE_COLOR_GRADING'],                        output: 'gamma_tonemap_cg_frag_spv'     },
	{ stage: 'frag', source: 'gamma.frag', defines: ['USE_FXAA'],                                                output: 'gamma_fxaa_frag_spv'           },
	{ stage: 'frag', source: 'gamma.frag', defines: ['USE_FXAA', 'USE_SSAO'],                                    output: 'gamma_fxaa_ssao_frag_spv'      },
	{ stage: 'frag', source: 'gamma.frag', defines: ['USE_FXAA', 'USE_SSAO', 'USE_TONEMAP', 'USE_COLOR_GRADING'], output: 'gamma_all_frag_spv'           },
	{ stage: 'frag', source: 'gamma.frag', defines: ['USE_GODRAYS'],                                             output: 'gamma_godrays_frag_spv'        },
	{ stage: 'frag', source: 'gamma.frag', defines: ['USE_SSAO', 'USE_GODRAYS'],                                 output: 'gamma_ssao_godrays_frag_spv'   },
	{ stage: 'frag', source: 'gamma.frag', defines: ['USE_SSAO', 'USE_GODRAYS', 'USE_TONEMAP'],                  output: 'gamma_ssao_godrays_tm_frag_spv'},
	{ stage: 'frag', source: 'gamma.frag', defines: ['USE_FXAA', 'USE_SSAO', 'USE_GODRAYS', 'USE_TONEMAP', 'USE_COLOR_GRADING'], output: 'gamma_ultimate_frag_spv' },

	// ── Lighting template variants (light_vert.tmpl, light_frag.tmpl) ──
	{ stage: 'vert', source: 'light_vert.tmpl',                                                  output: 'vert_light'                       },
	{ stage: 'vert', source: 'light_vert.tmpl', defines: ['USE_FOG'],                            output: 'vert_light_fog'                   },
	{ stage: 'frag', source: 'light_frag.tmpl',                                                  output: 'frag_light'                       },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_FOG'],                            output: 'frag_light_fog'                   },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_LINE'],                           output: 'frag_light_line'                  },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_LINE', 'USE_FOG'],                output: 'frag_light_line_fog'              },
	// parallax mapping
	{ stage: 'vert', source: 'light_vert.tmpl', defines: ['USE_PARALLAX'],                       output: 'vert_light_parallax'              },
	{ stage: 'vert', source: 'light_vert.tmpl', defines: ['USE_PARALLAX', 'USE_FOG'],            output: 'vert_light_parallax_fog'          },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_PARALLAX'],                       output: 'frag_light_parallax'              },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_PARALLAX', 'USE_FOG'],            output: 'frag_light_parallax_fog'          },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_PARALLAX', 'USE_LINE'],           output: 'frag_light_parallax_line'         },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_PARALLAX', 'USE_LINE', 'USE_FOG'], output: 'frag_light_parallax_line_fog'    },
	// shadow mapping
	{ stage: 'vert', source: 'light_vert.tmpl', defines: ['USE_SHADOWMAP'],                      output: 'vert_light_shadow'                },
	{ stage: 'vert', source: 'light_vert.tmpl', defines: ['USE_SHADOWMAP', 'USE_FOG'],           output: 'vert_light_shadow_fog'            },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_SHADOWMAP'],                      output: 'frag_light_shadow'                },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_SHADOWMAP', 'USE_FOG'],           output: 'frag_light_shadow_fog'            },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_SHADOWMAP', 'USE_LINE'],          output: 'frag_light_shadow_line'           },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_SHADOWMAP', 'USE_LINE', 'USE_FOG'], output: 'frag_light_shadow_line_fog'     },
	// PBR
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_PBR'],                            output: 'frag_light_pbr'                   },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_PBR', 'USE_FOG'],                 output: 'frag_light_pbr_fog'               },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_PBR', 'USE_LINE'],                output: 'frag_light_pbr_line'              },
	{ stage: 'frag', source: 'light_frag.tmpl', defines: ['USE_PBR', 'USE_LINE', 'USE_FOG'],     output: 'frag_light_pbr_line_fog'          },

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

	// ── Generic fragment (gen_frag.tmpl) ──────────────────────
	// single-texture, generic
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_ATEST'],                                                  output: 'frag_tx0'                  },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_ATEST', 'USE_FOG'],                                       output: 'frag_tx0_fog'              },
	// single-texture, identity color
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CLX_IDENT', 'USE_ATEST'],                                 output: 'frag_tx0_ident1'           },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CLX_IDENT', 'USE_ATEST', 'USE_FOG'],                      output: 'frag_tx0_ident1_fog'       },
	// single-texture, fixed color
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_FIXED_COLOR', 'USE_ATEST'],                               output: 'frag_tx0_fixed'            },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_FIXED_COLOR', 'USE_ATEST', 'USE_FOG'],                    output: 'frag_tx0_fixed_fog'        },
	// single-texture, entity color
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_ENT_COLOR', 'USE_ATEST'],                                 output: 'frag_tx0_ent'              },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_ENT_COLOR', 'USE_ATEST', 'USE_FOG'],                      output: 'frag_tx0_ent_fog'          },
	// single-texture, depth-fragment
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CLX_IDENT', 'USE_ATEST', 'USE_DF'],                       output: 'frag_tx0_df'               },
	// double-texture
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_TX1'],                                                    output: 'frag_tx1'                  },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_TX1', 'USE_FOG'],                                         output: 'frag_tx1_fog'              },
	// double-texture, identity colors
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CLX_IDENT', 'USE_TX1'],                                   output: 'frag_tx1_ident1'           },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CLX_IDENT', 'USE_TX1', 'USE_FOG'],                        output: 'frag_tx1_ident1_fog'       },
	// double-texture, fixed colors
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_FIXED_COLOR', 'USE_TX1'],                                 output: 'frag_tx1_fixed'            },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_FIXED_COLOR', 'USE_TX1', 'USE_FOG'],                      output: 'frag_tx1_fixed_fog'        },
	// double-texture, non-identical colors
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CL1', 'USE_TX1'],                                         output: 'frag_tx1_cl'               },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CL1', 'USE_TX1', 'USE_FOG'],                              output: 'frag_tx1_cl_fog'           },
	// triple-texture
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_TX2'],                                                    output: 'frag_tx2'                  },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_TX2', 'USE_FOG'],                                         output: 'frag_tx2_fog'              },
	// triple-texture, non-identical colors
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CL2', 'USE_TX2'],                                         output: 'frag_tx2_cl'               },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_CL2', 'USE_TX2', 'USE_FOG'],                              output: 'frag_tx2_cl_fog'           },

	// ── Depth fade (single-texture only) ──────────────────────
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_DEPTH_FADE', 'USE_ATEST'],                                output: 'frag_tx0_dfade'            },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_DEPTH_FADE', 'USE_ATEST', 'USE_FOG'],                     output: 'frag_tx0_dfade_fog'        },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_DEPTH_FADE', 'USE_CLX_IDENT', 'USE_ATEST'],               output: 'frag_tx0_ident1_dfade'     },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_DEPTH_FADE', 'USE_CLX_IDENT', 'USE_ATEST', 'USE_FOG'],    output: 'frag_tx0_ident1_dfade_fog' },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_DEPTH_FADE', 'USE_FIXED_COLOR', 'USE_ATEST'],             output: 'frag_tx0_fixed_dfade'      },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_DEPTH_FADE', 'USE_FIXED_COLOR', 'USE_ATEST', 'USE_FOG'],  output: 'frag_tx0_fixed_dfade_fog'  },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_DEPTH_FADE', 'USE_ENT_COLOR', 'USE_ATEST'],               output: 'frag_tx0_ent_dfade'        },
	{ stage: 'frag', source: 'gen_frag.tmpl', defines: ['USE_DEPTH_FADE', 'USE_ENT_COLOR', 'USE_ATEST', 'USE_FOG'],    output: 'frag_tx0_ent_dfade_fog'    },

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
];
