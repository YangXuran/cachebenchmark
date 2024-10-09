/* Generic optimized memcpy using SIMD.
   Copyright (C) 2012-2024 Free Software Foundation, Inc.

   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

	.text
	.global memcpy_arm64
	.global _memcpy_arm64
#ifndef __APPLE__
	.type memcpy_arm64, %function
#endif

#define dstin	x0
#define src	x1
#define count	x2
#define dst	x3
#define srcend	x4
#define dstend	x5
#define A_l	x6
#define A_lw	w6
#define A_h	x7
#define B_l	x8
#define B_lw	w8
#define B_h	x9
#define C_lw	w10
#define tmp1	x14

#define A_q	q0
#define B_q	q1
#define C_q	q2
#define D_q	q3
#define E_q	q4
#define F_q	q5
#define G_q	q6
#define H_q	q7

memcpy_arm64:
_memcpy_arm64:
	// Function prologue
	// Save link register and any callee-saved registers here if necessary
	
	// Your assembly code here
	add	srcend, src, count
	add	dstend, dstin, count
	cmp	count, 128
	b.hi	copy_long
	cmp	count, 32
	b.hi	copy32_128

	/* Small copies: 0..32 bytes.  */
	cmp	count, 16
	b.lo	copy16
	ldr	A_q, [src]
	ldr	B_q, [srcend, -16]
	str	A_q, [dstin]
	str	B_q, [dstend, -16]
	ret

	/* Copy 8-15 bytes.  */
copy16:
	tbz	count, 3, copy8
	ldr	A_l, [src]
	ldr	A_h, [srcend, -8]
	str	A_l, [dstin]
	str	A_h, [dstend, -8]
	ret

	/* Copy 4-7 bytes.  */
copy8:
	tbz	count, 2, copy4
	ldr	A_lw, [src]
	ldr	B_lw, [srcend, -4]
	str	A_lw, [dstin]
	str	B_lw, [dstend, -4]
	ret

	/* Copy 0..3 bytes using a branchless sequence.  */
copy4:
	cbz	count, copy0
	lsr	tmp1, count, 1
	ldrb	A_lw, [src]
	ldrb	C_lw, [srcend, -1]
	ldrb	B_lw, [src, tmp1]
	strb	A_lw, [dstin]
	strb	B_lw, [dstin, tmp1]
	strb	C_lw, [dstend, -1]
copy0:
	ret

	.p2align 4
	/* Medium copies: 33..128 bytes.  */
copy32_128:
	ldp	A_q, B_q, [src]
	ldp	C_q, D_q, [srcend, -32]
	cmp	count, 64
	b.hi	copy128
	stp	A_q, B_q, [dstin]
	stp	C_q, D_q, [dstend, -32]
	ret

	.p2align 4
	/* Copy 65..128 bytes.  */
copy128:
	ldp	E_q, F_q, [src, 32]
	cmp	count, 96
	b.ls	copy96
	ldp	G_q, H_q, [srcend, -64]
	stp	G_q, H_q, [dstend, -64]
copy96:
	stp	A_q, B_q, [dstin]
	stp	E_q, F_q, [dstin, 32]
	stp	C_q, D_q, [dstend, -32]
	ret

	/* Align loop64 below to 16 bytes.  */
	nop

	/* Copy more than 128 bytes.  */
copy_long:
	/* Copy 16 bytes and then align src to 16-byte alignment.  */
	ldr	D_q, [src]
	and	tmp1, src, 15
	bic	src, src, 15
	sub	dst, dstin, tmp1
	add	count, count, tmp1	/* Count is now 16 too large.  */
	ldp	A_q, B_q, [src, 16]
	str	D_q, [dstin]
	ldp	C_q, D_q, [src, 48]
	subs	count, count, 128 + 16	/* Test and readjust count.  */
	b.ls	copy64_from_end
loop64:
	stp	A_q, B_q, [dst, 16]
	ldp	A_q, B_q, [src, 80]
	stp	C_q, D_q, [dst, 48]
	ldp	C_q, D_q, [src, 112]
	add	src, src, 64
	add	dst, dst, 64
	subs	count, count, 64
	b.hi	loop64

	/* Write the last iteration and copy 64 bytes from the end.  */
copy64_from_end:
	ldp	E_q, F_q, [srcend, -64]
	stp	A_q, B_q, [dst, 16]
	ldp	A_q, B_q, [srcend, -32]
	stp	C_q, D_q, [dst, 48]
	stp	E_q, F_q, [dstend, -64]
	stp	A_q, B_q, [dstend, -32]
	ret

	// Function epilogue
	// Restore link register and any callee-saved registers here if necessary
#ifndef __APPLE__
	.size memcpy_arm64, .-memcpy_arm64
#endif
