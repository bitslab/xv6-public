.code16
.globl start

start:
movb %dl, bdnr # save boot drive number in memory

# anyway those registers may have already been cleared.
xorw %ax, %ax
movw %ax, %ds # clear ds
movw %ax, %es # clear es

movb $0x13, %ah # parameters for int 13h
movb $1,    %al # write mode = 1
movw $0x0f, %bx # bh = 0, bg = 0(black) fg = 15(white)
movb $10,   %dh # the 10th row
movb $0,    %dl # starts from the left
movw $17,   %cx # length
lea  hello, %bp # bp is the address of the string (es has been cleared)
int  $0x10

movb $0,    %ah
int  $0x16      # wait for keyboard input

movb $0x0,  %ah
movb $0x13, %al
int  $0x10      # change to video mode

movb $0x42, %ah
movb bdnr,  %dl
movw $DAP,  %si
int  $0x13      # load image


movb $0,    %ah
int  $0x16      # wait for keyboard input

# hint: what's the background color code?
movw $0x10, %ah
movw $0x10, %al # set DAC color register, see doc for %AL=10h
movw $0xf, %bx  # color register (how many possible values? see doc of video mode)
movb $63, %dh   # full red
movb $0, %ch    # no green
movb $0, %cl    # no blue
int  $0x10

loop:
jmp loop

DAP:
.byte 0x10, 0x00   # size, 0
.word 0x007d       # number of blocks (size of cover.raw / 512)
.word 0x0,  0xa000 # 0xa000:0x0000 => 0xa0000
.quad 1            # starting from block 1

hello: .string "xv6 @ UIC rocks!"

bdnr: .byte 0x00   # a place-holder for storing the boot drive number

.org 0x01fe  # or 510
.word 0xAA55 # or .byte 0x55, 0xaa
