.file "mCPYs.s"
.text
.globl	memcpy_asm_8_side
.type	memcpy_asm_8_side, @function
.att_syntax
memcpy_asm_8_side:		#AT&T инструкция источник, приёмник
							#Аргументы функции вводятся в регистры в следующем порядке их следования. ;%rdi,%rsi,%rdx,%rcx,%r8 и %r9;В %rax помещается 0

							# rbp начало стека
							# rsp - текущая позиция в стеке
							# rdi - void const* from
							# rsi - void* to
							# rdx - size_t size

	push %rbp
	push %rsp
	push %rdi
	push %rsi
	push %rdx

	movq	$0, %rcx

.forLoop8:
	cmpq	%rdx, %rcx
	jnb		.endLoop8
	movq	(%rdi), %rax
	movq 	%rax, (%rsi)
	addq	$8, %rdi
	addq	$8, %rsi
	addq	$8, %rcx
	jmp		.forLoop8

.endLoop8:

    pop %rdx
    pop %rsi
    pop %rdi
    pop %rsp
    pop %rbp

	ret

.globl	_Z18memcpy_asm_16_sidePKvPvm
.type	_Z18memcpy_asm_16_sidePKvPvm, @function
.att_syntax
_Z18memcpy_asm_16_sidePKvPvm:

    push %rbp
    push %rsp
    push %rdi
    push %rsi
    push %rdx

	movq	$0, %rcx

.loop_main:
	leaq	16(%rcx), %rbx
	cmpq	%rdx, %rbx
	jnb		.endLoop_main
	movups	(%rdi), %xmm0
	movups 	%xmm0, (%rsi)
	addq	$16, %rdi
	addq	$16, %rsi
	addq	$16, %rcx
	jmp		.loop_main

.endLoop_main:
	subq	 %rcx, %rdx
	call	memcpy_asm_8_side

	pop %rdx
    pop %rsi
    pop %rdi
    pop %rsp
    pop %rbp

	ret
