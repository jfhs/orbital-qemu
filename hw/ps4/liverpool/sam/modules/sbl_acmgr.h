/*
 * QEMU model of SBL's ACMgr module.
 *
 * Copyright (c) 2017-2018 Alexandro Sanchez Bach
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

#ifndef HW_PS4_LIVERPOOL_SAM_MODULES_SBL_ACMGR_H
#define HW_PS4_LIVERPOOL_SAM_MODULES_SBL_ACMGR_H

#include "qemu/osdep.h"

#define ACMGR_PATH_INVALID              0
#define ACMGR_PATH_SYSTEM               1
#define ACMGR_PATH_SYSTEM_EX            2
#define ACMGR_PATH_UPDATE               3
#define ACMGR_PATH_PREINST              4
#define ACMGR_PATH_PREINST2             5
#define ACMGR_PATH_PFSMNT               6
#define ACMGR_PATH_USB                  7
#define ACMGR_PATH_HOST                 8
#define ACMGR_PATH_ROOT                 9
#define ACMGR_PATH_DIAG                10
#define ACMGR_PATH_RDIAG               11

#endif /* HW_PS4_LIVERPOOL_SAM_MODULES_SBL_ACMGR_H */
