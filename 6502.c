/*
  6502 emulator

  referencing: 
	  https://en.wikipedia.org/wiki/MOS_Technology_6502
	  https://6502.org
	  https://llx.com/Neil/a2/opcodes.html (!!!)
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#define unreachable(f) \
	do { fprintf(stderr, "unreachable: %s\n", f); abort(); } while (0)

#define KB(n)	((n) * (2 << 10))

#define CPU_CLOCK_RATE_US	3

#define ZERO_PAGE_LEN		256
#define STACK_PAGE_LEN		256
#define STACK_PAGE_START	0xFF
#define MAX_AVAILABLE_MEMORY	KB(64)

#define OPCODE_AAA(o)	((o) >> 5)
#define OPCODE_BBB(o)	(((o) >> 2) & 7)
#define OPCODE_CC(o)	((o) & 3)

enum instruction {
	INS_INVALID,

	INS_ORA, INS_AND, INS_EOR, INS_ADC,
	INS_STA, INS_LDA, INS_CMP, INS_SBC,
	INS_ASL, INS_ROS, INS_LSR, INS_ROR,
	INS_STX, INS_LDX, INS_DEC, INS_INC,
	INS_BIT, INS_JMP, INS_JMA, INS_STY,
	INS_LDY, INS_CPY, INS_CPX,

	INS_BRK, INS_JSR, INS_RTI, INS_RTS,

	INS_PHP, INS_PLP, INS_PHA, INS_PLA,
	INS_DEY, INS_TAY, INS_INY, INS_INX,
	INS_CLC, INS_SEC, INS_CLI, INS_SEI,
	INS_TYA, INS_CLV, INS_CLD, INS_SED,
	INS_TXA, INS_TXS, INS_TAX, INS_TSX,
	INS_DEX, INS_NOP,
};

enum addressing_mode {
	ADDR_MODE_INVALID,
	ADDR_MODE_IMM,
	ADDR_MODE_ACC,
	ADDR_MODE_ZERO,
	ADDR_MODE_ZERO_X,
	ADDR_MODE_ZERO_Y,
	ADDR_MODE_ABS,
	ADDR_MODE_ABS_X,
	ADDR_MODE_ABS_Y,
	ADDR_MODE_IND_X,
	ADDR_MODE_IND_Y,
};

#define INSTRUCTION_GROUPS	3
#define INSTRUCTION_GROUP_MAX	8

static enum instruction instruction_table[INSTRUCTION_GROUPS][INSTRUCTION_GROUP_MAX] = {
	{ INS_BIT, INS_JMP, INS_JMA, INS_STY, INS_LDY, INS_CPY, INS_CPX, INS_INVALID },
	{ INS_ORA, INS_AND, INS_EOR, INS_ADC, INS_STA, INS_LDA, INS_CMP, INS_SBC },
	{ INS_ASL, INS_ROS, INS_LSR, INS_ROR, INS_STX, INS_LDX, INS_DEC, INS_INC },
};

static enum instruction special_instruction_table[256] = {
	[0x00] = INS_BRK, [0x20] = INS_JSR, [0x40] = INS_RTI, [0x60] = INS_RTS,
	[0x08] = INS_PHP, [0x28] = INS_PLP,  [0x48] = INS_PHA, [0x68] = INS_PLA,
	[0x88] = INS_DEY, [0xA8] = INS_TAY, [0xC8] = INS_INY, [0xE8] = INS_INX,
	[0x18] = INS_CLC, [0x38] = INS_SEC, [0x58] = INS_CLI, [0x78] = INS_SEI,
	[0x98] = INS_TYA, [0xB8] = INS_CLV, [0xD8] = INS_CLD, [0xF8] = INS_SED,
	[0x8A] = INS_TXA, [0x9A] = INS_TXS, [0xAA] = INS_TAX, [0xBA] = INS_TSX,
	[0xCA] = INS_DEX, [0xEA] = INS_NOP,
};

static enum addressing_mode addressing_mode_table[INSTRUCTION_GROUPS][INSTRUCTION_GROUP_MAX] = {
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

enum {
	PS_CARRY		= 0001,
	PS_ZERO			= 0002,
	PS_INTERRUPT_DISABLE	= 0004,
	PS_DECIMAL_MODE		= 0010,
	PS_BREAK		= 0020,
	PS_OVERFLOW		= 0040,
	PS_NEGATIVE		= 0100,
};

#define PS_NEGATIVE_MASK	(1U << 7)
#define PS_OVERFLOW_MASK	(1U << 6)

enum machine_state {
	MACHINE_OK,
	MACHINE_INVALID_OPCODE,
	MACHINE_OUT_OF_BOUNDS,
	MACHINE_STACK_OVERFLOW,
	MACHINE_STACK_UNDERFLOW,
};

enum machine_register {
	REG_ACC,
	REG_PS,
	REG_SP,
	REG_X,
	REG_Y,
	TOTAL_CPU_REGS,
};

struct machine {
	enum machine_state state;

	enum addressing_mode addr_mode;
	
	size_t rom_size;
	const uint8_t *rom;
	
	uint16_t pc;
	uint8_t reg[TOTAL_CPU_REGS];
	
	uint8_t zero_page[ZERO_PAGE_LEN];
	uint8_t stack[STACK_PAGE_LEN];
	uint8_t memory[MAX_AVAILABLE_MEMORY];
};

static void machine_init(struct machine *m, const uint8_t *rom, size_t size);
static int machine_run(struct machine *m);

static uint8_t *read_bytes(const char *path, size_t *datlen)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return 0;

	if (fseek(f, 0, SEEK_END))
		goto cleanup_file;
	size_t filelen = ftell(f);
	rewind(f);

	uint8_t *dat = calloc(filelen, sizeof(*dat));
	if (!dat)
		goto cleanup_file;
	size_t len = fread(dat, sizeof(*dat), filelen, f);

	*datlen = len;
	return dat;

cleanup_file:
	fclose(f);
	return 0;
}

static inline void dump_bytes(const uint8_t *rom, size_t size)
{
	for (size_t i = 0; i < size; i++)
		printf("%02hhx%c", rom[i],
		       ((i + 1) % 16 && i + 1 != size) ? ' ' : '\n');
}

static void log_machine_info(const struct machine *m)
{
	printf("6502 microprocessor diagnostics\n");
	
	printf("ROM (%zuB)\n", m->rom_size);
	dump_bytes(m->rom, m->rom_size);

	printf("state: ");
	switch (m->state) {
	case MACHINE_OK:		printf("OK"); break;
	case MACHINE_INVALID_OPCODE:	printf("INVALID OPCODE"); break;
	case MACHINE_OUT_OF_BOUNDS:	printf("OUT OF BOUNDS"); break;
	case MACHINE_STACK_OVERFLOW:	printf("STACK OVERFLOW"); break;
	case MACHINE_STACK_UNDERFLOW:	printf("STACK UNDERFLOW"); break;
	default: unreachable("log_machine_info");
	}
	printf("\n");
	
	printf("program counter: 0x%02hx\n", m->pc);
	printf("accumulator: %02hhx\n", m->reg[REG_ACC]);
	
	printf("processor status:\n");
	printf("  carry flag: %d\n",
	       (m->reg[REG_PS] & PS_CARRY) != 0);
	printf("  zero flag: %d\n",
	       (m->reg[REG_PS] & PS_ZERO) != 0);
	printf("  decimal mode: %d\n",
	       (m->reg[REG_PS] & PS_DECIMAL_MODE) != 0);
	printf("  interrupt disable: %d\n",
	       (m->reg[REG_PS] & PS_INTERRUPT_DISABLE) != 0);
	printf("  break: %d\n",
	       (m->reg[REG_PS] & PS_BREAK) != 0);
	printf("  overflow flag: %d\n",
	       (m->reg[REG_PS] & PS_OVERFLOW) != 0);
	printf("  negative flag: %d\n",
	       (m->reg[REG_PS] & PS_NEGATIVE) != 0);

	printf("stack pointer: 0x%hhx\n", m->reg[REG_SP]);
	printf("X register: %02hhx\n", m->reg[REG_X]);
	printf("Y register: %02hhx\n", m->reg[REG_Y]);
}

static inline void usage(void)
{
	fprintf(stderr, "usage: ./6502 ROM\n");
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		usage();
		return 1;
	}

	const char *path = argv[1];

	size_t size = 0;
	uint8_t *rom = read_bytes(path, &size);
	if (!rom) {
		perror(path);
		return 1;
	}

	if (size > MAX_AVAILABLE_MEMORY) {
		fprintf(stderr, "%s is too large\n", path);
		return 1;
	}

	struct machine m = { 0 };
	machine_init(&m, rom, size);

	machine_run(&m);

	log_machine_info(&m);

	return m.state != MACHINE_OK;
}

static void machine_init(struct machine *m, const uint8_t *rom, size_t size)
{
	m->state = MACHINE_OK;
	m->addr_mode = ADDR_MODE_INVALID;
	m->rom = rom;
	m->rom_size = size;
	m->reg[REG_SP] = STACK_PAGE_START;
}

/*
  since the error is already propagated through the machine structure
  itself, these functions (machine_step_*) simply return 0
 */

