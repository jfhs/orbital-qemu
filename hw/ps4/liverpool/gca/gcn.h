/*
 * AMD GCN bytecode common definitions
 *
 * Copyright (c) 2019 Alexandro Sanchez Bach
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_PS4_LIVERPOOL_GCA_GCN_H
#define HW_PS4_LIVERPOOL_GCA_GCN_H

#include <stdbool.h>
#include <stdint.h>

typedef enum gcn_stage_t {
    GCN_STAGE_PS = 0,
    GCN_STAGE_VS = 1,
    GCN_STAGE_GS = 2,
    GCN_STAGE_ES = 3,
    GCN_STAGE_HS = 4,
    GCN_STAGE_LS = 5,
    GCN_STAGE_CS = 6,
} gcn_stage_t;

/* opcodes */

enum gcn_opcode_sop2_t {
    S_ADD_U32                   = 0,
    S_SUB_U32                   = 1,
    S_ADD_I32                   = 2,
    S_SUB_I32                   = 3,
    S_ADDC_U32                  = 4,
    S_SUBB_U32                  = 5,
    S_MIN_I32                   = 6,
    S_MIN_U32                   = 7,
    S_MAX_I32                   = 8,
    S_MAX_U32                   = 9,
    S_CSELECT_B32               = 10,
    S_CSELECT_B64               = 11,
    S_AND_B32                   = 12,
    S_AND_B64                   = 13,
    S_OR_B32                    = 14,
    S_OR_B64                    = 15,
    S_XOR_B32                   = 16,
    S_XOR_B64                   = 17,
    S_ANDN2_B32                 = 18,
    S_ANDN2_B64                 = 19,
    S_ORN2_B32                  = 20,
    S_ORN2_B64                  = 21,
    S_NAND_B32                  = 22,
    S_NAND_B64                  = 23,
    S_NOR_B32                   = 24,
    S_NOR_B64                   = 25,
    S_XNOR_B32                  = 26,
    S_XNOR_B64                  = 27,
    S_LSHL_B32                  = 28,
    S_LSHL_B64                  = 29,
    S_LSHR_B32                  = 30,
    S_LSHR_B64                  = 31,
    S_ASHR_I32                  = 32,
    S_ASHR_I64                  = 33,
    S_BFM_B32                   = 34,
    S_BFM_B64                   = 35,
    S_MUL_I32                   = 36,
    S_BFE_U32                   = 37,
    S_BFE_I32                   = 38,
    S_BFE_U64                   = 39,
    S_BFE_I64                   = 40,
};

enum gcn_opcode_sopk_t {
    S_MOVK_I32                  = 0,
    S_CMOVK_I32                 = 1,
    S_CMPK_EQ_I32               = 2,
    S_CMPK_LG_I32               = 3,
    S_CMPK_GT_I32               = 4,
    S_CMPK_GE_I32               = 5,
    S_CMPK_LT_I32               = 6,
    S_CMPK_LE_I32               = 7,
    S_CMPK_EQ_U32               = 8,
    S_CMPK_LG_U32               = 9,
    S_CMPK_GT_U32               = 10,
    S_CMPK_GE_U32               = 11,
    S_CMPK_LT_U32               = 12,
    S_CMPK_LE_U32               = 13,
    S_ADDK_I32                  = 14,
    S_MULK_I32                  = 15,
    S_CBRANCH_I_FORK            = 16,
    S_GETREG_B32                = 17,
    S_SETREG_B32                = 18,
    S_SETREG_IMM32_B32          = 20,
    S_CALL_B64                  = 21,
};

