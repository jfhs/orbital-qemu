/*
 * AMD GCN bytecode disassembler
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

#include "gcn_disasm.h"

#include <stdio.h>
#include <string.h>

#define UNUSED(arg) (void)(arg)

void gcn_disasm_init(gcn_disasm_t *ctxt)
{
    memset(ctxt, 0, sizeof(gcn_disasm_t));
}

/* utilities */

static void disasm_print(gcn_disasm_t *ctxt, const char *name)
{
    UNUSED(ctxt);

    printf("> %s\n", name);
}

/* disassembly */

static void disasm_sop2(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, const char *name)
{
    UNUSED(insn);
    disasm_print(ctxt, name);
}

static void disasm_sopk(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, const char *name)
{
    UNUSED(insn);
    disasm_print(ctxt, name);
}

static void disasm_sop1(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, const char *name)
{
    UNUSED(insn);
    disasm_print(ctxt, name);
}

static void disasm_sopc(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, const char *name)
{
    UNUSED(insn);
    disasm_print(ctxt, name);
}

static void disasm_sopp(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, const char *name)
{
    UNUSED(insn);
    disasm_print(ctxt, name);
}

/* callbacks */

#define DISASM_CALLBACK(name) \
    static void disasm_##name(gcn_instruction_t *insn, void *ctxt)

