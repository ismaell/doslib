
#if TARGET_MSDOS == 32
static inline void draw_vrl1_vgax_modex_strip(unsigned char *draw,unsigned char *s) {
	const unsigned char stride = vga_state.vga_draw_stride;

	__asm {
		push	edx
		push	ecx
		push	ebx
		push	esi
		push	edi
		movzx	ebx,stride
		mov	edx,ebx
		dec	edx
		mov	esi,s
		mov	edi,draw
		cld

outloop:	lodsb
		cmp	al,0xFF
		jz	stoploop

		movzx	ecx,al		; CX = run length

		lodsb			; skip count
		mul	bl		; AX = stride * skip
		add	di,ax		; draw += stride * skip

		or	cl,cl		; is the high bit in CL set?
		jns	copystrip	; if not, then we copy CX bytes	

		; same color strip. next byte is the color to write
		and	cl,0x7F
		lodsb
		jcxz	outloop
colorloop:	stosb
		add	edi,edx
		loop	colorloop
		jmp	short outloop

		; pixels to copy
copystrip:	jcxz	outloop
copyloop:	lodsb
		stosb
		add	edi,edx
		loop	copyloop
		jmp	short outloop

stoploop:	pop	edi
		pop	esi
		pop	ebx
		pop	ecx
		pop	edx
	}
}
#else
static inline void draw_vrl1_vgax_modex_strip(unsigned char far *draw,unsigned char far *s) {
	const unsigned char stride = vga_state.vga_draw_stride;

	__asm {
		push	es
		push	ds
		push	dx
		push	cx
		push	bx
		push	si
		push	di
		mov	bl,stride
		xor	bh,bh
		mov	dx,bx
		dec	dx
		lds	si,s
		les	di,draw
		cld

outloop:	lodsb
		cmp	al,0xFF
		jz	stoploop

		xor	ch,ch
		mov	cl,al		; CX = run length

		lodsb			; skip count
		mul	bl		; AX = stride * skip
		add	di,ax		; draw += stride * skip

		or	cl,cl		; is the high bit in CL set?
		jns	copystrip	; if not, then we copy CX bytes	

		; same color strip. next byte is the color to write
		and	cl,0x7F
		lodsb
		jcxz	outloop
colorloop:	stosb
		add	di,dx
		loop	colorloop
		jmp	short outloop

		; pixels to copy
copystrip:	jcxz	outloop
copyloop:	lodsb
		stosb
		add	di,dx
		loop	copyloop
		jmp	short outloop

stoploop:	pop	di
		pop	si
		pop	bx
		pop	cx
		pop	dx
		pop	ds
		pop	es
	}
}
#endif