enum gcn_opcode_sop1_t {
    S_MOV_B32                   = 0,
    S_MOV_B64                   = 1,
    S_CMOV_B32                  = 2,
    S_CMOV_B64                  = 3,
    S_NOT_B32                   = 4,
    S_NOT_B64                   = 5,
    S_WQM_B32                   = 6,
    S_WQM_B64                   = 7,
    S_BREV_B32                  = 8,
    S_BREV_B64                  = 9,
    S_BCNT0_I32_B32             = 10,
    S_BCNT0_I32_B64             = 11,
    S_BCNT1_I32_B32             = 12,
    S_BCNT1_I32_B64             = 13,
    S_FF0_I32_B32               = 14,
    S_FF0_I32_B64               = 15,
    S_FF1_I32_B32               = 16,
    S_FF1_I32_B64               = 17,
    S_FLBIT_I32_B32             = 18,
    S_FLBIT_I32_B64             = 19,
    S_FLBIT_I32                 = 20,
    S_FLBIT_I32_I64             = 21,
    S_SEXT_I32_I8               = 22,
    S_SEXT_I32_I16              = 23,
    S_BITSET0_B32               = 24,
    S_BITSET0_B64               = 25,
    S_BITSET1_B32               = 26,
    S_BITSET1_B64               = 27,
    S_GETPC_B64                 = 28,
    S_SETPC_B64                 = 29,
    S_SWAPPC_B64                = 30,
    S_RFE_B64                   = 31,
    S_AND_SAVEEXEC_B64          = 32,
    S_OR_SAVEEXEC_B64           = 33,
    S_XOR_SAVEEXEC_B64          = 34,
    S_ANDN2_SAVEEXEC_B64        = 35,
    S_ORN2_SAVEEXEC_B64         = 36,
    S_NAND_SAVEEXEC_B64         = 37,
    S_NOR_SAVEEXEC_B64          = 38,
    S_XNOR_SAVEEXEC_B64         = 39,
    S_QUADMASK_B32              = 40,
    S_QUADMASK_B64              = 41,
    S_MOVRELS_B32               = 42,
    S_MOVRELS_B64               = 43,
    S_MOVRELD_B32               = 44,
    S_MOVRELD_B64               = 45,
    S_CBRANCH_JOIN              = 46,
    S_ABS_I32                   = 48,
    S_SET_GPR_IDX_IDX           = 50,
    S_ANDN1_SAVEEXEC_B64        = 51,
    S_ORN1_SAVEEXEC_B64         = 52,
    S_ANDN1_WREXEC_B64          = 53,
    S_ANDN2_WREXEC_B64          = 54,
    S_BITREPLICATE_B64_B32      = 55,
};

enum gcn_opcode_sopp_t {
    S_NOP                       = 0,
    S_ENDPGM                    = 1,
    S_BRANCH                    = 2,
    S_CBRANCH_SCC0              = 4,
    S_CBRANCH_SCC1              = 5,
    S_CBRANCH_VCCZ              = 6,
    S_CBRANCH_VCCNZ             = 7,
    S_CBRANCH_EXECZ             = 8,
    S_CBRANCH_EXECNZ            = 9,
    S_BARRIER                   = 10,
    S_SETKILL                   = 11,
    S_WAITCNT                   = 12,
    S_SETHALT                   = 13,
    S_SLEEP                     = 14,
    S_SETPRIO                   = 15,
    S_SENDMSG                   = 16,
    S_SENDMSGHALT               = 17,
    S_TRAP                      = 18,
    S_ICACHE_INV                = 19,
    S_INCPERFLEVEL              = 20,
    S_DECPERFLEVEL              = 21,
    S_TTRACEDATA                = 22,
    S_CBRANCH_CDBGSYS           = 23,
    S_CBRANCH_CDBGUSER          = 24,
    S_CBRANCH_CDBGSYS_OR_USER   = 25,
    S_CBRANCH_CDBGSYS_AND_USER  = 26,
};

