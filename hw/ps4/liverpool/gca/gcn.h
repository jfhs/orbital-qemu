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

/* opcodes */

enum gcn_opcode_sop2_t {
    S_ADD_U32               = 0,
    S_SUB_U32               = 1,
    S_ADD_I32               = 2,
    S_SUB_I32               = 3,
    S_ADDC_U32              = 4,
    S_SUBB_U32              = 5,
    S_MIN_I32               = 6,
    S_MIN_U32               = 7,
    S_MAX_I32               = 8,
    S_MAX_U32               = 9,
    S_CSELECT_B32           = 10,
    S_CSELECT_B64           = 11,
    S_AND_B32               = 12,
    S_AND_B64               = 13,
    S_OR_B32                = 14,
    S_OR_B64                = 15,
    S_XOR_B32               = 16,
    S_XOR_B64               = 17,
    S_ANDN2_B32             = 18,
    S_ANDN2_B64             = 19,
    S_ORN2_B32              = 20,
    S_ORN2_B64              = 21,
    S_NAND_B32              = 22,
    S_NAND_B64              = 23,
    S_NOR_B32               = 24,
    S_NOR_B64               = 25,
    S_XNOR_B32              = 26,
    S_XNOR_B64              = 27,
    S_LSHL_B32              = 28,
    S_LSHL_B64              = 29,
    S_LSHR_B32              = 30,
    S_LSHR_B64              = 31,
    S_ASHR_I32              = 32,
    S_ASHR_I64              = 33,
    S_BFM_B32               = 34,
    S_BFM_B64               = 35,
    S_MUL_I32               = 36,
    S_BFE_U32               = 37,
    S_BFE_I32               = 38,
    S_BFE_U64               = 39,
    S_BFE_I64               = 40,
};

enum gcn_opcode_sopk_t {
    S_MOVK_I32              = 0,
    S_CMOVK_I32             = 1,
    S_CMPK_EQ_I32           = 2,
    S_CMPK_LG_I32           = 3,
    S_CMPK_GT_I32           = 4,
    S_CMPK_GE_I32           = 5,
    S_CMPK_LT_I32           = 6,
    S_CMPK_LE_I32           = 7,
    S_CMPK_EQ_U32           = 8,
    S_CMPK_LG_U32           = 9,
    S_CMPK_GT_U32           = 10,
    S_CMPK_GE_U32           = 11,
    S_CMPK_LT_U32           = 12,
    S_CMPK_LE_U32           = 13,
    S_ADDK_I32              = 14,
    S_MULK_I32              = 15,
    S_CBRANCH_I_FORK        = 16,
    S_GETREG_B32            = 17,
    S_SETREG_B32            = 18,
    S_SETREG_IMM32_B32      = 20,
    S_CALL_B64              = 21,
};

enum gcn_opcode_sop1_t {
    S_MOV_B32               = 0,
    S_MOV_B64               = 1,
    S_CMOV_B32              = 2,
    S_CMOV_B64              = 3,
    S_NOT_B32               = 4,
    S_NOT_B64               = 5,
    S_WQM_B32               = 6,
    S_WQM_B64               = 7,
    S_BREV_B32              = 8,
    S_BREV_B64              = 9,
    S_BCNT0_I32_B32         = 10,
    S_BCNT0_I32_B64         = 11,
    S_BCNT1_I32_B32         = 12,
    S_BCNT1_I32_B64         = 13,
    S_FF0_I32_B32           = 14,
    S_FF0_I32_B64           = 15,
    S_FF1_I32_B32           = 16,
    S_FF1_I32_B64           = 17,
    S_FLBIT_I32_B32         = 18,
    S_FLBIT_I32_B64         = 19,
    S_FLBIT_I32             = 20,
    S_FLBIT_I32_I64         = 21,
    S_SEXT_I32_I8           = 22,
    S_SEXT_I32_I16          = 23,
    S_BITSET0_B32           = 24,
    S_BITSET0_B64           = 25,
    S_BITSET1_B32           = 26,
    S_BITSET1_B64           = 27,
    S_GETPC_B64             = 28,
    S_SETPC_B64             = 29,
    S_SWAPPC_B64            = 30,
    S_RFE_B64               = 31,
    S_AND_SAVEEXEC_B64      = 32,
    S_OR_SAVEEXEC_B64       = 33,
    S_XOR_SAVEEXEC_B64      = 34,
    S_ANDN2_SAVEEXEC_B64    = 35,
    S_ORN2_SAVEEXEC_B64     = 36,
    S_NAND_SAVEEXEC_B64     = 37,
    S_NOR_SAVEEXEC_B64      = 38,
    S_XNOR_SAVEEXEC_B64     = 39,
    S_QUADMASK_B32          = 40,
    S_QUADMASK_B64          = 41,
    S_MOVRELS_B32           = 42,
    S_MOVRELS_B64           = 43,
    S_MOVRELD_B32           = 44,
    S_MOVRELD_B64           = 45,
    S_CBRANCH_JOIN          = 46,
    S_ABS_I32               = 48,
    S_SET_GPR_IDX_IDX       = 50,
    S_ANDN1_SAVEEXEC_B64    = 51,
    S_ORN1_SAVEEXEC_B64     = 52,
    S_ANDN1_WREXEC_B64      = 53,
    S_ANDN2_WREXEC_B64      = 54,
    S_BITREPLICATE_B64_B32  = 55,
};

/* encodings */

// Vega ISA: 5.1. SALU Instruction Formats
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

#endif // HW_PS4_LIVERPOOL_GCA_GCN_H
