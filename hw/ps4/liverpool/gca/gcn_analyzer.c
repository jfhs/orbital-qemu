/*
 * AMD GCN bytecode analyzer
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

#include "gcn_analyzer.h"

#include <assert.h>

#define UNUSED(arg) (void)(arg)

#define ARRAYCOUNT(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#ifdef __cplusplus
extern "C" {
#endif

/* dumper */

void gcn_analyzer_init(gcn_analyzer_t *ctxt)
{
    memset(ctxt, 0, sizeof(gcn_analyzer_t));
    ctxt->has_isolated_components = 1;
}

void gcn_analyzer_print_deps(gcn_analyzer_t *ctxt, FILE *stream)
{
    UNUSED(ctxt);
    
    fprintf(stream, "...\n");
}

void gcn_analyzer_print_usage(gcn_analyzer_t *ctxt, FILE *stream)
{
    bool comma;
    unsigned int i;
    const char* s;

    // Show type usage
    comma = false;
    fprintf(stream, "- %-16s: ", "used_types");
    for (i = 0; i < 32; i++) {
        if (!(ctxt->used_types & (1 << i)))
            continue;
        
        switch (i) {
        case GCN_TYPE_B32: s = "b32"; break;
        case GCN_TYPE_B64: s = "b64"; break;
        case GCN_TYPE_F16: s = "f16"; break;
        case GCN_TYPE_F32: s = "f32"; break;
        case GCN_TYPE_F64: s = "f64"; break;
        case GCN_TYPE_I16: s = "i16"; break;
        case GCN_TYPE_I24: s = "i24"; break;
        case GCN_TYPE_I32: s = "i32"; break;
        case GCN_TYPE_I64: s = "i64"; break;
        case GCN_TYPE_U16: s = "u16"; break;
        case GCN_TYPE_U24: s = "u24"; break;
        case GCN_TYPE_U32: s = "u32"; break;
        case GCN_TYPE_U64: s = "u64"; break;
        case GCN_TYPE_ANY:
            continue;
        default:
            s = "???";
        }
        fprintf(stream, "%s%s", comma ? ", " : "", s);
        comma = true;
    }

    // Show register usage
#define USAGE_REG(array, name) {                               \
        comma = false;                                         \
        fprintf(stream, "\n- %-16s: ", #array);                \
        for (i = 0; i < ARRAYCOUNT(ctxt->array); i++) {        \
            if (!ctxt->array[i]) continue;                     \
            fprintf(stream, "%s" name, comma ? ", " : "", i);  \
            comma = true;                                      \
        }                                                      \
    }
    USAGE_REG(used_sgpr, "s%d");
    USAGE_REG(used_vgpr, "v%d");
    USAGE_REG(used_exp_mrt, "mrt%d");
    USAGE_REG(used_exp_mrtz, "mrtz%d");
    USAGE_REG(used_exp_pos, "pos%d");
    USAGE_REG(used_exp_param, "param%d");
    fprintf(stream, "\n");
}

void gcn_analyzer_print_props(gcn_analyzer_t *ctxt, FILE *stream)
{
#define DUMP_PROP(prop) \
    fprintf(stream, "- %-20s: %s\n", #prop, ctxt->prop ? "true" : "false");

    DUMP_PROP(has_isolated_components);
#undef DUMP_PROP
}

void gcn_analyzer_print(gcn_analyzer_t *ctxt, FILE *stream)
{
    fprintf(stream, "## usage\n");
    gcn_analyzer_print_usage(ctxt, stream);
    fprintf(stream, "\n## properties\n");
    gcn_analyzer_print_props(ctxt, stream);
    fprintf(stream, "\n## dependencies\n");
    gcn_analyzer_print_deps(ctxt, stream);
}

/* helpers */

static void analyze_operand(gcn_analyzer_t *ctxt, gcn_operand_t *op)
{
    if (!(op->flags & GCN_FLAGS_OP_USED)) {
        return;
    }

    switch (op->kind) {
    case GCN_KIND_SGPR:
        assert(op->id < ARRAYCOUNT(ctxt->used_sgpr));
        ctxt->used_sgpr[op->id] = 1;
        break;
    case GCN_KIND_VGPR:
        assert(op->id < ARRAYCOUNT(ctxt->used_vgpr));
        ctxt->used_vgpr[op->id] = 1;
        break;
    case GCN_KIND_EXP_MRT:
        assert(op->id < ARRAYCOUNT(ctxt->used_exp_mrt));
        ctxt->used_exp_mrt[op->id] = 1;
        break;
    case GCN_KIND_EXP_MRTZ:
        assert(op->id < ARRAYCOUNT(ctxt->used_exp_mrtz));
        ctxt->used_exp_mrtz[op->id] = 1;
        break;
    case GCN_KIND_EXP_POS:
        assert(op->id < ARRAYCOUNT(ctxt->used_exp_pos));
        ctxt->used_exp_pos[op->id] = 1;
        break;
    case GCN_KIND_EXP_PARAM:
        assert(op->id < ARRAYCOUNT(ctxt->used_exp_param));
        ctxt->used_exp_param[op->id] = 1;
        break;
    default:
        break;
    }
}

/* callbacks */

#define ANALYZER_CALLBACK(name) \
    static void analyze_##name(gcn_instruction_t *insn, void *ctxt)

static void analyze_insn(gcn_analyzer_t *ctxt,
    gcn_instruction_t *insn)
{
    assert(insn->type_dst < 32);
    assert(insn->type_src < 32);
    ctxt->used_types |= (1 << insn->type_dst);
    ctxt->used_types |= (1 << insn->type_src);

    analyze_operand(ctxt, &insn->dst);
    analyze_operand(ctxt, &insn->src0);
    analyze_operand(ctxt, &insn->src1);
    analyze_operand(ctxt, &insn->src2);
    analyze_operand(ctxt, &insn->src3);
}

#define ANALYZE_INSN(name) \
    ANALYZER_CALLBACK(name) { \
        analyze_insn(ctxt, insn); \
    };

#define GCN_HANDLER(encoding, name) \
    ANALYZE_INSN(name);
#include "gcn_handlers.inc"
#undef GCN_HANDLER

gcn_parser_callbacks_t gcn_analyzer_callbacks = {
#define GCN_HANDLER(encoding, name) \
    .handle_##name = analyze_##name,
#include "gcn_handlers.inc"
#undef GCN_HANDLER
};

#ifdef __cplusplus
}
#endif
