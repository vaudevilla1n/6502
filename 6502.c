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

#define NEGATIVE_FLAG_MASK	0x80

enum {
	PR_CARRY		= 0001,
	PR_ZERO			= 0002,
	PR_INTERRUPT_DISABLE	= 0004,
	PR_DECIMAL_MODE		= 0010,
	PR_BREAK		= 0020,
	PR_OVERFLOW		= 0040,
	PR_NEGATIVE		= 0100,
};

enum cpu_register {
	REG_AC,
	REG_PR,
	REG_SR,
	REG_XR,
	REG_YR,
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
	
	uint8_t ram[MAX_AVAILABLE_MEMORY];
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
	printf("accumulator: %02hhx\n", m->reg[REG_AC]);
	
	printf("processor status:\n");
	printf("  carry flag: %d\n",
	       (m->reg[REG_PR] & PR_CARRY) != 0);
	printf("  zero flag: %d\n",
	       (m->reg[REG_PR] & PR_ZERO) != 0);
	printf("  decimal mode: %d\n",
	       (m->reg[REG_PR] & PR_DECIMAL_MODE) != 0);
	printf("  interrupt disable: %d\n",
	       (m->reg[REG_PR] & PR_INTERRUPT_DISABLE) != 0);
	printf("  break: %d\n",
	       (m->reg[REG_PR] & PR_BREAK) != 0);
	printf("  overflow flag: %d\n",
	       (m->reg[REG_PR] & PR_OVERFLOW) != 0);
	printf("  negative flag: %d\n",
	       (m->reg[REG_PR] & PR_NEGATIVE) != 0);

	printf("stack pointer: 0x%hhx\n", m->reg[REG_SR]);
	printf("X register: %02hhx\n", m->reg[REG_XR]);
	printf("Y register: %02hhx\n", m->reg[REG_YR]);
}

static void machine_init(struct machine *m, const uint8_t *rom, size_t size)
{
	m->state = MACHINE_OK;
	m->rom = rom;
	m->rom_size = size;
	m->reg[REG_SR] = STACK_PAGE_START;
}

static inline uint8_t machine_step_pc(struct machine *m)
{
	if (m->pc >= m->rom_size) {
		m->state = MACHINE_OUT_OF_BOUNDS;
		return 0;
	} else {
		return m->rom[m->pc++];
	}
}

static inline void check_zero_flag(struct machine *m, uint8_t v)
{
	if (v == 0)
		m->reg[REG_PR] |= PR_ZERO;
}

static inline void check_negative_flag(struct machine *m, uint8_t v)
{
	if (v & NEGATIVE_FLAG_MASK)
		m->reg[REG_PR] |= PR_NEGATIVE;
}

static inline void load_accumulator_immediate(struct machine *m)
{
	uint8_t imm = machine_step_pc(m);
	if (m->state != MACHINE_OK)
		return;
	
	m->reg[REG_AC] = imm;

	check_zero_flag(m, m->reg[REG_AC]);
	check_negative_flag(m, m->reg[REG_AC]);
}

static void machine_execute_instruction(struct machine *m)
{
	uint8_t op = machine_step_pc(m);
	
	switch (op) {
	case 0xA9: load_accumulator_immediate(m); return;
		
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
