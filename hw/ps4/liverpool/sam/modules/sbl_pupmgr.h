/*
 * QEMU model of SBL's PUPMgr module.
 *
 * Copyright (c) 2017-2019 Alexandro Sanchez Bach
 *
 * Based on research from: flatz
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

#ifndef HW_PS4_LIVERPOOL_SAM_MODULES_SBL_PUPMGR_H
#define HW_PS4_LIVERPOOL_SAM_MODULES_SBL_PUPMGR_H

#include "qemu/osdep.h"

/* functions */
#define PUPMGR_SM_VERIFY_HEADER               0xF
#define PUPMGR_SM_EXIT                     0xFFFF

/* constants */
#define PUPMGR_PATH_INVALID              0
#define PUPMGR_PATH_SYSTEM               1
#define PUPMGR_PATH_SYSTEM_EX            2
#define PUPMGR_PATH_UPDATE               3
#define PUPMGR_PATH_PREINST              4
#define PUPMGR_PATH_PREINST2             5
#define PUPMGR_PATH_PFSMNT               6
#define PUPMGR_PATH_USB                  7
#define PUPMGR_PATH_HOST                 8
#define PUPMGR_PATH_ROOT                 9
#define PUPMGR_PATH_DIAG                10
#define PUPMGR_PATH_RDIAG               11

/* structures */
typedef struct pupmgr_verify_header_t {
    /* <input> */
    uint64_t header_addr;
    uint64_t header_size;  // TODO: Is this really size?
    uint64_t header_flags; // TODO: Is this really flags?
    /* <output> */
} pupmgr_verify_header_t;

typedef struct pupmgr_exit_t {
    uint8_t buf[0x100];
    /* <input> */
    // TODO
    /* <output> */
    // TODO
} pupmgr_exit_t;

/* functions */
void sbl_pupmgr_spawn();
bool sbl_pupmgr_spawned();

uint32_t sbl_pupmgr_verify_header(
    const pupmgr_verify_header_t *query, pupmgr_verify_header_t *reply);
uint32_t sbl_pupmgr_exit(
    const pupmgr_exit_t *query, pupmgr_exit_t *reply);

#endif /* HW_PS4_LIVERPOOL_SAM_MODULES_SBL_PUPMGR_H */
