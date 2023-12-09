entry:
	JMP main

imageok:
	0xEA 0xAC 0xAA 0xEA

imagefalse:
	0xCE 0xAA 0xAA 0xAE

im0:
	0xE0 0xA0 0xA0 0xE0

im1:
	0xC0 0x40 0x40 0xE0

im2:
	0xE0 0x20 0xC0 0xE0

im3:
	0xE0 0x60 0x20 0xE0

im4:
	0xA0 0xE0 0x20 0x20

im5:
	0x60 0x40 0x20 0x40

im6:
	0xE0 0x80 0xE0 0xE0

im7:
	0xE0 0x20 0x20 0x20

im8:
	0xE0 0xE0 0xA0 0xE0

im9:
	0xE0 0xE0 0x20 0xE0

imA:
	0x40 0xA0 0xE0 0xA0

imE:
	0xE0 0xC0 0x80 0xE0

imF:
	0xE0 0x80 0xC0 0x80

imX:
	0xA0 0x40 0xA0 0xA0

main:
	LD   V8, 0x1
	LD   V9, 0x5
	LD   V10, 0xa
	LD   V11, 0x1
	LD   V5, 0x2a
	LD   V6, 0x2b
	LD   I, 0x216
	DRW  V8, V11, 4
	LD   I, 0x23e
	DRW  V9, V11, 4
	LD   I, 0x202
	SE   V6, 0x2b
	LD   I, 0x206
	DRW  V10, V11, 4
	LD   V11, 0x6
	LD   I, 0x21a
	DRW  V8, V11, 4
	LD   I, 0x23e
	DRW  V9, V11, 4
	LD   I, 0x206
	SNE  V5, 0x2a
	LD   I, 0x202
	DRW  V10, V11, 4
	LD   V11, 0xb
	LD   I, 0x21e
	DRW  V8, V11, 4
	LD   I, 0x23e
	DRW  V9, V11, 4
	LD   I, 0x206
	SE   V5, V6
	LD   I, 0x202
	DRW  V10, V11, 4
	LD   V11, 0x10
	LD   I, 0x226
	DRW  V8, V11, 4
	LD   I, 0x23e
	DRW  V9, V11, 4
	LD   I, 0x206
	ADD  V6, 0xff
	SNE  V6, 0x2a
	LD   I, 0x202
	DRW  V10, V11, 4
	LD   V11, 0x15
	LD   I, 0x22e
	DRW  V8, V11, 4
	LD   I, 0x23e
	DRW  V9, V11, 4
	LD   I, 0x206
	SNE  V5, V6
	LD   I, 0x202
	DRW  V10, V11, 4
	LD   V11, 0x1a
	LD   I, 0x232
	DRW  V8, V11, 4
	LD   I, 0x23e
	DRW  V9, V11, 4
	CALL testAX
	LD   V8, 0x17
	LD   V9, 0x1b
	LD   V10, 0x20
	LD   V11, 0x1
	LD   I, 0x20a
	DRW  V8, V11, 4
	LD   I, 0x236
	DRW  V9, V11, 4
	LD   I, 0x202
	DRW  V10, V11, 4
	LD   V11, 0x6
	LD   I, 0x22a
	DRW  V8, V11, 4
	LD   I, 0x20a
	DRW  V9, V11, 4
	LD   I, 0x206
	LD   V7, V5
	SNE  V7, 0x2a
	LD   I, 0x202
	DRW  V10, V11, 4
	LD   V11, 0xb
	LD   I, 0x22a
	DRW  V8, V11, 4
	LD   I, 0x20e
	DRW  V9, V11, 4
	LD   I, 0x206
	LD   V7, 0x2a
	OR   V7, V11
	SNE  V7, 0x2b
	LD   I, 0x202
	DRW  V10, V11, 4
	LD   V11, 0x10
	LD   I, 0x22a
	DRW  V8, V11, 4
	LD   I, 0x212
	DRW  V9, V11, 4
	LD   I, 0x206
	LD   V6, 0x78
	LD   V7, 0x1f
	AND  V7, V6
	SNE  V7, 0x18
	LD   I, 0x202
	DRW  V10, V11, 4
	LD   V11, 0x15
	LD   I, 0x22a
	DRW  V8, V11, 4
	LD   I, 0x216
	DRW  V9, V11, 4
	LD   I, 0x206
	LD   V6, 0x78
	LD   V7, 0x1f
	XOR  V7, V6
	SNE  V7, 0x67
	LD   I, 0x202
	DRW  V10, V11, 4
	LD   V11, 0x1a
	LD   I, 0x22a
	DRW  V8, V11, 4
	LD   I, 0x21a
	DRW  V9, V11, 4
	LD   I, 0x206
	LD   V6, 0x8c
	LD   V7, 0x8c
	ADD  V7, V6
	SNE  V7, 0x18
	LD   I, 0x202
	DRW  V10, V11, 4
	LD   V8, 0x2c
	LD   V9, 0x30
	LD   V10, 0x34
	LD   V11, 0x1
	LD   I, 0x22a
	DRW  V8, V11, 4
	LD   I, 0x21e
	DRW  V9, V11, 4
	LD   I, 0x206
	LD   V6, 0x8c
	LD   V7, 0x78
	SUB  V7, V6
	SNE  V7, 0xec
	LD   I, 0x202
	DRW  V10, V11, 4
	LD   V11, 0x6
	LD   I, 0x22a
	DRW  V8, V11, 4
	LD   I, 0x222
	DRW  V9, V11, 4
	LD   I, 0x206
	LD   V6, 0xe0
	SHL  V6, V6
	SNE  V6, 0xc0
	LD   I, 0x202
	DRW  V10, V11, 4
	LD   V11, 0xb
	LD   I, 0x22a
	DRW  V8, V11, 4
	LD   I, 0x236
	DRW  V9, V11, 4
	LD   I, 0x206
	LD   V6, 0xf
	SHR  V6, V6
	SNE  V6, 0x7
	LD   I, 0x202
	DRW  V10, V11, 4
	LD   V11, 0x10
	LD   I, 0x23a
	DRW  V8, V11, 4
	LD   I, 0x21e
	DRW  V9, V11, 4
	LD   I, 0x3e8
	LD   V0, 00
	LD   V1, 0x30
	LD   [I], V1
	LD   I, 0x3e9
	LD   V0, [I]
	LD   I, 0x206
	SNE  V0, 0x30
	LD   I, 0x202
	DRW  V10, V11, 4
	LD   V11, 0x15
	LD   I, 0x23a
	DRW  V8, V11, 4
	LD   I, 0x216
	DRW  V9, V11, 4
	LD   I, 0x3e8
	LD   V6, 0x89
	LD   B, V6
	LD   V2, [I]
	LD   I, 0x202
	SE   V0, 0x1
	LD   I, 0x206
	SE   V1, 0x3
	LD   I, 0x206
	SE   V2, 0x7
	LD   I, 0x206
	DRW  V10, V11, 4
	LD   V11, 0x1a
	LD   I, 0x20e
	DRW  V8, V11, 4
	LD   I, 0x23e
	DRW  V9, V11, 4
	JMP  test1X

testAX:
	LD  I, 0x202
	DRW V10, V11, 4
	RET

test1X:
	LD  I, 0x202
	DRW V10, V11, 4
	JMP endtest

endtest:
	JMP endtest