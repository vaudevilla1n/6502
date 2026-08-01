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

enum proc_register {
	REG_ACC,
	REG_PS,
	REG_SP,
	REG_X,
	REG_Y,
	TOTAL_CPU_REGS,
};

enum machine_state {
	MACHINE_OK,
	MACHINE_INVALID_OPCODE,
	MACHINE_OUT_OF_BOUNDS,
};

struct machine {
	enum machine_state state;
	
	size_t rom_size;
	uint8_t *rom;
	
	uint16_t pc;
	uint8_t reg[TOTAL_CPU_REGS];
	
	uint8_t zero_page[ZERO_PAGE_LEN];
	uint8_t stack[STACK_PAGE_LEN];
	uint8_t memory[MAX_AVAILABLE_MEMORY];
};

#define MACHINE_INVALID_ADDRESS(m)	((m)->memory)

static void machine_init(struct machine *m, uint8_t *rom, size_t size);
static int machine_run(struct machine *m);

static uint8_t *read_bytes(const char *path, size_t *datlen);

static void log_machine_info(const struct machine *m);

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
	case MACHINE_OK:		printf("ok"); break;
	case MACHINE_INVALID_OPCODE:	printf("invalid opcode"); break;
	case MACHINE_OUT_OF_BOUNDS:	printf("out of bounds"); break;
	default: __builtin_unreachable();
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

static void machine_init(struct machine *m, uint8_t *rom, size_t size)
{
	m->state = MACHINE_OK;
	m->rom = rom;
	m->rom_size = size;
	m->reg[REG_SP] = STACK_PAGE_START;
}

/*
  since the error is already propagated through the machine structure
  itself, these functions (machine_step_*) simply return a pointer to the
  start of the machine's memory region so that a valid pointer can be
  derefenced in all cases
 */

static uint8_t *machine_step_u8(struct machine *m)
{
	if (m->pc + 1U <= m->rom_size) {
		return m->rom + m->pc++;
	} else {
		m->state = MACHINE_OUT_OF_BOUNDS;
		return MACHINE_INVALID_ADDRESS(m);
	}
}

static uint8_t *machine_step_u16(struct machine *m) {
	if (m->pc + 2U <= m->rom_size) {
		uint8_t *mem = m->rom + m->pc;
		m->pc += 2;
		return mem;
	} else {
		m->state = MACHINE_OUT_OF_BOUNDS;
		return MACHINE_INVALID_ADDRESS(m);
	}
}

static uint8_t *machine_get_address(struct machine *m, enum addr_mode mode)
{
	/*
	  all bytes of 6502 machine code are in little endian format 
	 */
	switch (mode) {
	case ADDR_MODE_IMM: {
		return machine_step_u8(m);
	}

	case ADDR_MODE_ZERO: {
		uint8_t off = *machine_step_u8(m);
		return &m->zero_page[off];
	}

	case ADDR_MODE_ZERO_X: {
		uint8_t off = *machine_step_u8(m) + m->reg[REG_X];
		return &m->zero_page[off];
	}

	case ADDR_MODE_ZERO_Y: {
		uint8_t off = *machine_step_u8(m) + m->reg[REG_Y];
		return &m->zero_page[off];
	}

	case ADDR_MODE_ABS: {
		uint16_t addr = htole16(*machine_step_u16(m));
		return &m->memory[addr];
	}

	case ADDR_MODE_ABS_X: {
		uint16_t addr = htole16(*machine_step_u16(m)) + m->reg[REG_X];
		return &m->memory[addr];
	}

	case ADDR_MODE_ABS_Y: {
		uint16_t addr = htole16(*machine_step_u16(m)) + m->reg[REG_Y];
		return &m->memory[addr];
	}

	case ADDR_MODE_IND_X: {
		uint8_t addr = *machine_step_u8(m) + m->reg[REG_X];
		uint8_t addr_zp = m->zero_page[addr];
		return &m->zero_page[addr_zp];
	}

	case ADDR_MODE_IND_Y: {
		uint8_t addr_zp = *machine_step_u8(m);
		uint8_t addr_lo = m->zero_page[addr_zp];
		uint8_t addr = (m->reg[REG_Y] << 8) | addr_lo;
		return &m->memory[addr];
	}

	default: __builtin_unreachable();
	}
}

static inline void machine_update_flags(struct machine *m,
					uint8_t flags, uint8_t v)
{
	/*
	  remember to add the rest when needed!!!
	 */
	if ((flags & PS_ZERO) && (v == 0))
		m->reg[REG_PS] |= PS_ZERO;
	if ((flags & PS_NEGATIVE) && ((int8_t)v < 0))
		m->reg[REG_PS] |= PS_NEGATIVE;
}

static inline void load_accumulator(struct machine *m, enum addr_mode mode)
{
	m->reg[REG_ACC] = *machine_get_address(m, mode);
	machine_update_flags(m, PS_ZERO | PS_NEGATIVE, m->reg[REG_ACC]);
}

static inline void load_register_x(struct machine *m, enum addr_mode mode)
{
	m->reg[REG_X] = *machine_get_address(m, mode);
	machine_update_flags(m, PS_ZERO | PS_NEGATIVE, m->reg[REG_X]);
}

static inline void load_register_y(struct machine *m, enum addr_mode mode)
{
	m->reg[REG_Y] = *machine_get_address(m, mode);
	machine_update_flags(m, PS_ZERO | PS_NEGATIVE, m->reg[REG_Y]);
}

static void machine_execute_instruction(struct machine *m)
{
	uint8_t op = *machine_step_u8(m);
	
	switch (op) {
	case 0xA9: load_accumulator(m, ADDR_MODE_IMM); return;
	case 0xA5: load_accumulator(m, ADDR_MODE_ZERO); return;
	case 0xB5: load_accumulator(m, ADDR_MODE_ZERO_X); return;
	case 0xAD: load_accumulator(m, ADDR_MODE_ABS); return;
	case 0xBD: load_accumulator(m, ADDR_MODE_ABS_X); return;
	case 0xB9: load_accumulator(m, ADDR_MODE_ABS_Y); return;
	case 0xA1: load_accumulator(m, ADDR_MODE_IND_X); return;
	case 0xB1: load_accumulator(m, ADDR_MODE_IND_Y); return;

	case 0xA2: load_register_x(m, ADDR_MODE_IMM); return;
	case 0xA6: load_register_x(m, ADDR_MODE_ZERO); return;
	case 0xB6: load_register_x(m, ADDR_MODE_ZERO_Y); return;
	case 0xAE: load_register_x(m, ADDR_MODE_ABS); return;
	case 0xBE: load_register_x(m, ADDR_MODE_ABS_Y); return;

	case 0xA0: load_register_y(m, ADDR_MODE_IMM); return;
	case 0xA4: load_register_y(m, ADDR_MODE_ZERO); return;
	case 0xB4: load_register_y(m, ADDR_MODE_ZERO_X); return;
	case 0xAC: load_register_y(m, ADDR_MODE_ABS); return;
	case 0xBC: load_register_y(m, ADDR_MODE_ABS_X); return;
		
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
