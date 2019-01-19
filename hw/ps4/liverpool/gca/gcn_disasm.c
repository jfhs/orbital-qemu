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

void gcn_disasm_init(gcn_disasm_t *ctxt)
{
    memset(ctxt, 0, sizeof(gcn_disasm_t));
}

/* utilities */

static void disasm_print(gcn_disasm_t *ctxt, const char *name)
{
    printf("> %s\n", name);
}

/* disassembly */

static void disasm_sop2(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, const char *name)
{
    disasm_print(ctxt, name);
}

static void disasm_sopk(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, const char *name)
{
    disasm_print(ctxt, name);
}

static void disasm_sop1(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, const char *name)
{
    disasm_print(ctxt, name);
}

static void disasm_sopc(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, const char *name)
{
    disasm_print(ctxt, name);
}

static void disasm_sopp(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, const char *name)
{
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
DISASM_SOP1(s_call);

// SOP1 Instructions
DISASM_SOP1(s_mov);
DISASM_SOP1(s_cmov);
DISASM_SOP1(s_not);

gcn_parser_callbacks_t gcn_disasm_callbacks = {
    .handle_s_add        = disasm_s_add,
    .handle_s_addc       = disasm_s_addc,
    .handle_s_and        = disasm_s_and,
    .handle_s_andn2      = disasm_s_andn2,
    .handle_s_ashr       = disasm_s_ashr,
    .handle_s_bfe        = disasm_s_bfe,
    .handle_s_bfm        = disasm_s_bfm,
    .handle_s_call       = disasm_s_call,
    .handle_s_cmov       = disasm_s_cmov,
    .handle_s_cselect    = disasm_s_cselect,
    .handle_s_lshl       = disasm_s_lshl,
    .handle_s_lshr       = disasm_s_lshr,
    .handle_s_max        = disasm_s_max,
    .handle_s_min        = disasm_s_min,
    .handle_s_mov        = disasm_s_mov,
    .handle_s_mul        = disasm_s_mul,
    .handle_s_nand       = disasm_s_nand,
    .handle_s_nor        = disasm_s_nor,
    .handle_s_not        = disasm_s_not,
    .handle_s_or         = disasm_s_or,
    .handle_s_orn2       = disasm_s_orn2,
    .handle_s_sub        = disasm_s_sub,
    .handle_s_subb       = disasm_s_subb,
    .handle_s_xnor       = disasm_s_xnor,
    .handle_s_xor        = disasm_s_xor,
};