static uint8_t step_u8(struct machine *m)
{
	if (m->pc + 1U <= m->rom_size) {
		return m->rom[m->pc++];
	} else {
		m->state = MACHINE_OUT_OF_BOUNDS;
		return 0;
	}
}

static uint16_t step_u16(struct machine *m) {
	/*
	  all values in 6502 machine code are in little endian format
	 */
	if (m->pc + 2U <= m->rom_size) {
		uint8_t lo = m->rom[m->pc++];
		uint8_t hi = m->rom[m->pc++];
		return (hi << 8) | lo;
	} else {
		m->state = MACHINE_OUT_OF_BOUNDS;
		return 0;
	}
}

static void set_processor_status(struct machine *m, uint8_t flags, uint8_t v)
{
	/*
	  remember to add the rest when needed!!!
	 */
	if ((flags & PS_ZERO) && (v == 0))
		m->reg[REG_PS] |= PS_ZERO;
	if ((flags & PS_NEGATIVE) && (v & PS_NEGATIVE_MASK))
		m->reg[REG_PS] |= PS_NEGATIVE;
	if ((flags & PS_OVERFLOW) && (v & PS_OVERFLOW_MASK))
		m->reg[REG_PS] |= PS_OVERFLOW;
}

static uint8_t *resolve_address(struct machine *m)
{
	switch (m->addr_mode) {
	case ADDR_MODE_ZERO: {
		uint8_t off = step_u8(m);
		return &m->zero_page[off];
	}

	case ADDR_MODE_ZERO_X: {
		uint8_t off = step_u8(m) + m->reg[REG_X];
		return &m->zero_page[off];
	}

	case ADDR_MODE_ZERO_Y: {
		uint8_t off = step_u8(m) + m->reg[REG_Y];
		return &m->zero_page[off];
	}

	case ADDR_MODE_ABS: {
		uint16_t addr = step_u16(m);
		return &m->memory[addr];
	}

	case ADDR_MODE_ABS_X: {
		uint16_t addr = step_u16(m) + m->reg[REG_X];
		return &m->memory[addr];
	}

	case ADDR_MODE_ABS_Y: {
		uint16_t addr = step_u16(m) + m->reg[REG_Y];
		return &m->memory[addr];
	}

	case ADDR_MODE_IND_X: {
		uint8_t off = step_u8(m) + m->reg[REG_X];
		uint16_t addr = (m->zero_page[(uint8_t)(off + 1)] << 8)
			| m->zero_page[off];
		return &m->memory[addr];
	}

	case ADDR_MODE_IND_Y: {
		uint8_t off = step_u8(m);
		uint16_t addr = (m->zero_page[(uint8_t)(off + 1)] << 8)
			| m->zero_page[off];
		return &m->memory[addr + m->reg[REG_Y]];
	}

	default: unreachable("machine_resolve_address");
	}
}

