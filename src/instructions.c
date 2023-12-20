#include "instructions.h"
#include "sds.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

sds inst2str(Chip8Instruction instruction) {
	sds buffer = sdsempty();
	// size_t buffer_len = 16;
	// char *buffer = malloc(buffer_len);

	switch (instruction_type(instruction)) {
	case CHIP8_CLS:
		memcpy(buffer, "CLS", 4);
		break;
	case CHIP8_RET:
		memcpy(buffer, "RET", 4);
		break;
	case CHIP8_SYS_ADDR:
		buffer = sdscatprintf(buffer, "SYS %#03x", instruction.aformat.addr);
		break;
	case CHIP8_JMP_ADDR:
		buffer = sdscatprintf(buffer, "JMP %#03x", instruction.aformat.addr);
		break;
	case CHIP8_CALL_ADDR:
		buffer = sdscatprintf(buffer, "CALL %#03x", instruction.aformat.addr);
		break;
	case CHIP8_SE_VX_BYTE:
		buffer = sdscatprintf(buffer, "SE V%hX, %#02x", instruction.iformat.reg,
				      instruction.iformat.imm);
		break;
	case CHIP8_SNE_VX_BYTE:
		buffer = sdscatprintf(buffer, "SNE V%hX, %#02x", instruction.iformat.reg,
				      instruction.iformat.imm);
		break;
	case CHIP8_SE_VX_VY:
		buffer = sdscatprintf(buffer, "SE V%hX, V%hX", instruction.rformat.rx,
				      instruction.rformat.ry);
		break;
	case CHIP8_LD_VX_BYTE:
		buffer = sdscatprintf(buffer, "LD V%hX, %#02x", instruction.iformat.reg,
				      instruction.iformat.imm);
		break;
	case CHIP8_ADD_VX_BYTE:
		buffer = sdscatprintf(buffer, "ADD V%hX, %#02x", instruction.iformat.reg,
				      instruction.iformat.imm);
		break;
	case CHIP8_LD_VX_VY:
		buffer = sdscatprintf(buffer, "LD V%hX, V%hX", instruction.rformat.rx,
				      instruction.rformat.ry);
		break;
	case CHIP8_OR_VX_VY:
		buffer = sdscatprintf(buffer, "OR V%hX, V%hX", instruction.rformat.rx,
				      instruction.rformat.ry);
		break;
	case CHIP8_AND_VX_VY:
		buffer = sdscatprintf(buffer, "AND V%hX, V%hX", instruction.rformat.rx,
				      instruction.rformat.ry);
		break;
	case CHIP8_XOR_VX_VY:
		buffer = sdscatprintf(buffer, "XOR V%hX, V%hX", instruction.rformat.rx,
				      instruction.rformat.ry);
		break;
	case CHIP8_ADD_VX_VY:
		buffer = sdscatprintf(buffer, "ADD V%hX, V%hX", instruction.rformat.rx,
				      instruction.rformat.ry);
		break;
	case CHIP8_SUB_VX_VY:
		buffer = sdscatprintf(buffer, "SUB V%hX, V%hX", instruction.rformat.rx,
				      instruction.rformat.ry);
		break;
	case CHIP8_SHR_VX:
		buffer = sdscatprintf(buffer, "SHR V%hX, V%hX", instruction.rformat.rx,
				      instruction.rformat.ry);
		break;
	case CHIP8_SUBN_VX_VY:
		buffer = sdscatprintf(buffer, "SUBN V%hX, V%hX", instruction.rformat.rx,
				      instruction.rformat.ry);
		break;
	case CHIP8_SHL_VX:
		buffer = sdscatprintf(buffer, "SHL V%hX, V%hX", instruction.rformat.rx,
				      instruction.rformat.ry);
		break;
	case CHIP8_SNE_VX_VY:
		buffer = sdscatprintf(buffer, "SNE V%hX, V%hX", instruction.rformat.rx,
				      instruction.rformat.ry);
		break;
	case CHIP8_LD_I_ADDR:
		buffer = sdscatprintf(buffer, "LD I, %#03x", instruction.aformat.addr);
		break;
	case CHIP8_JMP_V0_ADDR:
		buffer = sdscatprintf(buffer, "JMP V0, %#03x", instruction.aformat.addr);
		break;
	case CHIP8_RND_VX_BYTE:
		buffer = sdscatprintf(buffer, "RND V%hX, %#02x", instruction.iformat.reg,
				      instruction.iformat.imm);
		break;
	case CHIP8_DRW_VX_VY_NIBBLE:
		buffer = sdscatprintf(buffer, "DRW V%hX, V%hX, %d", instruction.rformat.rx,
				      instruction.rformat.ry, instruction.rformat.imm);
		break;
	case CHIP8_SKP_VX:
		buffer = sdscatprintf(buffer, "SKP V%hX", instruction.iformat.reg);
		break;
	case CHIP8_SKNP_VX:
		buffer = sdscatprintf(buffer, "SKNP V%hX", instruction.iformat.reg);
		break;
	case CHIP8_LD_VX_DT:
		buffer = sdscatprintf(buffer, "LD V%hX, DT", instruction.iformat.reg);
		break;
	case CHIP8_LD_VX_K:
		buffer = sdscatprintf(buffer, "LD V%hX, K", instruction.iformat.reg);
		break;
	case CHIP8_LD_DT_VX:
		buffer = sdscatprintf(buffer, "LD DT, V%hX", instruction.iformat.reg);
		break;
	case CHIP8_LD_ST_VX:
		buffer = sdscatprintf(buffer, "LD ST, V%hX", instruction.iformat.reg);
		break;
	case CHIP8_ADD_I_VX:
		buffer = sdscatprintf(buffer, "ADD I, V%hX", instruction.iformat.reg);
		break;
	case CHIP8_LD_F_VX:
		buffer = sdscatprintf(buffer, "LD F, V%hX", instruction.iformat.reg);
		break;
	case CHIP8_LD_B_VX:
		buffer = sdscatprintf(buffer, "LD B, V%hX", instruction.iformat.reg);
		break;
	case CHIP8_LD_I_VX:
		buffer = sdscatprintf(buffer, "LD [I], V%hX", instruction.iformat.reg);
		break;
	case CHIP8_LD_VX_I:
		buffer = sdscatprintf(buffer, "LD V%hX, [I]", instruction.iformat.reg);
		break;
	default:
		memcpy(buffer, "unknown", 8);
		break;
	}

	return buffer;
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
			return CHIP8_SUB_VX_VY;
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

Chip8InstructionFormat instruction_format(Chip8InstructionType type) {
	switch (type) {
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
