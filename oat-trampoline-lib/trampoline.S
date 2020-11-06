#define ASSEMBLY

.section .trampoline , "ax"

.global __cfv_icall
.global __cfv_ijmp
.global __cfv_ret

__cfv_icall:
	stp	x0, x1, [sp, #-144]!      /* store scratch registers */
	stp	x2, x3, [sp, #16]
	stp	x4, x5, [sp, #32]
	stp	x6, x7, [sp, #48]
	stp	x8, x9, [sp, #64]
	stp	x10, x11, [sp, #80]
	stp	x12, x13, [sp, #96]
	stp	x14, x15, [sp, #112]
	stp	x0, x30, [sp, #128]

        mov	x1, x30
	bl	cfv_icall               /* cfv_icall(dest, src) */

	ldp	x2, x3, [sp, #16]
	ldp	x4, x5, [sp, #32]
	ldp	x6, x7, [sp, #48]
	ldp	x8, x9, [sp, #64]
	ldp	x10, x11, [sp, #80]
	ldp	x12, x13, [sp, #96]
	ldp	x14, x15, [sp, #112]
	ldp	x0, x30, [sp, #128]
	ldp	x0, x1, [sp], #144 	/* restore scratch registers     */
	ret	

__cfv_ret:
	stp	x0, x1, [sp, #-144]!      /* store scratch registers */
	stp	x2, x3, [sp, #16]
	stp	x4, x5, [sp, #32]
	stp	x6, x7, [sp, #48]
	stp	x8, x9, [sp, #64]
	stp	x10, x11, [sp, #80]
	stp	x12, x13, [sp, #96]
	stp	x14, x15, [sp, #112]
	stp	x0, x30, [sp, #128]

        mov	x1, x30
	bl	cfv_ret               /* cfv_ret(dest, src) */

	ldp	x2, x3, [sp, #16]
	ldp	x4, x5, [sp, #32]
	ldp	x6, x7, [sp, #48]
	ldp	x8, x9, [sp, #64]
	ldp	x10, x11, [sp, #80]
	ldp	x12, x13, [sp, #96]
	ldp	x14, x15, [sp, #112]
	ldp	x0, x30, [sp, #128]
	ldp	x0, x1, [sp], #144 	/* restore scratch registers     */
	ret	

__cfv_ijmp:
	stp	x0, x1, [sp, #-144]!      /* store scratch registers */
	stp	x2, x3, [sp, #16]
	stp	x4, x5, [sp, #32]
	stp	x6, x7, [sp, #48]
	stp	x8, x9, [sp, #64]
	stp	x10, x11, [sp, #80]
	stp	x12, x13, [sp, #96]
	stp	x14, x15, [sp, #112]
	stp	x0, x30, [sp, #128]

        mov	x1, x30
	bl	cfv_ijmp               /* cfv_ijmp(dest, src) */

	ldp	x2, x3, [sp, #16]
	ldp	x4, x5, [sp, #32]
	ldp	x6, x7, [sp, #48]
	ldp	x8, x9, [sp, #64]
	ldp	x10, x11, [sp, #80]
	ldp	x12, x13, [sp, #96]
	ldp	x14, x15, [sp, #112]
	ldp	x0, x30, [sp, #128]
	ldp	x0, x1, [sp], #144 	/* restore scratch registers     */
	ret	
