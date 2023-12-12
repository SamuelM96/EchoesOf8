#include "instructions.h"

#include <stdio.h>

void print_instruction(Chip8Instruction instruction, Chip8InstructionFormat format) {
	switch (format) {
	case R_FORMAT:
		printf("Opcode: %01hx\n", instruction.rformat.opcode);
		printf("Rx: %01hx\n", instruction.rformat.rx);
		printf("Ry: %01hx\n", instruction.rformat.ry);
		printf("Imm: %01hx\n", instruction.rformat.imm);
		break;
	case I_FORMAT:
		printf("Opcode: %01hx\n", instruction.iformat.opcode);
		printf("Reg: %01hx\n", instruction.iformat.reg);
		printf("Imm: %02hx\n", instruction.iformat.imm);
		break;
	case A_FORMAT:
		printf("Opcode: %01hx\n", instruction.aformat.opcode);
		printf("Addr: %03hx\n", instruction.aformat.addr);
		break;
	default:
		printf("Unknown instruction format: %d\n", format);
		break;
	}
}

void print_asm(Chip8Instruction instruction) {
	switch (instruction_type(instruction)) {
	case CHIP8_CLS:
		printf("CLS");
		break;
	case CHIP8_RET:
		printf("RET");
		break;
	case CHIP8_SYS_ADDR:
		printf("SYS %#03x", instruction.aformat.addr);
		break;
	case CHIP8_JMP_ADDR:
		printf("JMP %#03x", instruction.aformat.addr);
		break;
	case CHIP8_CALL_ADDR:
		printf("CALL %#03x", instruction.aformat.addr);
		break;
	case CHIP8_SE_VX_BYTE:
		printf("SE V%d, %#02x", instruction.iformat.reg, instruction.iformat.imm);
		break;
	case CHIP8_SNE_VX_BYTE:
		printf("SNE V%d, %#02x", instruction.iformat.reg, instruction.iformat.imm);
		break;
	case CHIP8_SE_VX_VY:
		printf("SE V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
		break;
	case CHIP8_LD_VX_BYTE:
		printf("LD V%d, %#02x", instruction.iformat.reg, instruction.iformat.imm);
		break;
	case CHIP8_ADD_VX_BYTE:
		printf("ADD V%d, %#02x", instruction.iformat.reg, instruction.iformat.imm);
		break;
	case CHIP8_LD_VX_VY:
		printf("LD V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
		break;
	case CHIP8_OR_VX_VY:
		printf("OR V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
		break;
	case CHIP8_AND_VX_VY:
		printf("AND V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
		break;
	case CHIP8_XOR_VX_VY:
		printf("XOR V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
		break;
	case CHIP8_ADD_VX_VY:
		printf("ADD V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
		break;
	case CHIP8_SUB_VX_VY:
		printf("SUB V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
		break;
	case CHIP8_SHR_VX:
		printf("SHR V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
		break;
	case CHIP8_SUBN_VX_VY:
		printf("SUBN V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
		break;
	case CHIP8_SHL_VX:
		printf("SHL V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
		break;
	case CHIP8_SNE_VX_VY:
		printf("SNE V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
		break;
	case CHIP8_LD_I_ADDR:
		printf("LD I, %#03x", instruction.aformat.addr);
		break;
	case CHIP8_JMP_V0_ADDR:
		printf("JMP V0, %#03x", instruction.aformat.addr);
		break;
	case CHIP8_RND_VX_BYTE:
		printf("RND V%d, %#02x", instruction.iformat.reg, instruction.iformat.imm);
		break;
	case CHIP8_DRW_VX_VY_NIBBLE:
		printf("DRW V%d, V%d, %d", instruction.rformat.rx, instruction.rformat.ry,
		       instruction.rformat.imm);
		break;
	case CHIP8_SKP_VX:
		printf("SKP V%d", instruction.iformat.reg);
		break;
	case CHIP8_SKNP_VX:
		printf("SKNP V%d", instruction.iformat.reg);
		break;
	case CHIP8_LD_VX_DT:
		printf("LD V%d, DT", instruction.iformat.reg);
		break;
	case CHIP8_LD_VX_K:
		printf("LD V%d, K", instruction.iformat.reg);
		break;
	case CHIP8_LD_DT_VX:
		printf("LD DT, V%d", instruction.iformat.reg);
		break;
	case CHIP8_LD_ST_VX:
		printf("LD ST, V%d", instruction.iformat.reg);
		break;
	case CHIP8_ADD_I_VX:
		printf("ADD I, V%d", instruction.iformat.reg);
		break;
	case CHIP8_LD_F_VX:
		printf("LD F, V%d", instruction.iformat.reg);
		break;
	case CHIP8_LD_B_VX:
		printf("LD B, V%d", instruction.iformat.reg);
		break;
	case CHIP8_LD_I_VX:
		printf("LD [I], V%d", instruction.iformat.reg);
		break;
	case CHIP8_LD_VX_I:
		printf("LD V%d, [I]", instruction.iformat.reg);
		break;
	default:
		printf("unknown");
		break;
	}
	printf("\n");
}

Chip8InstructionType instruction_type(Chip8Instruction instruction) {
	switch (instruction.aformat.opcode) {
	case 0:
		switch (instruction.aformat.addr) {
		case 0x000:
			return CHIP8_UNKNOWN;
		case 0x0E0:
			return CHIP8_CLS;
		case 0x0EE:
			return CHIP8_RET;
		default:
			return CHIP8_SYS_ADDR;
		}
		break;
	case 1:
		return CHIP8_JMP_ADDR;
	case 2:
		return CHIP8_CALL_ADDR;
	case 3:
		return CHIP8_SE_VX_BYTE;
	case 4:
		return CHIP8_SNE_VX_BYTE;
	case 5:
		return CHIP8_SE_VX_VY;
	case 6:
		return CHIP8_LD_VX_BYTE;
	case 7:
		return CHIP8_ADD_VX_BYTE;
	case 8:
		switch (instruction.rformat.imm) {
		case 0:
			return CHIP8_LD_VX_VY;
		case 1:
			return CHIP8_OR_VX_VY;
		case 2:
			return CHIP8_AND_VX_VY;
		case 3:
			return CHIP8_XOR_VX_VY;
		case 4:
			return CHIP8_ADD_VX_VY;
		case 5:
			return CHIP8_SUBN_VX_VY;
		case 6:
			return CHIP8_SHR_VX;
		case 7:
			return CHIP8_SUBN_VX_VY;
		case 0xE:
			return CHIP8_SHL_VX;
		default:
			return CHIP8_UNKNOWN;
		}
		break;
	case 9:
		return CHIP8_SNE_VX_VY;
	case 0xA:
		return CHIP8_LD_I_ADDR;
	case 0xB:
		return CHIP8_JMP_V0_ADDR;
	case 0xC:
		return CHIP8_RND_VX_BYTE;
	case 0xD:
		return CHIP8_DRW_VX_VY_NIBBLE;
	case 0xE:
		switch (instruction.iformat.imm) {
		case 0x9E:
			return CHIP8_SKP_VX;
		case 0xA1:
			return CHIP8_SKNP_VX;
		default:
			return CHIP8_UNKNOWN;
		}
		break;
	case 0xF:
		switch (instruction.iformat.imm) {
		case 0x07:
			return CHIP8_LD_VX_DT;
		case 0x0A:
			return CHIP8_LD_VX_K;
		case 0x15:
			return CHIP8_LD_DT_VX;
		case 0x18:
			return CHIP8_LD_ST_VX;
		case 0x1E:
			return CHIP8_ADD_I_VX;
		case 0x29:
			return CHIP8_LD_F_VX;
		case 0x33:
			return CHIP8_LD_B_VX;
		case 0x55:
			return CHIP8_LD_I_VX;
		case 0x65:
			return CHIP8_LD_VX_I;
		default:
			return CHIP8_UNKNOWN;
		}
		break;
	default:
		return CHIP8_UNKNOWN;
	}
}

Chip8InstructionFormat instruction_format(Chip8Instruction instruction) {
	switch (instruction_type(instruction)) {
	case CHIP8_CLS:
		return A_FORMAT;
	case CHIP8_RET:
		return A_FORMAT;
	case CHIP8_SYS_ADDR:
		return A_FORMAT;
	case CHIP8_JMP_ADDR:
		return A_FORMAT;
	case CHIP8_CALL_ADDR:
		return A_FORMAT;
	case CHIP8_SE_VX_BYTE:
		return I_FORMAT;
	case CHIP8_SNE_VX_BYTE:
		return I_FORMAT;
	case CHIP8_SE_VX_VY:
		return R_FORMAT;
	case CHIP8_LD_VX_BYTE:
		return I_FORMAT;
	case CHIP8_ADD_VX_BYTE:
		return I_FORMAT;
	case CHIP8_LD_VX_VY:
		return R_FORMAT;
	case CHIP8_OR_VX_VY:
		return R_FORMAT;
	case CHIP8_AND_VX_VY:
		return R_FORMAT;
	case CHIP8_XOR_VX_VY:
		return R_FORMAT;
	case CHIP8_ADD_VX_VY:
		return R_FORMAT;
	case CHIP8_SUB_VX_VY:
		return R_FORMAT;
	case CHIP8_SHR_VX:
		return R_FORMAT;
	case CHIP8_SUBN_VX_VY:
		return R_FORMAT;
	case CHIP8_SHL_VX:
		return R_FORMAT;
	case CHIP8_SNE_VX_VY:
		return R_FORMAT;
	case CHIP8_LD_I_ADDR:
		return A_FORMAT;
	case CHIP8_JMP_V0_ADDR:
		return A_FORMAT;
	case CHIP8_RND_VX_BYTE:
		return I_FORMAT;
	case CHIP8_DRW_VX_VY_NIBBLE:
		return R_FORMAT;
	case CHIP8_SKP_VX:
		return I_FORMAT;
	case CHIP8_SKNP_VX:
		return I_FORMAT;
	case CHIP8_LD_VX_DT:
		return I_FORMAT;
	case CHIP8_LD_VX_K:
		return I_FORMAT;
	case CHIP8_LD_DT_VX:
		return I_FORMAT;
	case CHIP8_LD_ST_VX:
		return I_FORMAT;
	case CHIP8_ADD_I_VX:
		return I_FORMAT;
	case CHIP8_LD_F_VX:
		return I_FORMAT;
	case CHIP8_LD_B_VX:
		return I_FORMAT;
	case CHIP8_LD_I_VX:
		return I_FORMAT;
	case CHIP8_LD_VX_I:
		return I_FORMAT;
	default:
		return UNKNOWN_FORMAT;
	}
}
