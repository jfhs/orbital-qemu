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

/* SOP2 Instructions */
GCN_HANDLER(SOP2, s_add)
GCN_HANDLER(SOP2, s_addc)
GCN_HANDLER(SOP2, s_and)
GCN_HANDLER(SOP2, s_andn2)
GCN_HANDLER(SOP2, s_ashr)
GCN_HANDLER(SOP2, s_bfe)
GCN_HANDLER(SOP2, s_bfm)
GCN_HANDLER(SOP2, s_cselect)
GCN_HANDLER(SOP2, s_lshl)
GCN_HANDLER(SOP2, s_lshr)
GCN_HANDLER(SOP2, s_max)
GCN_HANDLER(SOP2, s_min)
GCN_HANDLER(SOP2, s_mul)
GCN_HANDLER(SOP2, s_nand)
GCN_HANDLER(SOP2, s_nor)
GCN_HANDLER(SOP2, s_or)
GCN_HANDLER(SOP2, s_orn2)
GCN_HANDLER(SOP2, s_sub)
GCN_HANDLER(SOP2, s_subb)
GCN_HANDLER(SOP2, s_xnor)
GCN_HANDLER(SOP2, s_xor)

/* SOPK Instructions */
GCN_HANDLER(SOPK, s_call)
GCN_HANDLER(SOPK, s_cmovk)
GCN_HANDLER(SOPK, s_cmpk)
GCN_HANDLER(SOPK, s_movk)

/* SOP1 Instructions */
GCN_HANDLER(SOP1, s_mov)
GCN_HANDLER(SOP1, s_cmov)
GCN_HANDLER(SOP1, s_not)
GCN_HANDLER(SOP1, s_wqm)

/* SOPP Instructions */
GCN_HANDLER(SOPP, s_barrier)
GCN_HANDLER(SOPP, s_branch)
GCN_HANDLER(SOPP, s_cbranch_cdbgsys)
GCN_HANDLER(SOPP, s_cbranch_cdbgsys_and_user)
GCN_HANDLER(SOPP, s_cbranch_cdbgsys_or_user)
GCN_HANDLER(SOPP, s_cbranch_cdbguser)
GCN_HANDLER(SOPP, s_cbranch_execnz)
GCN_HANDLER(SOPP, s_cbranch_execz)
GCN_HANDLER(SOPP, s_cbranch_scc0)
GCN_HANDLER(SOPP, s_cbranch_scc1)
GCN_HANDLER(SOPP, s_cbranch_vccnz)
GCN_HANDLER(SOPP, s_cbranch_vccz)
GCN_HANDLER(SOPP, s_decperflevel)
GCN_HANDLER(SOPP, s_endpgm)
GCN_HANDLER(SOPP, s_icache_inv)
GCN_HANDLER(SOPP, s_incperflevel)
GCN_HANDLER(SOPP, s_nop)
GCN_HANDLER(SOPP, s_sendmsg)
GCN_HANDLER(SOPP, s_sendmsghalt)
GCN_HANDLER(SOPP, s_sethalt)
GCN_HANDLER(SOPP, s_setkill)
GCN_HANDLER(SOPP, s_setprio)
GCN_HANDLER(SOPP, s_sleep)
GCN_HANDLER(SOPP, s_trap)
GCN_HANDLER(SOPP, s_ttracedata)
GCN_HANDLER(SOPP, s_waitcnt)

/* VOP1 Instructions */
GCN_HANDLER(VOP1, v_mov)
GCN_HANDLER(VOP1, v_cvt)

/* VOP2 Instructions */
GCN_HANDLER(VOP2, v_add)
GCN_HANDLER(VOP2, v_addc)
GCN_HANDLER(VOP2, v_and)
GCN_HANDLER(VOP2, v_ashr)
GCN_HANDLER(VOP2, v_ashrrev)
GCN_HANDLER(VOP2, v_bfm)
GCN_HANDLER(VOP2, v_cvt_pk)
GCN_HANDLER(VOP2, v_cvt_pkaccum)
GCN_HANDLER(VOP2, v_cvt_pknorm)
GCN_HANDLER(VOP2, v_cvt_pkrtz)
GCN_HANDLER(VOP2, v_ldexp)
GCN_HANDLER(VOP2, v_lshl)
GCN_HANDLER(VOP2, v_lshlrev)
GCN_HANDLER(VOP2, v_lshr)
GCN_HANDLER(VOP2, v_lshrrev)
GCN_HANDLER(VOP2, v_mac)
GCN_HANDLER(VOP2, v_madak)
GCN_HANDLER(VOP2, v_madmk)
GCN_HANDLER(VOP2, v_max)
GCN_HANDLER(VOP2, v_max_legacy)
GCN_HANDLER(VOP2, v_min)
GCN_HANDLER(VOP2, v_min_legacy)
GCN_HANDLER(VOP2, v_mul)
GCN_HANDLER(VOP2, v_mul_hi)
GCN_HANDLER(VOP2, v_or)
GCN_HANDLER(VOP2, v_sub)
GCN_HANDLER(VOP2, v_subb)
GCN_HANDLER(VOP2, v_subbrev)
GCN_HANDLER(VOP2, v_subrev)
GCN_HANDLER(VOP2, v_xor)

