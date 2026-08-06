#pragma once

#include <stdint.h>

enum {
	PS_CARRY,
	PS_ZERO,
	PS_INTERRUPT_DISABLE,
	PS_DECIMAL_MODE,
	PS_BREAK,
	PS_OVERFLOW,
	PS_NEGATIVE,
};

enum machine_register {
	REG_A,
	REG_PS,
	REG_S,
	REG_X,
	REG_Y,
	
	REGISTER_END,
};

// missing program counter
#define REGISTER_COUNT	(REGISTER_END + 1)

enum instruction {
	INS_INVALID,

// JMA -> JMP ABSOLUTE
	INS_BIT, INS_JMP, INS_JMA, INS_STY, INS_LDY, INS_CPY, INS_CPX,
	INS_ORA, INS_AND, INS_EOR, INS_ADC, INS_STA, INS_LDA, INS_CMP, INS_SBC,
	INS_ASL, INS_ROL, INS_LSR, INS_ROR, INS_STX, INS_LDX, INS_DEC, INS_INC,

	INS_BRK, INS_JSR, INS_RTI, INS_RTS,
	INS_BPL, INS_BMI, INS_BVC, INS_BVS,
	INS_BCC, INS_BCS, INS_BNE, INS_BEQ,
	INS_PHP, INS_PLP, INS_PHA, INS_PLA,
	INS_DEY, INS_TAY, INS_INY, INS_INX,
	INS_CLC, INS_SEC, INS_CLI, INS_SEI,
	INS_TYA, INS_CLV, INS_CLD, INS_SED,
	INS_TXA, INS_TXS, INS_TAX, INS_TSX,
	INS_DEX, INS_NOP,

	INSTRUCTION_COUNT,
};

#define INSTRUCTION_INDEX_FIRST	(1)

#define INSTRUCTION_GROUPS	3
#define INSTRUCTION_GROUP_MAX	8

#define GROUP_ONE_START	INS_BIT
#define GROUP_ONE_END	INS_CPX
#define instruction_is_group_one(ins) \
	(GROUP_ONE_START <= (ins) && (ins) <= GROUP_ONE_END)

#define GROUP_TWO_START	INS_ORA
#define GROUP_TWO_END	INS_SBC
#define instruction_is_group_two(ins) \
	(GROUP_TWO_START <= (ins) && (ins) <= GROUP_TWO_END)

#define GROUP_THREE_START	INS_ASL
#define GROUP_THREE_END	INS_INC
#define instruction_is_group_three(ins) \
	(GROUP_THREE_START <= (ins) && (ins) <= GROUP_THREE_END)

#define GROUP_SPECIAL_START	INS_BRK
#define GROUP_SPECIAL_END	INS_NOP
#define instruction_is_group_special(ins) \
	(GROUP_SPECIAL_START <= (ins) && (ins) <= GROUP_SPECIAL_END)

enum addressing_mode {
	ADDR_MODE_INVALID,
	
	ADDR_MODE_NONE,
	ADDR_MODE_IMM,
	ADDR_MODE_ACC,
	ADDR_MODE_REL,
	ADDR_MODE_ZERO,
	ADDR_MODE_ZERO_X,
	ADDR_MODE_ZERO_Y,
	ADDR_MODE_ABS,
	ADDR_MODE_ABS_X,
	ADDR_MODE_ABS_Y,
	ADDR_MODE_IND,
	ADDR_MODE_IND_X,
	ADDR_MODE_IND_Y,
	
	ADDR_MODE_COUNT,
};

#define KB(n)	((n) * (2 << 10))

#define ZERO_PAGE_LEN		256
#define STACK_PAGE_LEN		256
#define STACK_PAGE_START	0xFF
#define MEMORY_AVAILABLE	KB(64)

#define INT_HANDLER		0xFFFA
#define POW_HANDLER		0XFFFC
#define IRQ_HANDLER		0xFFFE

#define CPU_CLOCK_RATE_US	3

#define opcode_group(o)			((o) >> 5)
#define opcode_addressing_mode(o)	(((o) >> 2) & 7)
#define opcode_instruction(o)		((o) & 3)

#define OPCODE_GROUPS		3
#define OPCODE_INSTRUCTIONS	8

/*
  riddling this with static and __attribute((unused)) is kinda nuts
  
  but we ball
*/

__attribute((unused)) static enum instruction instruction_from_opcode[OPCODE_GROUPS][OPCODE_INSTRUCTIONS] = {
	{ INS_BIT, INS_JMP, INS_JMA, INS_STY, INS_LDY, INS_CPY, INS_CPX },
	{ INS_ORA, INS_AND, INS_EOR, INS_ADC, INS_STA, INS_LDA, INS_CMP, INS_SBC },
	{ INS_ASL, INS_ROL, INS_LSR, INS_ROR, INS_STX, INS_LDX, INS_DEC, INS_INC },
};