enum gcn_opcode_vop2_t {
    V_CNDMASK_B32               = 0,
    V_READLANE_B32              = 1,
    V_WRITELANE_B32             = 2,
    V_ADD_F32                   = 3,
    V_SUB_F32                   = 4,
    V_SUBREV_F32                = 5,
    V_MAC_LEGACY_F32            = 6,
    V_MUL_LEGACY_F32            = 7,
    V_MUL_F32                   = 8,
    V_MUL_I32_I24               = 9,
    V_MUL_HI_I32_I24            = 10,
    V_MUL_U32_U24               = 11,
    V_MUL_HI_U32_U24            = 12,
    V_MIN_LEGACY_F32            = 13,
    V_MAX_LEGACY_F32            = 14,
    V_MIN_F32                   = 15,
    V_MAX_F32                   = 16,
    V_MIN_I32                   = 17,
    V_MAX_I32                   = 18,
    V_MIN_U32                   = 19,
    V_MAX_U32                   = 20,
    V_LSHR_B32                  = 21,
    V_LSHRREV_B32               = 22,
    V_ASHR_I32                  = 23,
    V_ASHRREV_I32               = 24,
    V_LSHL_B32                  = 25,
    V_LSHLREV_B32               = 26,
    V_AND_B32                   = 27,
    V_OR_B32                    = 28,
    V_XOR_B32                   = 29,
    V_BFM_B32                   = 30,
    V_MAC_F32                   = 31,
    V_MADMK_F32                 = 32,
    V_MADAK_F32                 = 33,
    V_BCNT_U32_B32              = 34,
    V_MBCNT_LO_U32_B32          = 35,
    V_MBCNT_HI_U32_B32          = 36,
    V_ADD_I32                   = 37,
    V_SUB_I32                   = 38,
    V_SUBREV_I32                = 39,
    V_ADDC_U32                  = 40,
    V_SUBB_U32                  = 41,
    V_SUBBREV_U32               = 42,
    V_LDEXP_F32                 = 43,
    V_CVT_PKACCUM_U8_F32        = 44,
    V_CVT_PKNORM_I16_F32        = 45,
    V_CVT_PKNORM_U16_F32        = 46,
    V_CVT_PKRTZ_F16_F32         = 47,
    V_CVT_PK_U16_U32            = 48,
    V_CVT_PK_I16_I32            = 49,
};

enum gcn_opcode_vop1_t {
    V_NOP                       = 0,
    V_MOV_B32                   = 1,
    V_READFIRSTLANE_B32         = 2,
    V_CVT_I32_F64               = 3,
    V_CVT_F64_I32               = 4,
    V_CVT_F32_I32               = 5,
    V_CVT_F32_U32               = 6,
    V_CVT_U32_F32               = 7,
    V_CVT_I32_F32               = 8,
    V_MOV_FED_B32               = 9,
    V_CVT_F16_F32               = 10,
    V_CVT_F32_F16               = 11,
    V_CVT_RPI_I32_F32           = 12,
    V_CVT_FLR_I32_F32           = 13,
    V_CVT_OFF_F32_I4            = 14,
    V_CVT_F32_F64               = 15,
    V_CVT_F64_F32               = 16,
    V_CVT_F32_UBYTE0            = 17,
    V_CVT_F32_UBYTE1            = 18,
    V_CVT_F32_UBYTE2            = 19,
    V_CVT_F32_UBYTE3            = 20,
    V_CVT_U32_F64               = 21,
    V_CVT_F64_U32               = 22,
    V_TRUNC_F64                 = 23,
    V_CEIL_F64                  = 24,
    V_RNDNE_F64                 = 25,
    V_FLOOR_F64                 = 26,
    V_FRACT_F32                 = 32,
    V_TRUNC_F32                 = 33,
    V_CEIL_F32                  = 34,
    V_RNDNE_F32                 = 35,
    V_FLOOR_F32                 = 36,
    V_EXP_F32                   = 37,
    V_LOG_CLAMP_F32             = 38,
    V_LOG_F32                   = 39,
    V_RCP_CLAMP_F32             = 40,
    V_RCP_LEGACY_F32            = 41,
    V_RCP_F32                   = 42,
    V_RCP_IFLAG_F32             = 43,
    V_RSQ_CLAMP_F32             = 44,
    V_RSQ_LEGACY_F32            = 45,
    V_RSQ_F32                   = 46,
    V_RCP_F64                   = 47,
    V_RCP_CLAMP_F64             = 48,
    V_RSQ_F64                   = 49,
    V_RSQ_CLAMP_F64             = 50,
    V_SQRT_F32                  = 51,
    V_SQRT_F64                  = 52,
    V_SIN_F32                   = 53,
    V_COS_F32                   = 54,
    V_NOT_B32                   = 55,
    V_BFREV_B32                 = 56,
    V_FFBH_U32                  = 57,
    V_FFBL_B32                  = 58,
    V_FFBH_I32                  = 59,
    V_FREXP_EXP_I32_F64         = 60,
    V_FREXP_MANT_F64            = 61,
    V_FRACT_F64                 = 62,
    V_FREXP_EXP_I32_F32         = 63,
    V_FREXP_MANT_F32            = 64,
    V_CLREXCP                   = 65,
    V_MOVRELD_B32               = 66,
    V_MOVRELS_B32               = 67,
    V_MOVRELSD_B32              = 68,
    V_LOG_LEGACY_F32            = 69,
    V_EXP_LEGACY_F32            = 70,
};

