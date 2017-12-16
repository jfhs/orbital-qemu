/*
 * Miscellaneous macros.
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

#ifndef HW_PS4_MACROS_H
#define HW_PS4_MACROS_H

// Count arguments
#define __ARGUMENT_EXPAND(x) x
#define __ARGUMENT_COUNT(...) \
    __ARGUMENT_EXPAND(__ARGUMENT_EXTRACT(__VA_ARGS__, 4, 3, 2, 1, 0))
#define __ARGUMENT_EXTRACT(a1, a2, a3, a4, N, ...) N

// Dispatching macros
#define __MACRO_DISPATCH(function, ...) \
    __MACRO_SELECT(function, __ARGUMENT_COUNT(__VA_ARGS__))
#define __MACRO_SELECT(function, argc) \
    __MACRO_CONCAT(function, argc)
#define __MACRO_CONCAT(a, b) a##b

// MMIO macros
#define GET_HI(hi, lo) (hi)
#define GET_LO(hi, lo) (lo)
#define GET_MASK(hi, lo) \
    (((1 << ((hi)-(lo)+1)) - 1) << (lo))

#define MMIO_READ(...) \
    __MACRO_DISPATCH(MMIO_READ, __VA_ARGS__)(__VA_ARGS__)
#define MMIO_READ2(mmio, addr) \
    ((mmio)[(addr)>>2])
#define MMIO_READ3(mmio, addr, field) \
    ((MMIO_READ2(mmio, addr) & field(GET_MASK)) >> field(GET_LO))

#define MMIO_WRITE(...) \
    __MACRO_DISPATCH(MMIO_WRITE, __VA_ARGS__)(__VA_ARGS__)
#define MMIO_WRITE3(mmio, addr, value) \
    (mmio)[(addr)>>2] = (value)
#define MMIO_WRITE4(mmio, addr, field, value) \
    MMIO_WRITE3(mmio, addr, \
        ((MMIO_READ(mmio, addr) & ~field(GET_MASK)) | \
        (((value) << field(GET_LO)) & field(GET_MASK))))

#endif /* HW_PS4_MACROS_H */
