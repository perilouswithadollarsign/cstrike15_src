#ifdef GET_NEON_BUILTINS
BUILTIN(__builtin_neon_vabd_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vabdq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vabs_v, "V8ScV8Sci", "n")
BUILTIN(__builtin_neon_vabsq_v, "V16ScV16Sci", "n")
BUILTIN(__builtin_neon_vaddhn_v, "V8ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vbsl_v, "V8ScV8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vbslq_v, "V16ScV16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vcage_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vcageq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vcagt_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vcagtq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vcale_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vcaleq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vcalt_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vcaltq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vcls_v, "V8ScV8Sci", "n")
BUILTIN(__builtin_neon_vclsq_v, "V16ScV16Sci", "n")
BUILTIN(__builtin_neon_vclz_v, "V8ScV8Sci", "n")
BUILTIN(__builtin_neon_vclzq_v, "V16ScV16Sci", "n")
BUILTIN(__builtin_neon_vcnt_v, "V8ScV8Sci", "n")
BUILTIN(__builtin_neon_vcntq_v, "V16ScV16Sci", "n")
BUILTIN(__builtin_neon_vcvt_f16_v, "V8ScV16Sci", "n")
BUILTIN(__builtin_neon_vcvt_f32_v, "V2fV8Sci", "n")
BUILTIN(__builtin_neon_vcvtq_f32_v, "V4fV16Sci", "n")
BUILTIN(__builtin_neon_vcvt_f32_f16, "V4fV8Sci", "n")
BUILTIN(__builtin_neon_vcvt_n_f32_v, "V2fV8Scii", "n")
BUILTIN(__builtin_neon_vcvtq_n_f32_v, "V4fV16Scii", "n")
BUILTIN(__builtin_neon_vcvt_n_s32_v, "V8ScV8Scii", "n")
BUILTIN(__builtin_neon_vcvtq_n_s32_v, "V16ScV16Scii", "n")
BUILTIN(__builtin_neon_vcvt_n_u32_v, "V8ScV8Scii", "n")
BUILTIN(__builtin_neon_vcvtq_n_u32_v, "V16ScV16Scii", "n")
BUILTIN(__builtin_neon_vcvt_s32_v, "V8ScV8Sci", "n")
BUILTIN(__builtin_neon_vcvtq_s32_v, "V16ScV16Sci", "n")
BUILTIN(__builtin_neon_vcvt_u32_v, "V8ScV8Sci", "n")
BUILTIN(__builtin_neon_vcvtq_u32_v, "V16ScV16Sci", "n")
BUILTIN(__builtin_neon_vext_v, "V8ScV8ScV8Scii", "n")
BUILTIN(__builtin_neon_vextq_v, "V16ScV16ScV16Scii", "n")
BUILTIN(__builtin_neon_vfma_v, "V8ScV8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vfmaq_v, "V16ScV16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vget_lane_i8, "UcV8Sci", "n")
BUILTIN(__builtin_neon_vget_lane_i16, "UsV4si", "n")
BUILTIN(__builtin_neon_vget_lane_i32, "UiV2ii", "n")
BUILTIN(__builtin_neon_vget_lane_f32, "fV2fi", "n")
BUILTIN(__builtin_neon_vgetq_lane_i8, "UcV16Sci", "n")
BUILTIN(__builtin_neon_vgetq_lane_i16, "UsV8si", "n")
BUILTIN(__builtin_neon_vgetq_lane_i32, "UiV4ii", "n")
BUILTIN(__builtin_neon_vgetq_lane_f32, "fV4fi", "n")
BUILTIN(__builtin_neon_vget_lane_i64, "ULLiV1LLii", "n")
BUILTIN(__builtin_neon_vgetq_lane_i64, "ULLiV2LLii", "n")
BUILTIN(__builtin_neon_vhadd_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vhaddq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vhsub_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vhsubq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vld1q_v, "V16ScvC*i", "n")
BUILTIN(__builtin_neon_vld1_v, "V8ScvC*i", "n")
BUILTIN(__builtin_neon_vld1q_dup_v, "V16ScvC*i", "n")
BUILTIN(__builtin_neon_vld1_dup_v, "V8ScvC*i", "n")
BUILTIN(__builtin_neon_vld1q_lane_v, "V16ScvC*V16Scii", "n")
BUILTIN(__builtin_neon_vld1_lane_v, "V8ScvC*V8Scii", "n")
BUILTIN(__builtin_neon_vld2q_v, "vv*vC*i", "n")
BUILTIN(__builtin_neon_vld2_v, "vv*vC*i", "n")
BUILTIN(__builtin_neon_vld2_dup_v, "vv*vC*i", "n")
BUILTIN(__builtin_neon_vld2q_lane_v, "vv*vC*V16ScV16Scii", "n")
BUILTIN(__builtin_neon_vld2_lane_v, "vv*vC*V8ScV8Scii", "n")
BUILTIN(__builtin_neon_vld3q_v, "vv*vC*i", "n")
BUILTIN(__builtin_neon_vld3_v, "vv*vC*i", "n")
BUILTIN(__builtin_neon_vld3_dup_v, "vv*vC*i", "n")
BUILTIN(__builtin_neon_vld3q_lane_v, "vv*vC*V16ScV16ScV16Scii", "n")
BUILTIN(__builtin_neon_vld3_lane_v, "vv*vC*V8ScV8ScV8Scii", "n")
BUILTIN(__builtin_neon_vld4q_v, "vv*vC*i", "n")
BUILTIN(__builtin_neon_vld4_v, "vv*vC*i", "n")
BUILTIN(__builtin_neon_vld4_dup_v, "vv*vC*i", "n")
BUILTIN(__builtin_neon_vld4q_lane_v, "vv*vC*V16ScV16ScV16ScV16Scii", "n")
BUILTIN(__builtin_neon_vld4_lane_v, "vv*vC*V8ScV8ScV8ScV8Scii", "n")
BUILTIN(__builtin_neon_vmax_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vmaxq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vmin_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vminq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vmovl_v, "V16ScV8Sci", "n")
BUILTIN(__builtin_neon_vmovn_v, "V8ScV16Sci", "n")
BUILTIN(__builtin_neon_vmull_v, "V16ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vmul_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vmulq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vpadal_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vpadalq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vpadd_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vpaddl_v, "V8ScV8Sci", "n")
BUILTIN(__builtin_neon_vpaddlq_v, "V16ScV16Sci", "n")
BUILTIN(__builtin_neon_vpmax_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vpmin_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vqabs_v, "V8ScV8Sci", "n")
BUILTIN(__builtin_neon_vqabsq_v, "V16ScV16Sci", "n")
BUILTIN(__builtin_neon_vqadd_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vqaddq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vqdmlal_v, "V16ScV16ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vqdmlsl_v, "V16ScV16ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vqdmulh_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vqdmulhq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vqdmull_v, "V16ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vqmovn_v, "V8ScV16Sci", "n")
BUILTIN(__builtin_neon_vqmovun_v, "V8ScV16Sci", "n")
BUILTIN(__builtin_neon_vqneg_v, "V8ScV8Sci", "n")
BUILTIN(__builtin_neon_vqnegq_v, "V16ScV16Sci", "n")
BUILTIN(__builtin_neon_vqrdmulh_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vqrdmulhq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vqrshl_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vqrshlq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vqrshrn_n_v, "V8ScV16Scii", "n")
BUILTIN(__builtin_neon_vqrshrun_n_v, "V8ScV16Scii", "n")
BUILTIN(__builtin_neon_vqshl_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vqshlq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vqshlu_n_v, "V8ScV8Scii", "n")
BUILTIN(__builtin_neon_vqshluq_n_v, "V16ScV16Scii", "n")
BUILTIN(__builtin_neon_vqshl_n_v, "V8ScV8Scii", "n")
BUILTIN(__builtin_neon_vqshlq_n_v, "V16ScV16Scii", "n")
BUILTIN(__builtin_neon_vqshrn_n_v, "V8ScV16Scii", "n")
BUILTIN(__builtin_neon_vqshrun_n_v, "V8ScV16Scii", "n")
BUILTIN(__builtin_neon_vqsub_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vqsubq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vraddhn_v, "V8ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vrecpe_v, "V8ScV8Sci", "n")
BUILTIN(__builtin_neon_vrecpeq_v, "V16ScV16Sci", "n")
BUILTIN(__builtin_neon_vrecps_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vrecpsq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vrhadd_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vrhaddq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vrshl_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vrshlq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vrshrn_n_v, "V8ScV16Scii", "n")
BUILTIN(__builtin_neon_vrshr_n_v, "V8ScV8Scii", "n")
BUILTIN(__builtin_neon_vrshrq_n_v, "V16ScV16Scii", "n")
BUILTIN(__builtin_neon_vrsqrte_v, "V8ScV8Sci", "n")
BUILTIN(__builtin_neon_vrsqrteq_v, "V16ScV16Sci", "n")
BUILTIN(__builtin_neon_vrsqrts_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vrsqrtsq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vrsra_n_v, "V8ScV8ScV8Scii", "n")
BUILTIN(__builtin_neon_vrsraq_n_v, "V16ScV16ScV16Scii", "n")
BUILTIN(__builtin_neon_vrsubhn_v, "V8ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vset_lane_i8, "V8ScUcV8Sci", "n")
BUILTIN(__builtin_neon_vset_lane_i16, "V4sUsV4si", "n")
BUILTIN(__builtin_neon_vset_lane_i32, "V2iUiV2ii", "n")
BUILTIN(__builtin_neon_vset_lane_f32, "V2ffV2fi", "n")
BUILTIN(__builtin_neon_vsetq_lane_i8, "V16ScUcV16Sci", "n")
BUILTIN(__builtin_neon_vsetq_lane_i16, "V8sUsV8si", "n")
BUILTIN(__builtin_neon_vsetq_lane_i32, "V4iUiV4ii", "n")
BUILTIN(__builtin_neon_vsetq_lane_f32, "V4ffV4fi", "n")
BUILTIN(__builtin_neon_vset_lane_i64, "V1LLiULLiV1LLii", "n")
BUILTIN(__builtin_neon_vsetq_lane_i64, "V2LLiULLiV2LLii", "n")
BUILTIN(__builtin_neon_vshl_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vshlq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vshll_n_v, "V16ScV8Scii", "n")
BUILTIN(__builtin_neon_vshl_n_v, "V8ScV8Scii", "n")
BUILTIN(__builtin_neon_vshlq_n_v, "V16ScV16Scii", "n")
BUILTIN(__builtin_neon_vshrn_n_v, "V8ScV16Scii", "n")
BUILTIN(__builtin_neon_vshr_n_v, "V8ScV8Scii", "n")
BUILTIN(__builtin_neon_vshrq_n_v, "V16ScV16Scii", "n")
BUILTIN(__builtin_neon_vsli_n_v, "V8ScV8ScV8Scii", "n")
BUILTIN(__builtin_neon_vsliq_n_v, "V16ScV16ScV16Scii", "n")
BUILTIN(__builtin_neon_vsra_n_v, "V8ScV8ScV8Scii", "n")
BUILTIN(__builtin_neon_vsraq_n_v, "V16ScV16ScV16Scii", "n")
BUILTIN(__builtin_neon_vsri_n_v, "V8ScV8ScV8Scii", "n")
BUILTIN(__builtin_neon_vsriq_n_v, "V16ScV16ScV16Scii", "n")
BUILTIN(__builtin_neon_vst1q_v, "vv*V16Sci", "n")
BUILTIN(__builtin_neon_vst1_v, "vv*V8Sci", "n")
BUILTIN(__builtin_neon_vst1q_lane_v, "vv*V16Scii", "n")
BUILTIN(__builtin_neon_vst1_lane_v, "vv*V8Scii", "n")
BUILTIN(__builtin_neon_vst2q_v, "vv*V16ScV16Sci", "n")
BUILTIN(__builtin_neon_vst2_v, "vv*V8ScV8Sci", "n")
BUILTIN(__builtin_neon_vst2q_lane_v, "vv*V16ScV16Scii", "n")
BUILTIN(__builtin_neon_vst2_lane_v, "vv*V8ScV8Scii", "n")
BUILTIN(__builtin_neon_vst3q_v, "vv*V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vst3_v, "vv*V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vst3q_lane_v, "vv*V16ScV16ScV16Scii", "n")
BUILTIN(__builtin_neon_vst3_lane_v, "vv*V8ScV8ScV8Scii", "n")
BUILTIN(__builtin_neon_vst4q_v, "vv*V16ScV16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vst4_v, "vv*V8ScV8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vst4q_lane_v, "vv*V16ScV16ScV16ScV16Scii", "n")
BUILTIN(__builtin_neon_vst4_lane_v, "vv*V8ScV8ScV8ScV8Scii", "n")
BUILTIN(__builtin_neon_vsubhn_v, "V8ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vtbl1_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vtbl2_v, "V8ScV8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vtbl3_v, "V8ScV8ScV8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vtbl4_v, "V8ScV8ScV8ScV8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vtbx1_v, "V8ScV8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vtbx2_v, "V8ScV8ScV8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vtbx3_v, "V8ScV8ScV8ScV8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vtbx4_v, "V8ScV8ScV8ScV8ScV8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vtrn_v, "vv*V8ScV8Sci", "n")
BUILTIN(__builtin_neon_vtrnq_v, "vv*V16ScV16Sci", "n")
BUILTIN(__builtin_neon_vtst_v, "V8ScV8ScV8Sci", "n")
BUILTIN(__builtin_neon_vtstq_v, "V16ScV16ScV16Sci", "n")
BUILTIN(__builtin_neon_vuzp_v, "vv*V8ScV8Sci", "n")
BUILTIN(__builtin_neon_vuzpq_v, "vv*V16ScV16Sci", "n")
BUILTIN(__builtin_neon_vzip_v, "vv*V8ScV8Sci", "n")
BUILTIN(__builtin_neon_vzipq_v, "vv*V16ScV16Sci", "n")
#endif

#ifdef GET_NEON_OVERLOAD_CHECK
case ARM::BI__builtin_neon_vabd_v: mask = 0x70087ULL; break;
case ARM::BI__builtin_neon_vabdq_v: mask = 0x7008700000000ULL; break;
case ARM::BI__builtin_neon_vabs_v: mask = 0x87ULL; break;
case ARM::BI__builtin_neon_vabsq_v: mask = 0x8700000000ULL; break;
case ARM::BI__builtin_neon_vaddhn_v: mask = 0x70007ULL; break;
case ARM::BI__builtin_neon_vbsl_v: mask = 0xF00BFULL; break;
case ARM::BI__builtin_neon_vbslq_v: mask = 0xF00BF00000000ULL; break;
case ARM::BI__builtin_neon_vcage_v: mask = 0x40000ULL; break;
case ARM::BI__builtin_neon_vcageq_v: mask = 0x4000000000000ULL; break;
case ARM::BI__builtin_neon_vcagt_v: mask = 0x40000ULL; break;
case ARM::BI__builtin_neon_vcagtq_v: mask = 0x4000000000000ULL; break;
case ARM::BI__builtin_neon_vcale_v: mask = 0x40000ULL; break;
case ARM::BI__builtin_neon_vcaleq_v: mask = 0x4000000000000ULL; break;
case ARM::BI__builtin_neon_vcalt_v: mask = 0x40000ULL; break;
case ARM::BI__builtin_neon_vcaltq_v: mask = 0x4000000000000ULL; break;
case ARM::BI__builtin_neon_vcls_v: mask = 0x7ULL; break;
case ARM::BI__builtin_neon_vclsq_v: mask = 0x700000000ULL; break;
case ARM::BI__builtin_neon_vclz_v: mask = 0x70007ULL; break;
case ARM::BI__builtin_neon_vclzq_v: mask = 0x7000700000000ULL; break;
case ARM::BI__builtin_neon_vcnt_v: mask = 0x10011ULL; break;
case ARM::BI__builtin_neon_vcntq_v: mask = 0x1001100000000ULL; break;
case ARM::BI__builtin_neon_vcvt_f16_v: mask = 0x40ULL; break;
case ARM::BI__builtin_neon_vcvt_f32_v: mask = 0x40004ULL; break;
case ARM::BI__builtin_neon_vcvtq_f32_v: mask = 0x4000400000000ULL; break;
case ARM::BI__builtin_neon_vcvt_f32_f16: mask = 0x40ULL; break;
case ARM::BI__builtin_neon_vcvt_n_f32_v: mask = 0x40004ULL; break;
case ARM::BI__builtin_neon_vcvtq_n_f32_v: mask = 0x4000400000000ULL; break;
case ARM::BI__builtin_neon_vcvt_n_s32_v: mask = 0x4ULL; break;
case ARM::BI__builtin_neon_vcvtq_n_s32_v: mask = 0x400000000ULL; break;
case ARM::BI__builtin_neon_vcvt_n_u32_v: mask = 0x40000ULL; break;
case ARM::BI__builtin_neon_vcvtq_n_u32_v: mask = 0x4000000000000ULL; break;
case ARM::BI__builtin_neon_vcvt_s32_v: mask = 0x4ULL; break;
case ARM::BI__builtin_neon_vcvtq_s32_v: mask = 0x400000000ULL; break;
case ARM::BI__builtin_neon_vcvt_u32_v: mask = 0x40000ULL; break;
case ARM::BI__builtin_neon_vcvtq_u32_v: mask = 0x4000000000000ULL; break;
case ARM::BI__builtin_neon_vext_v: mask = 0xF00BFULL; break;
case ARM::BI__builtin_neon_vextq_v: mask = 0xF00BF00000000ULL; break;
case ARM::BI__builtin_neon_vfma_v: mask = 0x80ULL; break;
case ARM::BI__builtin_neon_vfmaq_v: mask = 0x8000000000ULL; break;
case ARM::BI__builtin_neon_vhadd_v: mask = 0x70007ULL; break;
case ARM::BI__builtin_neon_vhaddq_v: mask = 0x7000700000000ULL; break;
case ARM::BI__builtin_neon_vhsub_v: mask = 0x70007ULL; break;
case ARM::BI__builtin_neon_vhsubq_v: mask = 0x7000700000000ULL; break;
case ARM::BI__builtin_neon_vld1_v: mask = 0xF00FFULL; PtrArgNum = 0; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vld1q_v: mask = 0xF00FF00000000ULL; PtrArgNum = 0; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vld1_dup_v: mask = 0xF00FFULL; break;
case ARM::BI__builtin_neon_vld1q_dup_v: mask = 0xF00FF00000000ULL; break;
case ARM::BI__builtin_neon_vld1_lane_v: mask = 0xF00FFULL; break;
case ARM::BI__builtin_neon_vld1q_lane_v: mask = 0xF00FF00000000ULL; break;
case ARM::BI__builtin_neon_vld2_v: mask = 0xF00FFULL; PtrArgNum = 1; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vld2q_v: mask = 0x700F700000000ULL; PtrArgNum = 1; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vld2_dup_v: mask = 0xF00FFULL; PtrArgNum = 1; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vld2_lane_v: mask = 0x700F7ULL; PtrArgNum = 1; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vld2q_lane_v: mask = 0x600E600000000ULL; PtrArgNum = 1; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vld3_v: mask = 0xF00FFULL; PtrArgNum = 1; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vld3q_v: mask = 0x700F700000000ULL; PtrArgNum = 1; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vld3_dup_v: mask = 0xF00FFULL; PtrArgNum = 1; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vld3_lane_v: mask = 0x700F7ULL; PtrArgNum = 1; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vld3q_lane_v: mask = 0x600E600000000ULL; PtrArgNum = 1; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vld4_v: mask = 0xF00FFULL; PtrArgNum = 1; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vld4q_v: mask = 0x700F700000000ULL; PtrArgNum = 1; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vld4_dup_v: mask = 0xF00FFULL; PtrArgNum = 1; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vld4_lane_v: mask = 0x700F7ULL; PtrArgNum = 1; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vld4q_lane_v: mask = 0x600E600000000ULL; PtrArgNum = 1; HasConstPtr = true; break;
case ARM::BI__builtin_neon_vmax_v: mask = 0x70087ULL; break;
case ARM::BI__builtin_neon_vmaxq_v: mask = 0x7008700000000ULL; break;
case ARM::BI__builtin_neon_vmin_v: mask = 0x70087ULL; break;
case ARM::BI__builtin_neon_vminq_v: mask = 0x7008700000000ULL; break;
case ARM::BI__builtin_neon_vmovl_v: mask = 0xE000E00000000ULL; break;
case ARM::BI__builtin_neon_vmovn_v: mask = 0x70007ULL; break;
case ARM::BI__builtin_neon_vmull_v: mask = 0xE002E00000000ULL; break;
case ARM::BI__builtin_neon_vmul_v: mask = 0x10ULL; break;
case ARM::BI__builtin_neon_vmulq_v: mask = 0x1000000000ULL; break;
case ARM::BI__builtin_neon_vpadal_v: mask = 0xE000EULL; break;
case ARM::BI__builtin_neon_vpadalq_v: mask = 0xE000E00000000ULL; break;
case ARM::BI__builtin_neon_vpadd_v: mask = 0x70087ULL; break;
case ARM::BI__builtin_neon_vpaddl_v: mask = 0xE000EULL; break;
case ARM::BI__builtin_neon_vpaddlq_v: mask = 0xE000E00000000ULL; break;
case ARM::BI__builtin_neon_vpmax_v: mask = 0x70087ULL; break;
case ARM::BI__builtin_neon_vpmin_v: mask = 0x70087ULL; break;
case ARM::BI__builtin_neon_vqabs_v: mask = 0x7ULL; break;
case ARM::BI__builtin_neon_vqabsq_v: mask = 0x700000000ULL; break;
case ARM::BI__builtin_neon_vqadd_v: mask = 0xF000FULL; break;
case ARM::BI__builtin_neon_vqaddq_v: mask = 0xF000F00000000ULL; break;
case ARM::BI__builtin_neon_vqdmlal_v: mask = 0xC00000000ULL; break;
case ARM::BI__builtin_neon_vqdmlsl_v: mask = 0xC00000000ULL; break;
case ARM::BI__builtin_neon_vqdmulh_v: mask = 0x6ULL; break;
case ARM::BI__builtin_neon_vqdmulhq_v: mask = 0x600000000ULL; break;
case ARM::BI__builtin_neon_vqdmull_v: mask = 0xC00000000ULL; break;
case ARM::BI__builtin_neon_vqmovn_v: mask = 0x70007ULL; break;
case ARM::BI__builtin_neon_vqmovun_v: mask = 0x70000ULL; break;
case ARM::BI__builtin_neon_vqneg_v: mask = 0x7ULL; break;
case ARM::BI__builtin_neon_vqnegq_v: mask = 0x700000000ULL; break;
case ARM::BI__builtin_neon_vqrdmulh_v: mask = 0x6ULL; break;
case ARM::BI__builtin_neon_vqrdmulhq_v: mask = 0x600000000ULL; break;
case ARM::BI__builtin_neon_vqrshl_v: mask = 0xF000FULL; break;
case ARM::BI__builtin_neon_vqrshlq_v: mask = 0xF000F00000000ULL; break;
case ARM::BI__builtin_neon_vqrshrn_n_v: mask = 0x70007ULL; break;
case ARM::BI__builtin_neon_vqrshrun_n_v: mask = 0x70000ULL; break;
case ARM::BI__builtin_neon_vqshl_v: mask = 0xF000FULL; break;
case ARM::BI__builtin_neon_vqshlq_v: mask = 0xF000F00000000ULL; break;
case ARM::BI__builtin_neon_vqshlu_n_v: mask = 0xF0000ULL; break;
case ARM::BI__builtin_neon_vqshluq_n_v: mask = 0xF000000000000ULL; break;
case ARM::BI__builtin_neon_vqshl_n_v: mask = 0xF000FULL; break;
case ARM::BI__builtin_neon_vqshlq_n_v: mask = 0xF000F00000000ULL; break;
case ARM::BI__builtin_neon_vqshrn_n_v: mask = 0x70007ULL; break;
case ARM::BI__builtin_neon_vqshrun_n_v: mask = 0x70000ULL; break;
case ARM::BI__builtin_neon_vqsub_v: mask = 0xF000FULL; break;
case ARM::BI__builtin_neon_vqsubq_v: mask = 0xF000F00000000ULL; break;
case ARM::BI__builtin_neon_vraddhn_v: mask = 0x70007ULL; break;
case ARM::BI__builtin_neon_vrecpe_v: mask = 0x40080ULL; break;
case ARM::BI__builtin_neon_vrecpeq_v: mask = 0x4008000000000ULL; break;
case ARM::BI__builtin_neon_vrecps_v: mask = 0x80ULL; break;
case ARM::BI__builtin_neon_vrecpsq_v: mask = 0x8000000000ULL; break;
case ARM::BI__builtin_neon_vrhadd_v: mask = 0x70007ULL; break;
case ARM::BI__builtin_neon_vrhaddq_v: mask = 0x7000700000000ULL; break;
case ARM::BI__builtin_neon_vrshl_v: mask = 0xF000FULL; break;
case ARM::BI__builtin_neon_vrshlq_v: mask = 0xF000F00000000ULL; break;
case ARM::BI__builtin_neon_vrshrn_n_v: mask = 0x70007ULL; break;
case ARM::BI__builtin_neon_vrshr_n_v: mask = 0xF000FULL; break;
case ARM::BI__builtin_neon_vrshrq_n_v: mask = 0xF000F00000000ULL; break;
case ARM::BI__builtin_neon_vrsqrte_v: mask = 0x40080ULL; break;
case ARM::BI__builtin_neon_vrsqrteq_v: mask = 0x4008000000000ULL; break;
case ARM::BI__builtin_neon_vrsqrts_v: mask = 0x80ULL; break;
case ARM::BI__builtin_neon_vrsqrtsq_v: mask = 0x8000000000ULL; break;
case ARM::BI__builtin_neon_vrsra_n_v: mask = 0xF000FULL; break;
case ARM::BI__builtin_neon_vrsraq_n_v: mask = 0xF000F00000000ULL; break;
case ARM::BI__builtin_neon_vrsubhn_v: mask = 0x70007ULL; break;
case ARM::BI__builtin_neon_vshl_v: mask = 0xF000FULL; break;
case ARM::BI__builtin_neon_vshlq_v: mask = 0xF000F00000000ULL; break;
case ARM::BI__builtin_neon_vshll_n_v: mask = 0xE000E00000000ULL; break;
case ARM::BI__builtin_neon_vshl_n_v: mask = 0xF000FULL; break;
case ARM::BI__builtin_neon_vshlq_n_v: mask = 0xF000F00000000ULL; break;
case ARM::BI__builtin_neon_vshrn_n_v: mask = 0x70007ULL; break;
case ARM::BI__builtin_neon_vshr_n_v: mask = 0xF000FULL; break;
case ARM::BI__builtin_neon_vshrq_n_v: mask = 0xF000F00000000ULL; break;
case ARM::BI__builtin_neon_vsli_n_v: mask = 0xF003FULL; break;
case ARM::BI__builtin_neon_vsliq_n_v: mask = 0xF003F00000000ULL; break;
case ARM::BI__builtin_neon_vsra_n_v: mask = 0xF000FULL; break;
case ARM::BI__builtin_neon_vsraq_n_v: mask = 0xF000F00000000ULL; break;
case ARM::BI__builtin_neon_vsri_n_v: mask = 0xF003FULL; break;
case ARM::BI__builtin_neon_vsriq_n_v: mask = 0xF003F00000000ULL; break;
case ARM::BI__builtin_neon_vst1_v: mask = 0xF00FFULL; PtrArgNum = 0; break;
case ARM::BI__builtin_neon_vst1q_v: mask = 0xF00FF00000000ULL; PtrArgNum = 0; break;
case ARM::BI__builtin_neon_vst1_lane_v: mask = 0xF00FFULL; break;
case ARM::BI__builtin_neon_vst1q_lane_v: mask = 0xF00FF00000000ULL; break;
case ARM::BI__builtin_neon_vst2_v: mask = 0xF00FFULL; PtrArgNum = 0; break;
case ARM::BI__builtin_neon_vst2q_v: mask = 0x700F700000000ULL; PtrArgNum = 0; break;
case ARM::BI__builtin_neon_vst2_lane_v: mask = 0x700F7ULL; PtrArgNum = 0; break;
case ARM::BI__builtin_neon_vst2q_lane_v: mask = 0x600E600000000ULL; PtrArgNum = 0; break;
case ARM::BI__builtin_neon_vst3_v: mask = 0xF00FFULL; PtrArgNum = 0; break;
case ARM::BI__builtin_neon_vst3q_v: mask = 0x700F700000000ULL; PtrArgNum = 0; break;
case ARM::BI__builtin_neon_vst3_lane_v: mask = 0x700F7ULL; PtrArgNum = 0; break;
case ARM::BI__builtin_neon_vst3q_lane_v: mask = 0x600E600000000ULL; PtrArgNum = 0; break;
case ARM::BI__builtin_neon_vst4_v: mask = 0xF00FFULL; PtrArgNum = 0; break;
case ARM::BI__builtin_neon_vst4q_v: mask = 0x700F700000000ULL; PtrArgNum = 0; break;
case ARM::BI__builtin_neon_vst4_lane_v: mask = 0x700F7ULL; PtrArgNum = 0; break;
case ARM::BI__builtin_neon_vst4q_lane_v: mask = 0x600E600000000ULL; PtrArgNum = 0; break;
case ARM::BI__builtin_neon_vsubhn_v: mask = 0x70007ULL; break;
case ARM::BI__builtin_neon_vtbl1_v: mask = 0x10011ULL; break;
case ARM::BI__builtin_neon_vtbl2_v: mask = 0x10011ULL; break;
case ARM::BI__builtin_neon_vtbl3_v: mask = 0x10011ULL; break;
case ARM::BI__builtin_neon_vtbl4_v: mask = 0x10011ULL; break;
case ARM::BI__builtin_neon_vtbx1_v: mask = 0x10011ULL; break;
case ARM::BI__builtin_neon_vtbx2_v: mask = 0x10011ULL; break;
case ARM::BI__builtin_neon_vtbx3_v: mask = 0x10011ULL; break;
case ARM::BI__builtin_neon_vtbx4_v: mask = 0x10011ULL; break;
case ARM::BI__builtin_neon_vtrn_v: mask = 0x700B7ULL; break;
case ARM::BI__builtin_neon_vtrnq_v: mask = 0x700B700000000ULL; break;
case ARM::BI__builtin_neon_vtst_v: mask = 0x70000ULL; break;
case ARM::BI__builtin_neon_vtstq_v: mask = 0x7000000000000ULL; break;
case ARM::BI__builtin_neon_vuzp_v: mask = 0x700B7ULL; break;
case ARM::BI__builtin_neon_vuzpq_v: mask = 0x700B700000000ULL; break;
case ARM::BI__builtin_neon_vzip_v: mask = 0x700B7ULL; break;
case ARM::BI__builtin_neon_vzipq_v: mask = 0x700B700000000ULL; break;
#endif

#ifdef GET_NEON_IMMEDIATE_CHECK
case ARM::BI__builtin_neon_vcvt_n_f32_v: i = 1; l = 1; u = 31; break;
case ARM::BI__builtin_neon_vcvtq_n_f32_v: i = 1; l = 1; u = 31; break;
case ARM::BI__builtin_neon_vcvt_n_s32_v: i = 1; l = 1; u = 31; break;
case ARM::BI__builtin_neon_vcvtq_n_s32_v: i = 1; l = 1; u = 31; break;
case ARM::BI__builtin_neon_vcvt_n_u32_v: i = 1; l = 1; u = 31; break;
case ARM::BI__builtin_neon_vcvtq_n_u32_v: i = 1; l = 1; u = 31; break;
case ARM::BI__builtin_neon_vext_v: i = 2; u = RFT(TV); break;
case ARM::BI__builtin_neon_vextq_v: i = 2; u = RFT(TV); break;
case ARM::BI__builtin_neon_vget_lane_i8: i = 1; u = 7; break;
case ARM::BI__builtin_neon_vget_lane_i16: i = 1; u = 3; break;
case ARM::BI__builtin_neon_vget_lane_i32: i = 1; u = 1; break;
case ARM::BI__builtin_neon_vget_lane_f32: i = 1; u = 1; break;
case ARM::BI__builtin_neon_vgetq_lane_i8: i = 1; u = 15; break;
case ARM::BI__builtin_neon_vgetq_lane_i16: i = 1; u = 7; break;
case ARM::BI__builtin_neon_vgetq_lane_i32: i = 1; u = 3; break;
case ARM::BI__builtin_neon_vgetq_lane_f32: i = 1; u = 3; break;
case ARM::BI__builtin_neon_vget_lane_i64: i = 1; u = 0; break;
case ARM::BI__builtin_neon_vgetq_lane_i64: i = 1; u = 1; break;
case ARM::BI__builtin_neon_vld1q_lane_v: i = 2; u = RFT(TV); break;
case ARM::BI__builtin_neon_vld1_lane_v: i = 2; u = RFT(TV); break;
case ARM::BI__builtin_neon_vld2q_lane_v: i = 4; u = RFT(TV); break;
case ARM::BI__builtin_neon_vld2_lane_v: i = 4; u = RFT(TV); break;
case ARM::BI__builtin_neon_vld3q_lane_v: i = 5; u = RFT(TV); break;
case ARM::BI__builtin_neon_vld3_lane_v: i = 5; u = RFT(TV); break;
case ARM::BI__builtin_neon_vld4q_lane_v: i = 6; u = RFT(TV); break;
case ARM::BI__builtin_neon_vld4_lane_v: i = 6; u = RFT(TV); break;
case ARM::BI__builtin_neon_vqrshrn_n_v: i = 1; l = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vqrshrun_n_v: i = 1; l = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vqshlu_n_v: i = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vqshluq_n_v: i = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vqshl_n_v: i = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vqshlq_n_v: i = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vqshrn_n_v: i = 1; l = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vqshrun_n_v: i = 1; l = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vrshrn_n_v: i = 1; l = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vrshr_n_v: i = 1; l = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vrshrq_n_v: i = 1; l = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vrsra_n_v: i = 2; l = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vrsraq_n_v: i = 2; l = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vset_lane_i8: i = 2; u = 7; break;
case ARM::BI__builtin_neon_vset_lane_i16: i = 2; u = 3; break;
case ARM::BI__builtin_neon_vset_lane_i32: i = 2; u = 1; break;
case ARM::BI__builtin_neon_vset_lane_f32: i = 2; u = 1; break;
case ARM::BI__builtin_neon_vsetq_lane_i8: i = 2; u = 15; break;
case ARM::BI__builtin_neon_vsetq_lane_i16: i = 2; u = 7; break;
case ARM::BI__builtin_neon_vsetq_lane_i32: i = 2; u = 3; break;
case ARM::BI__builtin_neon_vsetq_lane_f32: i = 2; u = 3; break;
case ARM::BI__builtin_neon_vset_lane_i64: i = 2; u = 0; break;
case ARM::BI__builtin_neon_vsetq_lane_i64: i = 2; u = 1; break;
case ARM::BI__builtin_neon_vshll_n_v: i = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vshl_n_v: i = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vshlq_n_v: i = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vshrn_n_v: i = 1; l = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vshr_n_v: i = 1; l = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vshrq_n_v: i = 1; l = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vsli_n_v: i = 2; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vsliq_n_v: i = 2; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vsra_n_v: i = 2; l = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vsraq_n_v: i = 2; l = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vsri_n_v: i = 2; l = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vsriq_n_v: i = 2; l = 1; u = RFT(TV, true); break;
case ARM::BI__builtin_neon_vst1q_lane_v: i = 2; u = RFT(TV); break;
case ARM::BI__builtin_neon_vst1_lane_v: i = 2; u = RFT(TV); break;
case ARM::BI__builtin_neon_vst2q_lane_v: i = 3; u = RFT(TV); break;
case ARM::BI__builtin_neon_vst2_lane_v: i = 3; u = RFT(TV); break;
case ARM::BI__builtin_neon_vst3q_lane_v: i = 4; u = RFT(TV); break;
case ARM::BI__builtin_neon_vst3_lane_v: i = 4; u = RFT(TV); break;
case ARM::BI__builtin_neon_vst4q_lane_v: i = 5; u = RFT(TV); break;
case ARM::BI__builtin_neon_vst4_lane_v: i = 5; u = RFT(TV); break;
#endif