enum gcn_opcode_vintrp_t {
    V_INTERP_P1_F32             = 0,
    V_INTERP_P2_F32             = 1,
    V_INTERP_MOV_F32            = 2,
};

enum gcn_opcode_vop3a_t {
    // Remappings
    // - Remapping of VOPC opcodes at 0x000-0x0FF (see: gcn_opcode_vopc_t)
    // - Remapping of VOP2 opcodes at 0x100-0x13F (see: gcn_opcode_vop2_t)
    // - Remapping of VOP1 opcodes at 0x180-0x1FF (see: gcn_opcode_vop1_t)

    // Opcodes
    // - Exclusive VOP3a opcodes at 0x140-0x17F
    V_MAD_LEGACY_F32            = 320 - 0x140,
    V_MAD_F32                   = 321 - 0x140,
    V_MAD_I32_I24               = 322 - 0x140,
    V_MAD_U32_U24               = 323 - 0x140,
    V_CUBEID_F32                = 324 - 0x140,
    V_CUBESC_F32                = 325 - 0x140,
    V_CUBETC_F32                = 326 - 0x140,
    V_CUBEMA_F32                = 327 - 0x140,
    V_BFE_U32                   = 328 - 0x140,
    V_BFE_I32                   = 329 - 0x140,
    V_BFI_B32                   = 330 - 0x140,
    V_FMA_F32                   = 331 - 0x140,
    V_FMA_F64                   = 332 - 0x140,
    V_LERP_U8                   = 333 - 0x140,
    V_ALIGNBIT_B32              = 334 - 0x140,
    V_ALIGNBYTE_B32             = 335 - 0x140,
    V_MULLIT_F32                = 336 - 0x140,
    V_MIN3_F32                  = 337 - 0x140,
    V_MIN3_I32                  = 338 - 0x140,
    V_MIN3_U32                  = 339 - 0x140,
    V_MAX3_F32                  = 340 - 0x140,
    V_MAX3_I32                  = 341 - 0x140,
    V_MAX3_U32                  = 342 - 0x140,
    V_MED3_F32                  = 343 - 0x140,
    V_MED3_I32                  = 344 - 0x140,
    V_MED3_U32                  = 345 - 0x140,
    V_SAD_U8                    = 346 - 0x140,
    V_SAD_HI_U8                 = 347 - 0x140,
    V_SAD_U16                   = 348 - 0x140,
    V_SAD_U32                   = 349 - 0x140,
    V_CVT_PK_U8_F32             = 350 - 0x140,
    V_DIV_FIXUP_F32             = 351 - 0x140,
    V_DIV_FIXUP_F64             = 352 - 0x140,
    V_LSHL_B64                  = 353 - 0x140,
    V_LSHR_B64                  = 354 - 0x140,
    V_ASHR_I64                  = 355 - 0x140,
    V_ADD_F64                   = 356 - 0x140,
    V_MUL_F64                   = 357 - 0x140,
    V_MIN_F64                   = 358 - 0x140,
    V_MAX_F64                   = 359 - 0x140,
    V_LDEXP_F64                 = 360 - 0x140,
    V_MUL_LO_U32                = 361 - 0x140,
    V_MUL_HI_U32                = 362 - 0x140,
    V_MUL_LO_I32                = 363 - 0x140,
    V_MUL_HI_I32                = 364 - 0x140,
    V_DIV_FMAS_F32              = 367 - 0x140,
    V_DIV_FMAS_F64              = 368 - 0x140,
    V_MSAD_U8                   = 369 - 0x140,
    V_QSAD_PK_U16_U8            = 370 - 0x140,
    V_MQSAD_PK_U16_U8           = 371 - 0x140,
    V_TRIG_PREOP_F64            = 372 - 0x140,
    V_MQSAD_U32_U8              = 373 - 0x140,
    V_MAD_U64_U32               = 374 - 0x140,
    V_MAD_I64_I32               = 375 - 0x140,
};