/* VINTRP Instructions */
GCN_HANDLER(VINTRP, v_interp_p1)
GCN_HANDLER(VINTRP, v_interp_p2)
GCN_HANDLER(VINTRP, v_interp_mov)

/* VOP3a Instructions */
GCN_HANDLER(VOP3A, v_mad_legacy)
GCN_HANDLER(VOP3A, v_mad)
GCN_HANDLER(VOP3A, v_cubeid)
GCN_HANDLER(VOP3A, v_cubesc)
GCN_HANDLER(VOP3A, v_cubetc)
GCN_HANDLER(VOP3A, v_cubema)
GCN_HANDLER(VOP3A, v_bfe)
GCN_HANDLER(VOP3A, v_bfi)
GCN_HANDLER(VOP3A, v_fma)
GCN_HANDLER(VOP3A, v_lerp)
GCN_HANDLER(VOP3A, v_alignbit)
GCN_HANDLER(VOP3A, v_alignbyte)
GCN_HANDLER(VOP3A, v_mullit)
GCN_HANDLER(VOP3A, v_min3)
GCN_HANDLER(VOP3A, v_max3)
GCN_HANDLER(VOP3A, v_med3)
GCN_HANDLER(VOP3A, v_sad)
GCN_HANDLER(VOP3A, v_sad_hi)
GCN_HANDLER(VOP3A, v_div_fixup)
GCN_HANDLER(VOP3A, v_div_fmas)
GCN_HANDLER(VOP3A, v_mul_lo)
GCN_HANDLER(VOP3A, v_msad)
GCN_HANDLER(VOP3A, v_mqsad)
GCN_HANDLER(VOP3A, v_mqsad_pk)
GCN_HANDLER(VOP3A, v_qsad_pk)
GCN_HANDLER(VOP3A, v_trig_preop)

/* SMRD Instructions */
GCN_HANDLER(SMRD, s_load_dword)
GCN_HANDLER(SMRD, s_buffer_load_dword)
GCN_HANDLER(SMRD, s_dcache_inv_vol)
GCN_HANDLER(SMRD, s_memtime)
GCN_HANDLER(SMRD, s_dcache_inv)

/* MIMG Instructions */
GCN_HANDLER(MIMG, image_atomic_add)
GCN_HANDLER(MIMG, image_atomic_and)
GCN_HANDLER(MIMG, image_atomic_cmpswap)
GCN_HANDLER(MIMG, image_atomic_dec)
GCN_HANDLER(MIMG, image_atomic_fcmpswap)
GCN_HANDLER(MIMG, image_atomic_fmax)
GCN_HANDLER(MIMG, image_atomic_fmin)
GCN_HANDLER(MIMG, image_atomic_inc)
GCN_HANDLER(MIMG, image_atomic_or)
GCN_HANDLER(MIMG, image_atomic_smax)
GCN_HANDLER(MIMG, image_atomic_smin)
GCN_HANDLER(MIMG, image_atomic_sub)
GCN_HANDLER(MIMG, image_atomic_swap)
GCN_HANDLER(MIMG, image_atomic_umax)
GCN_HANDLER(MIMG, image_atomic_umin)
GCN_HANDLER(MIMG, image_atomic_xor)
GCN_HANDLER(MIMG, image_gather4)
GCN_HANDLER(MIMG, image_get_lod)
GCN_HANDLER(MIMG, image_get_resinfo)
GCN_HANDLER(MIMG, image_load)
GCN_HANDLER(MIMG, image_sample)
GCN_HANDLER(MIMG, image_store)

/* EXP Instructions */
GCN_HANDLER(EXP, exp)
