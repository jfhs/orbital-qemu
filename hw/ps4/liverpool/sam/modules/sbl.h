/*
 * QEMU model of common SBL structures.
 *
 * Copyright (c) 2017-2018 Alexandro Sanchez Bach
 *
 * Partially based on research from: flatz
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

#ifndef HW_PS4_LIVERPOOL_SAM_MODULES_SBL_H
#define HW_PS4_LIVERPOOL_SAM_MODULES_SBL_H

#include "qemu/osdep.h"

/* structures */
typedef struct sbl_chunk_entry_t {
    uint64_t data_addr;
    uint64_t data_size;
} sbl_chunk_entry_t;

typedef struct sbl_chunk_table_t {
    uint64_t data_addr;
    uint64_t data_size;
    uint64_t num_entries;
    uint64_t reserved;
    sbl_chunk_entry_t entries[0];
} sbl_chunk_table_t;

#endif /* HW_PS4_LIVERPOOL_SAM_MODULES_SBL_H */