enum gcn_opcode_smrd_t {
    S_LOAD_DWORD                = 0,   // Read from read-only constant memory.
    S_LOAD_DWORDX2              = 1,   // Read from read-only constant memory.
    S_LOAD_DWORDX4              = 2,   // Read from read-only constant memory.
    S_LOAD_DWORDX8              = 3,   // Read from read-only constant memory.
    S_LOAD_DWORDX16             = 4,   // Read from read-only constant memory.
    S_BUFFER_LOAD_DWORD         = 8,   // Read from read-only constant memory.
    S_BUFFER_LOAD_DWORDX2       = 9,   // Read from read-only constant memory.
    S_BUFFER_LOAD_DWORDX4       = 10,  // Read from read-only constant memory.
    S_BUFFER_LOAD_DWORDX8       = 11,  // Read from read-only constant memory.
    S_BUFFER_LOAD_DWORDX16      = 12,  // Read from read-only constant memory.
    S_DCACHE_INV_VOL            = 29,  // Invalidate all volatile lines in L1 constant cache.
    S_MEMTIME                   = 30,  // Return current 64-bit timestamp.
    S_DCACHE_INV                = 31,  // Invalidate entire L1 K cache.
};

enum gcn_opcode_mimg_t {
    IMAGE_LOAD                  = 0,
    IMAGE_LOAD_MIP              = 1,
    IMAGE_LOAD_PCK              = 2,
    IMAGE_LOAD_PCK_SGN          = 3,
    IMAGE_LOAD_MIP_PCK          = 4,
    IMAGE_LOAD_MIP_PCK_SGN      = 5,
    IMAGE_STORE                 = 8,
    IMAGE_STORE_MIP             = 9,
    IMAGE_STORE_PCK             = 10,
    IMAGE_STORE_MIP_PCK         = 11,
    IMAGE_GET_RESINFO           = 14,
    IMAGE_ATOMIC_SWAP           = 15,
    IMAGE_ATOMIC_CMPSWAP        = 16,
    IMAGE_ATOMIC_ADD            = 17,
    IMAGE_ATOMIC_SUB            = 18,
    IMAGE_ATOMIC_SMIN           = 20,
    IMAGE_ATOMIC_UMIN           = 21,
    IMAGE_ATOMIC_SMAX           = 22,
    IMAGE_ATOMIC_UMAX           = 23,
    IMAGE_ATOMIC_AND            = 24,
    IMAGE_ATOMIC_OR             = 25,
    IMAGE_ATOMIC_XOR            = 26,
    IMAGE_ATOMIC_INC            = 27,
    IMAGE_ATOMIC_DEC            = 28,
    IMAGE_ATOMIC_FCMPSWAP       = 29,
    IMAGE_ATOMIC_FMIN           = 30,
    IMAGE_ATOMIC_FMAX           = 31,
    IMAGE_SAMPLE                = 32,
    IMAGE_SAMPLE_CL             = 33,
    IMAGE_SAMPLE_D              = 34,
    IMAGE_SAMPLE_D_CL           = 35,
    IMAGE_SAMPLE_L              = 36,
    IMAGE_SAMPLE_B              = 37,
    IMAGE_SAMPLE_B_CL           = 38,
    IMAGE_SAMPLE_LZ             = 39,
    IMAGE_SAMPLE_C              = 40,
    IMAGE_SAMPLE_C_CL           = 41,
    IMAGE_SAMPLE_C_D            = 42,
    IMAGE_SAMPLE_C_D_CL         = 43,
    IMAGE_SAMPLE_C_L            = 44,
    IMAGE_SAMPLE_C_B            = 45,
    IMAGE_SAMPLE_C_B_CL         = 46,
    IMAGE_SAMPLE_C_LZ           = 47,
    IMAGE_SAMPLE_O              = 48,
    IMAGE_SAMPLE_CL_O           = 49,
    IMAGE_SAMPLE_D_O            = 50,
    IMAGE_SAMPLE_D_CL_O         = 51,
    IMAGE_SAMPLE_L_O            = 52,
    IMAGE_SAMPLE_B_O            = 53,
    IMAGE_SAMPLE_B_CL_O         = 54,
    IMAGE_SAMPLE_LZ_O           = 55,
    IMAGE_SAMPLE_C_O            = 56,
    IMAGE_SAMPLE_C_CL_O         = 57,
    IMAGE_SAMPLE_C_D_O          = 58,
    IMAGE_SAMPLE_C_D_CL_O       = 59,
    IMAGE_SAMPLE_C_L_O          = 60,
    IMAGE_SAMPLE_C_B_O          = 61,
    IMAGE_SAMPLE_C_B_CL_O       = 62,
    IMAGE_SAMPLE_C_LZ_O         = 63,
    IMAGE_GATHER4               = 64,
    IMAGE_GATHER4_CL            = 65,
    IMAGE_GATHER4_L             = 66,
    IMAGE_GATHER4_B             = 67,
    IMAGE_GATHER4_B_CL          = 68,
    IMAGE_GATHER4_LZ            = 69,
    IMAGE_GATHER4_C             = 70,
    IMAGE_GATHER4_C_CL          = 71,
    IMAGE_GATHER4_C_L           = 76,
    IMAGE_GATHER4_C_B           = 77,
    IMAGE_GATHER4_C_B_CL        = 78,
    IMAGE_GATHER4_C_LZ          = 79,
    IMAGE_GATHER4_O             = 80,
    IMAGE_GATHER4_CL_O          = 81,
    IMAGE_GATHER4_L_O           = 84,
    IMAGE_GATHER4_B_O           = 85,
    IMAGE_GATHER4_B_CL_O        = 86,
    IMAGE_GATHER4_LZ_O          = 87,
    IMAGE_GATHER4_C_O           = 88,
    IMAGE_GATHER4_C_CL_O        = 89,
    IMAGE_GATHER4_C_L_O         = 92,
    IMAGE_GATHER4_C_B_O         = 93,
    IMAGE_GATHER4_C_B_CL_O      = 94,
    IMAGE_GATHER4_C_LZ_O        = 95,
    IMAGE_GET_LOD               = 96,
    IMAGE_SAMPLE_CD             = 104,
    IMAGE_SAMPLE_CD_CL          = 105,
    IMAGE_SAMPLE_C_CD           = 106,
    IMAGE_SAMPLE_C_CD_CL        = 107,
    IMAGE_SAMPLE_CD_O           = 108,
    IMAGE_SAMPLE_CD_CL_O        = 109,
    IMAGE_SAMPLE_C_CD_O         = 110,
    IMAGE_SAMPLE_C_CD_CL_O      = 111,
};

