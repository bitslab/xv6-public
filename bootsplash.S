#include "mmu.h"
#include "asm.h"

.code16
.global start
start:

# anyway those registers may have already been cleared.
xorw %ax, %ax
movw %ax, %ds # clear ds

movb $0x3, %ah # get cursor position
int  $0x10

movb $0x13, %ah # parameters for int 10h / ah 13h
movb $1,    %al # write mode = 1
movw $0x0f, %bx # bh = 0, bg = 0(black) fg = 15(white)
movw $17,   %cx # length
lea  hello, %bp # bp is the address of the string
int  $0x10

movb $0,    %ah
int  $0x16      # wait for keyboard input

movb $0x0,  %ah
movb $0x13, %al
int  $0x10      # change to video mode

cli
lgdt    gdtdesc
movl    %cr0, %eax
orl     $CR0_PE, %eax
movl    %eax, %cr0

ljmp    $(SEG_KCODE<<3), $start32
.code32  # Tell assembler to generate 32-bit code now.
start32:

movw    $(SEG_KDATA<<3), %ax    # Our data segment selector
movw    %ax, %ds                # -> DS: Data Segment
movw    %ax, %es                # -> ES: Extra Segment
movw    %ax, %ss                # -> SS: Stack Segment

movl $0x0f0f0f0f,0xA0000
mov $0x1000, %esp

call splashmain

loop:
    jmp loop

hello: .string "xv6 @ UIC rocks!"

# Bootstrap GDT
.p2align 2                                # force 4 byte alignment
gdt:
  SEG_NULLASM                             # null seg
  SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)   # code seg
  SEG_ASM(STA_W, 0x0, 0xffffffff)         # data seg

gdtdesc:
  .word   (gdtdesc - gdt - 1)             # sizeof(gdt) - 1
  .long   gdt                             # address gdt

#.org 0x01fe  # or 510
#.word 0xAA55 # or .byte 0x55, 0xaa
