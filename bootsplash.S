.code16
.globl start

start:
	movb %dl, bootdrive
	
	mov $0,%ax
	mov %ax,%es
	movb $0x13,%ah
	movw $0x0f,%bx
	movb $10,%dh
	movb $0,%dl
	movb $1,%al
	movb $17,%cl
	lea hw,%bp
	int $0x10

	movb $0,%ah
	int $0x16

	movb $0x0,%ah
	movb $0x13,%al
	int $0x10

	movw $0x7d00,%si
	movw $0,%ax
	movw %ax,%ds
	movb $0x00,%dh
	movb bootdrive,%dl
	movw $0x4200,%ax
	int $0x13

here: 	
	jmp here
.org 0x100
DAP:
	.byte 0x10,0x00
        .word 0x007d
	.word 0x0, 0xa000
	.quad 1

	// or
	//       .byte 0x10,0x00,0x7d,0x00
	//       .byte 0x00,0x00,0x00,0xa0
	//       .byte 1,0,0,0,0,0,0,0
hw: .string "xv6 @ UIC rocks!"
bootdrive:	.byte 0x00

.org 0x01fe
 .word 0xAA55