static inline uint8_t load_byte(struct machine *m)
{
	if (m->addr_mode == ADDR_MODE_IMM)
		return step_u8(m);
	else
		return *resolve_address(m);
}

static inline void store_byte(struct machine *m, uint8_t v)
{
	*resolve_address(m) = v;
}

static inline void load_register(struct machine *m, enum machine_register reg)
{
	m->reg[reg] = load_byte(m);
	set_processor_status(m, PS_ZERO | PS_NEGATIVE, m->reg[reg]);
}

static inline void store_register(struct machine *m, enum machine_register reg)
{
	store_byte(m, m->reg[reg]);
}

static inline void transfer_registers(struct machine *m,
				      enum machine_register reg_src,
				      enum machine_register reg_dst)
{
	m->reg[reg_dst] = m->reg[reg_src];
	if (reg_dst != REG_SP)
		set_processor_status(m, PS_ZERO | PS_NEGATIVE, m->reg[reg_dst]);
}

static inline void stack_push_register(struct machine *m,
				       enum machine_register reg)
{
	if (m->reg[REG_SP] != 0)
		m->stack[(m->reg[REG_SP])--] = m->reg[reg];
	else
		m->state = MACHINE_STACK_OVERFLOW;
}

static inline void stack_pull_register(struct machine *m,
				       enum machine_register reg)
{
	if (m->reg[REG_SP] + 1 <= STACK_PAGE_LEN)
		m->reg[reg] = m->stack[++(m->reg[REG_SP])];
	else
		m->state = MACHINE_STACK_UNDERFLOW;
}