/* operands */

enum gcn_operand_scalar_t {
    OP_SGPR0   = 0,
    OP_SGPR103 = 103,
    OP_TTMP0   = 112,
    OP_TTMP11  = 123,
    OP_LITERAL = 255,
    OP_VGPR0   = 256,
    OP_VGPR255 = 511,
};

/* instruction encodings */

// Sea Islands ISA. Chapter 5: Scalar ALU Operations.
// Section 5.1: SALU Instruction Formats.
struct gcn_encoding_salu_t {
    uint32_t data   : 23;
    uint32_t op     : 7;
    uint32_t        : 2;
};
struct gcn_encoding_salu_sop1_t {
    uint32_t ssrc0  : 8;
    uint32_t op     : 8;
    uint32_t sdst   : 7;
    uint32_t        : 7;
    uint32_t        : 2;
};
struct gcn_encoding_salu_sop2_t {
    uint32_t ssrc0  : 8;
    uint32_t ssrc1  : 8;
    uint32_t sdst   : 7;
    uint32_t op     : 7;
    uint32_t        : 2;
};
struct gcn_encoding_salu_sopk_t {
    uint32_t simm   : 16;
    uint32_t sdst   : 7;
    uint32_t op     : 5;
    uint32_t        : 2;
    uint32_t        : 2;
};
struct gcn_encoding_salu_sopc_t {
    uint32_t ssrc0  : 8;
    uint32_t ssrc1  : 8;
    uint32_t op     : 7;
    uint32_t        : 7;
    uint32_t        : 2;
};
struct gcn_encoding_salu_sopp_t {
    uint32_t simm   : 16;
    uint32_t op     : 7;
    uint32_t        : 7;
    uint32_t        : 2;
};