#define DISASM_SOP2(name) \
    DISASM_CALLBACK(name) { \
        disasm_sop2(ctxt, insn, #name); \
    };
#define DISASM_SOPK(name) \
    DISASM_CALLBACK(name) { \
        disasm_sopk(ctxt, insn, #name); \
    };
#define DISASM_SOP1(name) \
    DISASM_CALLBACK(name) { \
        disasm_sop1(ctxt, insn, #name); \
    };
#define DISASM_SOPC(name) \
    DISASM_CALLBACK(name) { \
        disasm_sopc(ctxt, insn, #name); \
    };
#define DISASM_SOPP(name) \
    DISASM_CALLBACK(name) { \
        disasm_sopp(ctxt, insn, #name); \
    };
#define DISASM_VOP1(name) \
    DISASM_CALLBACK(name) { \
        UNUSED(insn); \
        disasm_print(ctxt, #name); \
    };
#define DISASM_VOP2(name) \
    DISASM_CALLBACK(name) { \
        UNUSED(insn); \
        disasm_print(ctxt, #name); \
    };
#define DISASM_VOPC(name) \
    DISASM_CALLBACK(name) { \
        UNUSED(insn); \
        disasm_print(ctxt, #name); \
    };

// SOP2 Instructions
DISASM_SOP2(s_add);
DISASM_SOP2(s_addc);
DISASM_SOP2(s_and);
DISASM_SOP2(s_andn2);
DISASM_SOP2(s_ashr);
DISASM_SOP2(s_bfe);
DISASM_SOP2(s_bfm);
DISASM_SOP2(s_cselect);
DISASM_SOP2(s_lshl);
DISASM_SOP2(s_lshr);
DISASM_SOP2(s_max);
DISASM_SOP2(s_min);
DISASM_SOP2(s_mul);
DISASM_SOP2(s_nand);
DISASM_SOP2(s_nor);
DISASM_SOP2(s_or);
DISASM_SOP2(s_orn2);
DISASM_SOP2(s_sub);
DISASM_SOP2(s_subb);
DISASM_SOP2(s_xnor);
DISASM_SOP2(s_xor);

// SOPK Instructions
DISASM_SOPK(s_call);
DISASM_SOPK(s_cmovk);
DISASM_SOPK(s_cmpk);
DISASM_SOPK(s_movk);

// SOP1 Instructions
DISASM_SOP1(s_mov);
DISASM_SOP1(s_cmov);
DISASM_SOP1(s_not);

// SOPP Instructions
DISASM_SOPP(s_barrier);
DISASM_SOPP(s_branch);
DISASM_SOPP(s_cbranch_cdbgsys);
DISASM_SOPP(s_cbranch_cdbgsys_and_user);
DISASM_SOPP(s_cbranch_cdbgsys_or_user);
DISASM_SOPP(s_cbranch_cdbguser);
DISASM_SOPP(s_cbranch_execnz);
DISASM_SOPP(s_cbranch_execz);
DISASM_SOPP(s_cbranch_scc0);
DISASM_SOPP(s_cbranch_scc1);
DISASM_SOPP(s_cbranch_vccnz);
DISASM_SOPP(s_cbranch_vccz);
DISASM_SOPP(s_decperflevel);
DISASM_SOPP(s_endpgm);
DISASM_SOPP(s_icache_inv);
DISASM_SOPP(s_incperflevel);
DISASM_SOPP(s_nop);
DISASM_SOPP(s_sendmsg);
DISASM_SOPP(s_sendmsghalt);
DISASM_SOPP(s_sethalt);
DISASM_SOPP(s_setkill);
DISASM_SOPP(s_setprio);
DISASM_SOPP(s_sleep);
DISASM_SOPP(s_trap);
DISASM_SOPP(s_ttracedata);
DISASM_SOPP(s_waitcnt);

// VOP2 Instructions
DISASM_VOP2(v_and);
DISASM_VOP2(v_ashr);
DISASM_VOP2(v_ashrrev);
DISASM_VOP2(v_bfm);
DISASM_VOP2(v_lshl);
DISASM_VOP2(v_lshlrev);
DISASM_VOP2(v_lshr);
DISASM_VOP2(v_lshrrev);
DISASM_VOP2(v_mac);
DISASM_VOP2(v_madak);
DISASM_VOP2(v_madmk);
DISASM_VOP2(v_max);
DISASM_VOP2(v_min);
DISASM_VOP2(v_mul);
DISASM_VOP2(v_mul_hi);
DISASM_VOP2(v_or);
DISASM_VOP2(v_xor);

gcn_parser_callbacks_t gcn_disasm_callbacks = {
    .handle_s_add                       = disasm_s_add,
    .handle_s_addc                      = disasm_s_addc,
    .handle_s_and                       = disasm_s_and,
    .handle_s_andn2                     = disasm_s_andn2,
    .handle_s_ashr                      = disasm_s_ashr,
    .handle_s_barrier                   = disasm_s_barrier,
    .handle_s_bfe                       = disasm_s_bfe,
    .handle_s_bfm                       = disasm_s_bfm,
    .handle_s_branch                    = disasm_s_branch,
    .handle_s_call                      = disasm_s_call,
    .handle_s_cmovk                     = disasm_s_cmovk,
    .handle_s_cmpk                      = disasm_s_cmpk,
    .handle_s_cbranch_cdbgsys           = disasm_s_cbranch_cdbgsys,
    .handle_s_cbranch_cdbgsys_and_user  = disasm_s_cbranch_cdbgsys_and_user,
    .handle_s_cbranch_cdbgsys_or_user   = disasm_s_cbranch_cdbgsys_or_user,
    .handle_s_cbranch_cdbguser          = disasm_s_cbranch_cdbguser,
    .handle_s_cbranch_execnz            = disasm_s_cbranch_execnz,
    .handle_s_cbranch_execz             = disasm_s_cbranch_execz,
    .handle_s_cbranch_scc0              = disasm_s_cbranch_scc0,
    .handle_s_cbranch_scc1              = disasm_s_cbranch_scc1,
    .handle_s_cbranch_vccnz             = disasm_s_cbranch_vccnz,
    .handle_s_cbranch_vccz              = disasm_s_cbranch_vccz,
    .handle_s_cmov                      = disasm_s_cmov,
    .handle_s_cselect                   = disasm_s_cselect,
    .handle_s_decperflevel              = disasm_s_decperflevel,
    .handle_s_endpgm                    = disasm_s_endpgm,
    .handle_s_icache_inv                = disasm_s_icache_inv,
    .handle_s_incperflevel              = disasm_s_incperflevel,
    .handle_s_lshl                      = disasm_s_lshl,
    .handle_s_lshr                      = disasm_s_lshr,
    .handle_s_max                       = disasm_s_max,
    .handle_s_min                       = disasm_s_min,
    .handle_s_mov                       = disasm_s_mov,
    .handle_s_movk                      = disasm_s_movk,
    .handle_s_mul                       = disasm_s_mul,
    .handle_s_nand                      = disasm_s_nand,
    .handle_s_nop                       = disasm_s_nop,
    .handle_s_nor                       = disasm_s_nor,
    .handle_s_not                       = disasm_s_not,
    .handle_s_or                        = disasm_s_or,
    .handle_s_orn2                      = disasm_s_orn2,
    .handle_s_sendmsg                   = disasm_s_sendmsg,
    .handle_s_sendmsghalt               = disasm_s_sendmsghalt,
    .handle_s_sethalt                   = disasm_s_sethalt,
    .handle_s_setkill                   = disasm_s_setkill,
    .handle_s_setprio                   = disasm_s_setprio,
    .handle_s_sleep                     = disasm_s_sleep,
    .handle_s_sub                       = disasm_s_sub,
    .handle_s_subb                      = disasm_s_subb,
    .handle_s_trap                      = disasm_s_trap,
    .handle_s_ttracedata                = disasm_s_ttracedata,
    .handle_s_waitcnt                   = disasm_s_waitcnt,
    .handle_s_xnor                      = disasm_s_xnor,
    .handle_s_xor                       = disasm_s_xor,
    .handle_v_and                       = disasm_v_and,
    .handle_v_ashr                      = disasm_v_ashr,
    .handle_v_ashrrev                   = disasm_v_ashrrev,
    .handle_v_bfm                       = disasm_v_bfm,
    .handle_v_lshl                      = disasm_v_lshl,
    .handle_v_lshlrev                   = disasm_v_lshlrev,
    .handle_v_lshr                      = disasm_v_lshr,
    .handle_v_lshrrev                   = disasm_v_lshrrev,
    .handle_v_mac                       = disasm_v_mac,
    .handle_v_madak                     = disasm_v_madak,
    .handle_v_madmk                     = disasm_v_madmk,
    .handle_v_max                       = disasm_v_max,
    .handle_v_min                       = disasm_v_min,
    .handle_v_mul                       = disasm_v_mul,
    .handle_v_mul_hi                    = disasm_v_mul_hi,
    .handle_v_or                        = disasm_v_or,
    .handle_v_xor                       = disasm_v_xor,
};