static inline void logical_and(struct machine *m)
{
	uint8_t v = m->reg[REG_ACC] & load_byte(m);
	set_processor_status(m, PS_ZERO | PS_NEGATIVE, v);
}

static inline void logical_xor(struct machine *m)
{
	uint8_t v = m->reg[REG_ACC] ^ load_byte(m);
	set_processor_status(m, PS_ZERO | PS_NEGATIVE, v);
}

static inline void logical_or(struct machine *m)
{
	uint8_t v = m->reg[REG_ACC] | load_byte(m);
	set_processor_status(m, PS_ZERO | PS_NEGATIVE, v);
}

static inline void bit_test(struct machine *m)
{
	uint8_t v = m->reg[REG_ACC] & load_byte(m);
	set_processor_status(m, PS_ZERO | PS_NEGATIVE | PS_OVERFLOW, v);
}

static enum instruction instruction_lookup(struct machine *m, uint8_t op)
{
	enum instruction ins = special_instruction_table[op];
	if (ins != INS_INVALID)
		return ins;
	
	uint8_t op_hi = OPCODE_AAA(op);
	uint8_t op_addr_mode = OPCODE_BBB(op);
	uint8_t op_lo = OPCODE_CC(op);
	
	m->addr_mode = addressing_mode_table[op_lo][op_addr_mode];
	if (m->addr_mode == ADDR_MODE_INVALID)
		return INS_INVALID;
	
	return instruction_table[op_lo][op_hi];
}

static void machine_execute_instruction(struct machine *m)
{
	uint8_t op = step_u8(m);
	
	enum instruction ins = instruction_lookup(m, op);
	if (ins == INS_INVALID)
		goto invalid_opcode;

	switch (ins) {
	case INS_LDA: load_register(m, REG_ACC); break;
	case INS_LDX: {
		if (m->addr_mode == ADDR_MODE_ZERO_X)
			m->addr_mode = ADDR_MODE_ZERO_Y;
		else if (m->addr_mode == ADDR_MODE_ABS_X)
			m->addr_mode = ADDR_MODE_ABS_Y;
		
		load_register(m, REG_X); break;
	} break;
	case INS_LDY: load_register(m, REG_Y); break;
	case INS_STA: {
		if (m->addr_mode == ADDR_MODE_IMM)
			goto invalid_opcode;
		
		store_register(m, REG_ACC);
	} break;
	case INS_STX: {
		if (m->addr_mode == ADDR_MODE_ZERO_X)
			m->addr_mode = ADDR_MODE_ZERO_Y;
		
		store_register(m, REG_X);
	} break;
	case INS_STY: store_register(m, REG_Y); break;

	case INS_AND: logical_and(m); break;
	case INS_EOR: logical_xor(m); break;
	case INS_ORA: logical_or(m); break;
	case INS_BIT: bit_test(m); break;

	case INS_TAX: transfer_registers(m, REG_ACC, REG_X); break;
	case INS_TAY: transfer_registers(m, REG_ACC, REG_Y); break;
	case INS_TXA: transfer_registers(m, REG_X, REG_ACC); break;
	case INS_TYA: transfer_registers(m, REG_Y, REG_ACC); break;

	case INS_TSX: transfer_registers(m, REG_SP, REG_X); break;
	case INS_TXS: transfer_registers(m, REG_X, REG_SP); break;
	case INS_PHA: stack_push_register(m, REG_ACC); break;
	case INS_PHP: stack_push_register(m, REG_PS); break;
	case INS_PLA: stack_pull_register(m, REG_ACC); break;
	case INS_PLP: stack_pull_register(m, REG_PS); break;

	default: goto invalid_opcode;
	}

	return;

invalid_opcode:
	m->state = MACHINE_INVALID_OPCODE;
	fprintf(stderr, "INVALID OPCODE (%hhx)\n", op);
	return;
}

static inline void clock_tick(void)
{
	usleep(CPU_CLOCK_RATE_US);
}

static int machine_run(struct machine *m)
{
	int err = 0;
	
	while (m->pc < m->rom_size) {
		machine_execute_instruction(m);
		if (m->state != MACHINE_OK)
			break;
		clock_tick();
	}

	return err;
}