// Sea Islands ISA. Chapter 6: Vector ALU Operations.
// Section 6.1: Microcode Encodings.
struct gcn_encoding_valu_vop1_t {
    uint32_t src0   : 9;
    uint32_t op     : 8;
    uint32_t vdst   : 8;
    uint32_t        : 6;
    uint32_t        : 1;
};
struct gcn_encoding_valu_vop2_t {
    uint32_t src0   : 9;
    uint32_t vsrc1  : 8;
    uint32_t vdst   : 8;
    uint32_t op     : 6;
    uint32_t        : 1;
};
struct gcn_encoding_valu_vopc_t {
    uint32_t src0   : 9;
    uint32_t vsrc1  : 8;
    uint32_t op     : 8;
    uint32_t        : 6;
    uint32_t        : 1;
};
struct gcn_encoding_valu_vintrp_t {
    uint32_t vsrc0  : 8;
    uint32_t chan   : 2;
    uint32_t attr   : 6;
    uint32_t op     : 2;
    uint32_t vdst   : 8;
    uint32_t        : 6;
};
struct gcn_encoding_valu_vop3a_t {
    struct {
        uint32_t vdst   : 8;
        uint32_t abs    : 3;
        uint32_t clmp   : 1;
        uint32_t r      : 5;
        uint32_t op     : 9;
        uint32_t        : 6;
    };
    struct {
        uint32_t src0   : 9;
        uint32_t src1   : 9;
        uint32_t src2   : 9;
        uint32_t omod   : 2;
        uint32_t neg    : 3;
    };
};
struct gcn_encoding_valu_vop3b_t {
    struct {
        uint32_t vdst   : 8;
        uint32_t sdst   : 7;
        uint32_t r      : 2;
        uint32_t op     : 9;
        uint32_t        : 6;
    };
    struct {
        uint32_t src0   : 9;
        uint32_t src1   : 9;
        uint32_t src2   : 9;
        uint32_t omod   : 2;
        uint32_t neg    : 3;
    };
};

// Sea Islands ISA. Chapter 12: Instruction Set
// Section 12.6: SMRD Instructions.
struct gcn_encoding_smrd_t {
    uint32_t offset  : 8;
    uint32_t imm     : 1;
    uint32_t sbase   : 6;
    uint32_t sdst    : 7;
    uint32_t op      : 5;
    uint32_t         : 5;
};

// Sea Islands ISA. Chapter 12.16: MIMG Instructions
struct gcn_encoding_mimg_t {
    struct {
        uint32_t        : 8;
        uint32_t dmask  : 4; // Enable mask for image R/W data components.
        uint32_t unrm   : 1; // Force address to be unnormalized.
        uint32_t glc    : 1; // Global coherency.
        uint32_t da     : 1; // Declare an array.
        uint32_t r128   : 1; // Texture resource size.
        uint32_t tfe    : 1; // Texture fail enable.
        uint32_t lwe    : 1; // LOD warning enable.
        uint32_t op     : 7; // Opcode.
        uint32_t slc    : 1; // System level coherent.
        uint32_t        : 6;
    };
    struct {
        uint32_t vaddr  : 8; // VGPR address source.
        uint32_t vdata  : 8; // VGPR for R/W result.
        uint32_t srsrc  : 5; // SGPR that specifies resource constant.
        uint32_t ssamp  : 5; // SGPR that specifies sampler constant.
        uint32_t        : 6;
    };
};

// Sea Islands ISA. Chapter 11: Exporting Pixel Color and Vertex Shader Parameters.
// Section 11.1: Microcode Encoding.
struct gcn_encoding_exp_t {
    struct {
        uint32_t en     : 4;
        uint32_t target : 6;
        uint32_t compr  : 1;
        uint32_t done   : 1;
        uint32_t vm     : 1;
        uint32_t        : 13;
        uint32_t        : 6;
    };
    struct {
        uint32_t vsrc0  : 8;
        uint32_t vsrc1  : 8;
        uint32_t vsrc2  : 8;
        uint32_t vsrc3  : 8;
    };
};

#endif // HW_PS4_LIVERPOOL_GCA_GCN_H
