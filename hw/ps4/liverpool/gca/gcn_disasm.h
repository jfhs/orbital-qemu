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

#ifndef HW_PS4_LIVERPOOL_GCA_GCN_DISASM_H
#define HW_PS4_LIVERPOOL_GCA_GCN_DISASM_H

#include "gcn.h"
#include "gcn_parser.h"

typedef struct gcn_disasm_t {
} gcn_disasm_t;

/* callbacks */

extern gcn_parser_callbacks_t gcn_disasm_callbacks;

/* functions */

void gcn_disasm_init(gcn_disasm_t *ctxt);

#endif // HW_PS4_LIVERPOOL_GCA_GCN_DISASM_H
