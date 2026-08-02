/*
  6502 emulator

  referencing: 
	  https://en.wikipedia.org/wiki/MOS_Technology_6502
	  https://6502.org
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

enum addr_mode {
	ADDR_MODE_IMM,
	ADDR_MODE_ZERO,
	ADDR_MODE_ZERO_X,
	ADDR_MODE_ZERO_Y,
	ADDR_MODE_ABS,
	ADDR_MODE_ABS_X,
	ADDR_MODE_ABS_Y,
	ADDR_MODE_IND_X,
	ADDR_MODE_IND_Y,
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
	if ((flags & PS_NEGATIVE) && ((int8_t)v < 0))
		m->reg[REG_PS] |= PS_NEGATIVE;
}

static uint8_t *resolve_address(struct machine *m, enum addr_mode mode)
{
	switch (mode) {
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

static inline uint8_t load_byte(struct machine *m, enum addr_mode mode)
{
	if (mode == ADDR_MODE_IMM)
		return step_u8(m);
	else
		return *resolve_address(m, mode);
}

static inline void store_byte(struct machine *m, enum addr_mode mode, uint8_t v)
{
	*resolve_address(m, mode) = v;
}

static inline void load_register(struct machine *m, enum machine_register reg,
					 enum addr_mode mode)
{
	m->reg[reg] = load_byte(m, mode);
	set_processor_status(m, PS_ZERO | PS_NEGATIVE, m->reg[reg]);
}

static inline void store_register(struct machine *m, enum machine_register reg,
				  enum addr_mode mode)
{
	store_byte(m, mode, m->reg[reg]);
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

static void machine_execute_instruction(struct machine *m)
{
	uint8_t op = step_u8(m);
	
	switch (op) {
	case 0xA9: load_register(m, REG_ACC, ADDR_MODE_IMM); return;
	case 0xA5: load_register(m, REG_ACC, ADDR_MODE_ZERO); return;
	case 0xB5: load_register(m, REG_ACC, ADDR_MODE_ZERO_X); return;
	case 0xAD: load_register(m, REG_ACC, ADDR_MODE_ABS); return;
	case 0xBD: load_register(m, REG_ACC, ADDR_MODE_ABS_X); return;
	case 0xB9: load_register(m, REG_ACC, ADDR_MODE_ABS_Y); return;
	case 0xA1: load_register(m, REG_ACC, ADDR_MODE_IND_X); return;
	case 0xB1: load_register(m, REG_ACC, ADDR_MODE_IND_Y); return;

	case 0xA2: load_register(m, REG_X, ADDR_MODE_IMM); return;
	case 0xA6: load_register(m, REG_X, ADDR_MODE_ZERO); return;
	case 0xB6: load_register(m, REG_X, ADDR_MODE_ZERO_Y); return;
	case 0xAE: load_register(m, REG_X, ADDR_MODE_ABS); return;
	case 0xBE: load_register(m, REG_X, ADDR_MODE_ABS_Y); return;

	case 0xA0: load_register(m, REG_Y, ADDR_MODE_IMM); return;
	case 0xA4: load_register(m, REG_Y, ADDR_MODE_ZERO); return;
	case 0xB4: load_register(m, REG_Y, ADDR_MODE_ZERO_X); return;
	case 0xAC: load_register(m, REG_Y, ADDR_MODE_ABS); return;
	case 0xBC: load_register(m, REG_Y, ADDR_MODE_ABS_X); return;
		
	case 0x85: store_register(m, REG_ACC, ADDR_MODE_ZERO); return;
	case 0x95: store_register(m, REG_ACC, ADDR_MODE_ZERO_X); return;
	case 0x8D: store_register(m, REG_ACC, ADDR_MODE_ABS); return;
	case 0x9D: store_register(m, REG_ACC, ADDR_MODE_ABS_X); return;
	case 0x99: store_register(m, REG_ACC, ADDR_MODE_ABS_Y); return;
	case 0x81: store_register(m, REG_ACC, ADDR_MODE_IND_X); return;
	case 0x91: store_register(m, REG_ACC, ADDR_MODE_IND_Y); return;

	case 0x86: store_register(m, REG_X, ADDR_MODE_ZERO); return;
	case 0x96: store_register(m, REG_X, ADDR_MODE_ZERO_Y); return;
	case 0x8E: store_register(m, REG_X, ADDR_MODE_ABS); return;

	case 0x84: store_register(m, REG_Y, ADDR_MODE_ZERO); return;
	case 0x94: store_register(m, REG_Y, ADDR_MODE_ZERO_X); return;
	case 0x8C: store_register(m, REG_Y, ADDR_MODE_ABS); return;

	case 0xAA: transfer_registers(m, REG_ACC, REG_X); return;
	case 0xA8: transfer_registers(m, REG_ACC, REG_Y); return;
	case 0x8A: transfer_registers(m, REG_X, REG_ACC); return;
	case 0x98: transfer_registers(m, REG_Y, REG_ACC); return;

	case 0xBA: transfer_registers(m, REG_SP, REG_X); return;
	case 0x9A: transfer_registers(m, REG_X, REG_SP); return;
	case 0x48: stack_push_register(m, REG_ACC); break;
	case 0x08: stack_push_register(m, REG_PS); break;
	case 0x68: stack_pull_register(m, REG_ACC); break;
	case 0x28: stack_pull_register(m, REG_PS); break;

	default:
		m->state = MACHINE_INVALID_OPCODE;
		fprintf(stderr, "INVALID OPCODE (%hhx)\n", op);
		break;
	}
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
