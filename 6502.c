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
	ADDR_MODE_ABS,
	ADDR_MODE_ABS_X,
	ADDR_MODE_ABS_Y,
	ADDR_MODE_IND_X,
	ADDR_MODE_IND_Y,
};

enum {
	PR_CARRY		= 0001,
	PR_ZERO			= 0002,
	PR_INTERRUPT_DISABLE	= 0004,
	PR_DECIMAL_MODE		= 0010,
	PR_BREAK		= 0020,
	PR_OVERFLOW		= 0040,
	PR_NEGATIVE		= 0100,
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
	const uint8_t *rom;
	
	uint16_t pc;
	uint8_t reg[TOTAL_CPU_REGS];
	
	uint8_t zero_page[ZERO_PAGE_LEN];
	uint8_t stack[STACK_PAGE_LEN];
	uint8_t memory[MAX_AVAILABLE_MEMORY];
};

static void machine_init(struct machine *m, const uint8_t *rom, size_t size);
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
	       (m->reg[REG_PS] & PR_CARRY) != 0);
	printf("  zero flag: %d\n",
	       (m->reg[REG_PS] & PR_ZERO) != 0);
	printf("  decimal mode: %d\n",
	       (m->reg[REG_PS] & PR_DECIMAL_MODE) != 0);
	printf("  interrupt disable: %d\n",
	       (m->reg[REG_PS] & PR_INTERRUPT_DISABLE) != 0);
	printf("  break: %d\n",
	       (m->reg[REG_PS] & PR_BREAK) != 0);
	printf("  overflow flag: %d\n",
	       (m->reg[REG_PS] & PR_OVERFLOW) != 0);
	printf("  negative flag: %d\n",
	       (m->reg[REG_PS] & PR_NEGATIVE) != 0);

	printf("stack pointer: 0x%hhx\n", m->reg[REG_SP]);
	printf("X register: %02hhx\n", m->reg[REG_X]);
	printf("Y register: %02hhx\n", m->reg[REG_Y]);
}

static void machine_init(struct machine *m, const uint8_t *rom, size_t size)
{
	m->state = MACHINE_OK;
	m->rom = rom;
	m->rom_size = size;
	m->reg[REG_SP] = STACK_PAGE_START;
}

static inline uint8_t machine_step_byte(struct machine *m)
{
	if (m->pc + 1U > m->rom_size) {
		m->state = MACHINE_OUT_OF_BOUNDS;
		return 0;
	} else {
		return m->rom[m->pc++];
	}
}

static inline uint8_t machine_step_short(struct machine *m) {
	if (m->pc + 2U > m->rom_size) {
		m->state = MACHINE_OUT_OF_BOUNDS;
		return 0;
	} else {
		uint8_t lo = m->rom[m->pc++];
		uint8_t hi = m->rom[m->pc++];
		return (hi << 8) | lo;
	}
}

static void load_accumulator(struct machine *m, enum addr_mode mode)
{
	switch (mode) {
	case ADDR_MODE_IMM: {
		m->reg[REG_ACC] = machine_step_byte(m);
	} break;

	case ADDR_MODE_ZERO: {
		uint8_t addr = machine_step_byte(m);
		m->reg[REG_ACC] = m->zero_page[addr];
	} break;

	case ADDR_MODE_ZERO_X: {
		uint8_t addr = machine_step_byte(m) + m->reg[REG_X];
		m->reg[REG_ACC] = m->zero_page[addr];
	} break;

	case ADDR_MODE_ABS: {
		uint16_t addr = machine_step_short(m);
		m->reg[REG_ACC] = m->memory[addr];
	} break;

	case ADDR_MODE_ABS_X: {
		uint16_t addr = machine_step_short(m) + m->reg[REG_X];
		m->reg[REG_ACC] = m->memory[addr];
	} break;

	case ADDR_MODE_ABS_Y: {
		uint16_t addr = machine_step_short(m) + m->reg[REG_Y];
		m->reg[REG_ACC] = m->memory[addr];
	} break;

	case ADDR_MODE_IND_X: {
		uint8_t addr = machine_step_byte(m) + m->reg[REG_X];
		uint8_t addr_zp = m->zero_page[addr];
		
		m->reg[REG_ACC] = m->zero_page[addr_zp];
	} break;

	case ADDR_MODE_IND_Y: {
		uint8_t addr_zp = machine_step_byte(m);
		uint8_t addr_lo = m->zero_page[addr_zp];
		uint8_t addr = (m->reg[REG_Y] << 8) | addr_lo;
		m->reg[REG_ACC] = m->memory[addr];
	} break;

	default: __builtin_unreachable();
	}
}

static void machine_execute_instruction(struct machine *m)
{
	uint8_t op = machine_step_byte(m);
	
	switch (op) {
	case 0xA9: load_accumulator(m, ADDR_MODE_IMM); return;
	case 0xA5: load_accumulator(m, ADDR_MODE_ZERO); return;
	case 0xB5: load_accumulator(m, ADDR_MODE_ZERO_X); return;
	case 0xAD: load_accumulator(m, ADDR_MODE_ABS); return;
	case 0xBD: load_accumulator(m, ADDR_MODE_ABS_X); return;
	case 0xB9: load_accumulator(m, ADDR_MODE_ABS_Y); return;
	case 0xA1: load_accumulator(m, ADDR_MODE_IND_X); return;
	case 0xB1: load_accumulator(m, ADDR_MODE_IND_Y); return;
		
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