__attribute((unused)) static enum instruction special_instruction_from_opcode[256] = {
	[0x00] = INS_BRK, [0x20] = INS_JSR, [0x40] = INS_RTI, [0x60] = INS_RTS,
	[0x10] = INS_BPL, [0x30] = INS_BMI, [0x50] = INS_BVC, [0x70] = INS_BVS,
	[0x90] = INS_BCC, [0xB0] = INS_BCS, [0xD0] = INS_BNE, [0xF0] = INS_BEQ,
	[0x08] = INS_PHP, [0x28] = INS_PLP,  [0x48] = INS_PHA, [0x68] = INS_PLA,
	[0x88] = INS_DEY, [0xA8] = INS_TAY, [0xC8] = INS_INY, [0xE8] = INS_INX,
	[0x18] = INS_CLC, [0x38] = INS_SEC, [0x58] = INS_CLI, [0x78] = INS_SEI,
	[0x98] = INS_TYA, [0xB8] = INS_CLV, [0xD8] = INS_CLD, [0xF8] = INS_SED,
	[0x8A] = INS_TXA, [0x9A] = INS_TXS, [0xAA] = INS_TAX, [0xBA] = INS_TSX,
	[0xCA] = INS_DEX, [0xEA] = INS_NOP,
};

__attribute((unused)) static enum addressing_mode addressing_mode_from_opcode[OPCODE_GROUPS][OPCODE_INSTRUCTIONS] = {
	{
		ADDR_MODE_IMM,
		ADDR_MODE_ZERO,
		ADDR_MODE_INVALID,
		ADDR_MODE_ABS,
		ADDR_MODE_INVALID,
		ADDR_MODE_ZERO_X,
		ADDR_MODE_INVALID,
		ADDR_MODE_ABS_X,
	},
	{
		ADDR_MODE_ZERO_X,
		ADDR_MODE_ZERO,
		ADDR_MODE_IMM,
		ADDR_MODE_ABS,
		ADDR_MODE_IND_Y,
		ADDR_MODE_IND_X,
		ADDR_MODE_ABS_Y,
		ADDR_MODE_ABS_X,
	},
	{
		ADDR_MODE_IMM,
		ADDR_MODE_ZERO,
		ADDR_MODE_ACC,
		ADDR_MODE_ABS,
		ADDR_MODE_INVALID,
		ADDR_MODE_ZERO_X,
		ADDR_MODE_INVALID,
		ADDR_MODE_ABS_X,
	}
};

__attribute((unused)) static const char *instruction_to_string[INSTRUCTION_COUNT] = {
	"INVALID",

	"BIT", "JMP", "JMA", "STY", "LDY", "CPY", "CPX",
	"ORA", "AND", "EOR", "ADC", "STA", "LDA", "CMP", "SBC",
	"ASL", "ROL", "LSR", "ROR", "STX", "LDX", "DEC", "INC",

	"BRK", "JSR", "RTI", "RTS",
	"BPL", "BMI", "BVC", "BVS",
	"BCC", "BCS", "BNE", "BEQ",
	"PHP", "PLP", "PHA", "PLA",
	"DEY", "TAY", "INY", "INX",
	"CLC", "SEC", "CLI", "SEI",
	"TYA", "CLV", "CLD", "SED",
	"TXA", "TXS", "TAX", "TSX",
	"DEX", "NOP",
};

__attribute((unused)) static uint8_t instruction_to_opcode[INSTRUCTION_COUNT] = {
	0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
	0x01, 0x11, 0x21, 0x31, 0x41, 0x51, 0x61, 0x71,
	0x02, 0x12, 0x22, 0x32, 0x42, 0x52, 0x62, 0x72,

	0x00, 0x20, 0x40, 0x60,
	0x10, 0x30, 0x50, 0x70,
	0x90, 0xB0, 0xD0, 0xF0,
	0x08, 0x28, 0x48, 0x68,
	0x88, 0xA8, 0xC8, 0xE8,
	0x18, 0x38, 0x58, 0x78,
	0x98, 0xB8, 0xD8, 0xF8,
	0x8A, 0x9A, 0xAA, 0xBA,
	0xCA, 0xEA,
};

__attribute((unused)) static enum addressing_mode addressing_mode_to_opcode[INSTRUCTION_GROUPS][ADDR_MODE_COUNT] = {
	{
		[ADDR_MODE_IMM] = 0x00,
		[ADDR_MODE_ZERO] = 0x01,
		[ADDR_MODE_ABS] = 0x03,
		[ADDR_MODE_ZERO_X] = 0x05,
		[ADDR_MODE_ABS_X] = 0x07,
	},
	{
		[ADDR_MODE_ZERO_X] = 0x00,
		[ADDR_MODE_ZERO] = 0x01,
		[ADDR_MODE_IMM] = 0x02,
		[ADDR_MODE_ABS] = 0x03,
		[ADDR_MODE_IND_Y] = 0x04,
		[ADDR_MODE_IND_X] = 0x05,
		[ADDR_MODE_ABS_Y] = 0x06,
		[ADDR_MODE_ABS_X] = 0x07,
	},
	{
		[ADDR_MODE_IMM] = 0x00,
		[ADDR_MODE_ZERO] = 0x01,
		[ADDR_MODE_ACC] = 0x02,
		[ADDR_MODE_ABS] = 0x03,
		[ADDR_MODE_ZERO_X] = 0x05,
		[ADDR_MODE_ABS_X] = 0x07,
	}
};
