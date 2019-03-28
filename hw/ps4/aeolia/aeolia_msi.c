/*
 * QEMU model of Aeolia MSI handling on the PCIe glue device.
 *
 * Copyright (c) 2018-2019. Alexandro Sanchez Bach
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

#include "aeolia_msi.h"

#include "exec/memory.h"
#include "exec/address-spaces.h"

#define REG_MSI(x)                                                    (x)
#define REG_MSI_CONTROL                                    REG_MSI(0x000)
#define REG_MSI_UNK004                                     REG_MSI(0x004)
#define REG_MSI_UNK008                                     REG_MSI(0x008) // Always 0xFFFFFFFF?

// Always 0xB7FFFFX0?
#define REG_MSI_UNK00C(func)              (REG_MSI(0x00C) + 4*(func & 7))
#define REG_MSI_FNC0_UNK00C                             REG_MSI_UNK00C(0)
#define REG_MSI_FNC1_UNK00C                             REG_MSI_UNK00C(1)
#define REG_MSI_FNC2_UNK00C                             REG_MSI_UNK00C(2)
#define REG_MSI_FNC3_UNK00C                             REG_MSI_UNK00C(3)
#define REG_MSI_FNC4_UNK00C                             REG_MSI_UNK00C(4)
#define REG_MSI_FNC5_UNK00C                             REG_MSI_UNK00C(5)
#define REG_MSI_FNC6_UNK00C                             REG_MSI_UNK00C(6)
#define REG_MSI_FNC7_UNK00C                             REG_MSI_UNK00C(7)

#define REG_MSI_IRQ_STA(func)             (REG_MSI(0x02C) + 4*(func & 7))
#define REG_MSI_FNC0_IRQ_STA                           REG_MSI_IRQ_STA(0)
#define REG_MSI_FNC1_IRQ_STA                           REG_MSI_IRQ_STA(1)
#define REG_MSI_FNC2_IRQ_STA                           REG_MSI_IRQ_STA(2)
#define REG_MSI_FNC3_IRQ_STA                           REG_MSI_IRQ_STA(3)
#define REG_MSI_FNC4_IRQ_STA                           REG_MSI_IRQ_STA(4)
#define REG_MSI_FNC5_IRQ_STA                           REG_MSI_IRQ_STA(5)
#define REG_MSI_FNC6_IRQ_STA                           REG_MSI_IRQ_STA(6)
#define REG_MSI_FNC7_IRQ_STA                           REG_MSI_IRQ_STA(7)

#define REG_MSI_MASK(func)                (REG_MSI(0x04C) + 4*(func & 7))
#define REG_MSI_FNC0_MASK                                 REG_MSI_MASK(0)
#define REG_MSI_FNC1_MASK                                 REG_MSI_MASK(1)
#define REG_MSI_FNC2_MASK                                 REG_MSI_MASK(2)
#define REG_MSI_FNC3_MASK                                 REG_MSI_MASK(3)
#define REG_MSI_FNC4_MASK                                 REG_MSI_MASK(4)
#define REG_MSI_FNC5_MASK                                 REG_MSI_MASK(5)
#define REG_MSI_FNC6_MASK                                 REG_MSI_MASK(6)
#define REG_MSI_FNC7_MASK                                 REG_MSI_MASK(7)

#define REG_MSI_DATA(func)                (REG_MSI(0x08C) + 4*(func & 7))
#define REG_MSI_FNC0_DATA                                 REG_MSI_DATA(0)
#define REG_MSI_FNC1_DATA                                 REG_MSI_DATA(1)
#define REG_MSI_FNC2_DATA                                 REG_MSI_DATA(2)
#define REG_MSI_FNC3_DATA                                 REG_MSI_DATA(3)
#define REG_MSI_FNC4_DATA                                 REG_MSI_DATA(4)
#define REG_MSI_FNC5_DATA                                 REG_MSI_DATA(5)
#define REG_MSI_FNC6_DATA                                 REG_MSI_DATA(6)
#define REG_MSI_FNC7_DATA                                 REG_MSI_DATA(7)

#define REG_MSI_ADDR(func)                (REG_MSI(0x0AC) + 4*(func & 7))
#define REG_MSI_FNC0_ADDR                                 REG_MSI_ADDR(0)
#define REG_MSI_FNC1_ADDR                                 REG_MSI_ADDR(1)
#define REG_MSI_FNC2_ADDR                                 REG_MSI_ADDR(2)
#define REG_MSI_FNC3_ADDR                                 REG_MSI_ADDR(3)
#define REG_MSI_FNC4_ADDR                                 REG_MSI_ADDR(4)
#define REG_MSI_FNC5_ADDR                                 REG_MSI_ADDR(5)
#define REG_MSI_FNC6_ADDR                                 REG_MSI_ADDR(6)
#define REG_MSI_FNC7_ADDR                                 REG_MSI_ADDR(7)

// Always 0x0?
#define REG_MSI_UNK0CC(func)              (REG_MSI(0x0CC) + 4*(func & 7))
#define REG_MSI_FNC0_UNK0CC                             REG_MSI_UNK0CC(0)
#define REG_MSI_FNC1_UNK0CC                             REG_MSI_UNK0CC(1)
#define REG_MSI_FNC2_UNK0CC                             REG_MSI_UNK0CC(2)
#define REG_MSI_FNC3_UNK0CC                             REG_MSI_UNK0CC(3)
#define REG_MSI_FNC4_UNK0CC                             REG_MSI_UNK0CC(4)
#define REG_MSI_FNC5_UNK0CC                             REG_MSI_UNK0CC(5)
#define REG_MSI_FNC6_UNK0CC                             REG_MSI_UNK0CC(6)
#define REG_MSI_FNC7_UNK0CC                             REG_MSI_UNK0CC(7)

#define REG_MSI_DATA_LO(func, sub)           REG_MSI_LODATA_FN##func(sub)
#define REG_MSI_FNC0_DATA_LO(sub)       (REG_MSI(0x100) + 4*(sub & 0x03))
#define REG_MSI_FNC1_DATA_LO(sub)       (REG_MSI(0x110) + 4*(sub & 0x03))
#define REG_MSI_FNC2_DATA_LO(sub)       (REG_MSI(0x120) + 4*(sub & 0x03))
#define REG_MSI_FNC3_DATA_LO(sub)       (REG_MSI(0x130) + 4*(sub & 0x03))
#define REG_MSI_FNC4_DATA_LO(sub)       (REG_MSI(0x140) + 4*(sub & 0x17))
#define REG_MSI_FNC5_DATA_LO(sub)       (REG_MSI(0x1A0) + 4*(sub & 0x01))
#define REG_MSI_FNC6_DATA_LO(sub)       (REG_MSI(0x1B0) + 4*(sub & 0x01))
#define REG_MSI_FNC7_DATA_LO(sub)       (REG_MSI(0x1C0) + 4*(sub & 0x03))

#define CASE_FUNC_R(index, name, variable) \
    case REG_MSI_FNC##index##_##name: \
        value = variable[index]; \
        break;
#define CASE_FUNC_W(index, name, variable) \
    case REG_MSI_FNC##index##_##name: \
        variable[index] = value; \
        break;
#define CASE_FUNCS(type, name, variable) \
    CASE_FUNC_##type(0, name, variable) \
    CASE_FUNC_##type(1, name, variable) \
    CASE_FUNC_##type(2, name, variable) \
    CASE_FUNC_##type(3, name, variable) \
    CASE_FUNC_##type(4, name, variable) \
    CASE_FUNC_##type(5, name, variable) \
    CASE_FUNC_##type(6, name, variable) \
    CASE_FUNC_##type(7, name, variable)

void apcie_msi_trigger(apcie_msi_controller_t *s, uint32_t func, uint32_t sub)
{
    uint32_t data;
    bool enabled;

    if (sub > 30) {
        fprintf(stderr, "%s: Subfunction #%u out of range!",
            __FUNCTION__, sub);
        assert(0);
        return;
    }
    enabled = s->func_mask[func] & (1 << sub);
    if (!enabled) {
        fprintf(stderr, "%s: Cannot send MSI to disabled device %u:%u!",
            __FUNCTION__, func, sub);
        assert(0);
        return;
    }

    data = s->func_data[func];
    switch (func) {
    case 0:
        assert(sub < 4);
        data |= s->func0_data_lo[sub];
        break;
    case 1:
        assert(sub < 4);
        data |= s->func1_data_lo[sub];
        break;
    case 2:
        assert(sub < 4);
        data |= s->func2_data_lo[sub];
        break;
    case 3:
        assert(sub < 4);
        data |= s->func3_data_lo[sub];
        break;
    case 4:
        assert(sub < 24);
        data |= s->func4_data_lo[sub];
        break;
    case 5:
        assert(sub < 2);
        data |= s->func5_data_lo[sub];
        break;
    case 6:
        assert(sub < 2);
        data |= s->func6_data_lo[sub];
        break;
    case 7:
        assert(sub < 4);
        data |= s->func7_data_lo[sub];
        break;
    default:
        fprintf(stderr, "%s: Function #%u out of range!", __FUNCTION__, func);
        return;
    }
    stl_le_phys(&address_space_memory, s->func_addr[func], data);
}

uint32_t apcie_msi_read(apcie_msi_controller_t *s, uint32_t offs)
{
    uint32_t data_lo_index;
    uint32_t value;

    value = 0;
    switch (offs) {
    // Handle global control/status
    case REG_MSI_CONTROL:
    case REG_MSI_UNK004:
    case REG_MSI_UNK008:
        break;

    // Handle regular function-specific registers
    CASE_FUNCS(R, ADDR, s->func_addr);
    CASE_FUNCS(R, MASK, s->func_mask);
    CASE_FUNCS(R, DATA, s->func_data);

    // Handle irregular function-specific registers
    default:
        data_lo_index = (offs - REG_MSI_FNC0_DATA_LO(0)) >> 2;
        if (data_lo_index < 48) {
            value = s->data_lo[data_lo_index];
        }
    }
    return value;
}

void apcie_msi_write(apcie_msi_controller_t *s, uint32_t offs, uint32_t value)
{
    uint32_t data_lo_index;

    switch (offs) {
    // Handle global control/status
    case REG_MSI_CONTROL:
    case REG_MSI_UNK004:
    case REG_MSI_UNK008:
        break;

    // Handle regular function-specific registers
    CASE_FUNCS(W, ADDR, s->func_addr);
    CASE_FUNCS(W, MASK, s->func_mask);
    CASE_FUNCS(W, DATA, s->func_data);

    // Handle irregular function-specific registers
    default:
        data_lo_index = (offs - REG_MSI_FNC0_DATA_LO(0)) >> 2;
        if (data_lo_index < 52) {
            s->data_lo[data_lo_index] = value;
        }
    }
}
