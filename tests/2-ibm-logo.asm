main:
	CLS
	LD  I, ibm_logo_1
	LD  V0, 0xc
	LD  V1, 0x8
	DRW V0, V1, 15
	ADD V0, 0x9
	LD  I, ibm_logo_2
	DRW V0, V1, 15
	LD  I, ibm_logo_3
	ADD V0, 0x8
	DRW V0, V1, 15
	ADD V0, 0x4
	LD  I, ibm_logo_4
	DRW V0, V1, 15
	ADD V0, 0x8
	LD  I, ibm_logo_5
	DRW V0, V1, 15
	ADD V0, 0x8
	LD  I, ibm_logo_6
	DRW V0, V1, 15

loop:
	JMP loop

ibm_logo_1:
	0xff00ff003c003c003c003c00ff00ff

ibm_logo_2:
	0xff00ff0038003f003f003800ff00ff

ibm_logo_3:
	0x8000e000e00080008000e000e00080

ibm_logo_4:
	0xf800fc003e003f003b003900f800f8

ibm_logo_5:
	0x030007000f00bf00fb00f300e30043

ibm_logo_6:
	0xe505e2008507810180028002e602e7
