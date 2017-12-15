/*
 * QEMU model of Liverpool.
 *
 * Copyright (c) 2017 Alexandro Sanchez Bach
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

#ifndef HW_PS4_LIVERPOOL_H
#define HW_PS4_LIVERPOOL_H

// Liverpool devices
#define TYPE_LIVERPOOL_ROOTC    "liverpool-rootc"
#define TYPE_LIVERPOOL_IOMMU    "liverpool-iommu"
#define TYPE_LIVERPOOL_GC       "liverpool-gc"
#define TYPE_LIVERPOOL_HDAC     "liverpool-hdac"
#define TYPE_LIVERPOOL_ROOTP    "liverpool-rootp"
#define TYPE_LIVERPOOL_DEV142E  "liverpool-dev142e"
#define TYPE_LIVERPOOL_DEV142F  "liverpool-dev142f"
#define TYPE_LIVERPOOL_DEV1430  "liverpool-dev1430"
#define TYPE_LIVERPOOL_DEV1431  "liverpool-dev1431"
#define TYPE_LIVERPOOL_DEV1432  "liverpool-dev1432"
#define TYPE_LIVERPOOL_DEV1433  "liverpool-dev1433"

// Memory
#define BASE_LIVERPOOL_GC_0 0xE0000000
#define BASE_LIVERPOOL_GC_1 0xE4000000
#define BASE_LIVERPOOL_GC_2 0xE4800000
#define BASE_LIVERPOOL_HDAC 0xE4840000

#endif /* HW_PS4_LIVERPOOL_H */
