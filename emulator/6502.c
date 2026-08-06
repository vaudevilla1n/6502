/*
  6502 emulator

  referencing: 
	  https://en.wikipedia.org/wiki/MOS_Technology_6502
	  https://6502.org
	  https://llx.com/Neil/a2/opcodes.html (!!!)
 */
#include "util.h"
#include "6502_constants.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

enum machine_state {
	MACHINE_OK,
	MACHINE_INVALID_OPCODE,
	MACHINE_OUT_OF_BOUNDS,
	MACHINE_STACK_OVERFLOW,
	MACHINE_STACK_UNDERFLOW,
};

struct machine {
	enum machine_state state;

	enum addressing_mode addr_mode;
	
	size_t rom_size;
	const uint8_t *rom;
	
	uint16_t pc;
	uint8_t reg[REGISTER_COUNT];
	
	uint8_t zero_page[ZERO_PAGE_LEN];
	uint8_t stack[STACK_PAGE_LEN];
	uint8_t memory[MEMORY_AVAILABLE];
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
	default: u_unreachable("log_machine_info");
	}
	printf("\n");
	
	printf("program counter: 0x%02hx\n", m->pc);
	printf("accumulator: %02hhx\n", m->reg[REG_A]);
	
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

	printf("stack pointer: 0x%hhx\n", m->reg[REG_S]);
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

	if (size > MEMORY_AVAILABLE) {
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
	m->reg[REG_S] = STACK_PAGE_START;
}

/*
  since the error is already propagated through the machine structure
  itself, these functions (machine_step_*) simply return 0
 */

static inline uint8_t get_flag_bit(struct machine *m, uint8_t flag)
{
	return (m->reg[REG_PS] >> flag) & 1;
}

static inline void check_flag(struct machine *m, uint8_t flag, uint8_t bit)
{
	if (bit)
		m->reg[REG_PS] |= (1U << flag);
	else
		m->reg[REG_PS] &= ~(1U << flag);
}

static inline void check_zero_flag(struct machine *m, uint8_t v)
{
	check_flag(m, PS_ZERO, (v == 0));
}

static inline void check_negative_flag(struct machine *m, uint8_t v)
{
	check_flag(m, PS_NEGATIVE, (v >> 7));
}

static inline void check_overflow_flag(struct machine *m, uint16_t v)
{
	check_flag(m, PS_OVERFLOW, (v > 0xFF));
}

static inline void load_register(struct machine *m,
				 enum machine_register reg,
				 uint8_t b)
{
	m->reg[reg] = b;
	check_zero_flag(m, m->reg[reg]);
	check_negative_flag(m, m->reg[reg]);
}

static inline void store_register(struct machine *m,
				  enum machine_register reg,
				  uint8_t *mem)
{
	*mem = m->reg[reg];
}

static inline void transfer_registers(struct machine *m,
				      enum machine_register src,
				      enum machine_register dst)
{
	m->reg[dst] = m->reg[src];
	check_zero_flag(m, m->reg[dst]);
	check_negative_flag(m, m->reg[dst]);
}

static inline void transfer_x_to_stack_pointer(struct machine *m)
{
	m->reg[REG_S] = m->reg[REG_X];
}

static inline void stack_push_u8(struct machine *m, uint8_t dat)
{
	if (m->reg[REG_S] > 0)
		m->stack[m->reg[REG_S]--] = dat;
	else
		m->state = MACHINE_STACK_OVERFLOW;
}

static inline void stack_push_u16(struct machine *m, uint16_t dat)
{
	stack_push_u8(m, dat & 0xFF);
	stack_push_u8(m, (dat >> 8) & 0xFF);
}

static inline uint8_t stack_pop_u8(struct machine *m)
{
	if (m->reg[REG_S] + 1 < STACK_PAGE_LEN) {
		return m->stack[++m->reg[REG_S]];
	} else {
		m->state = MACHINE_STACK_OVERFLOW;
		return 0x00;
	}
}

static inline uint16_t stack_pop_u16(struct machine *m)
{
	uint8_t lo = stack_pop_u8(m);
	uint8_t hi = stack_pop_u8(m);
	return (hi << 8) | lo;
}

static inline void stack_push_register(struct machine *m,
				       enum machine_register reg)
{
	stack_push_u8(m, m->reg[reg]);
}

static inline void stack_pull_register(struct machine *m,
				       enum machine_register reg)
{
	m->reg[reg] = stack_pop_u8(m);
}

static inline void logical_and(struct machine *m, uint8_t b)
{
	m->reg[REG_A] &= b;
	check_zero_flag(m, m->reg[REG_A]);
	check_negative_flag(m, m->reg[REG_A]);
}

static inline void logical_xor(struct machine *m, uint8_t b)
{
	m->reg[REG_A] ^= b;
	check_zero_flag(m, m->reg[REG_A]);
	check_negative_flag(m, m->reg[REG_A]);
}

static inline void logical_or(struct machine *m, uint8_t b)
{
	m->reg[REG_A] |= b;
	check_zero_flag(m, m->reg[REG_A]);
	check_negative_flag(m, m->reg[REG_A]);
}

static inline void bit_test(struct machine *m, uint8_t b)
{
	uint8_t v = m->reg[REG_A] & b;
	check_zero_flag(m, v);
	check_negative_flag(m, v);
	check_flag(m, PS_OVERFLOW, ((v >> 6) & 1));
}

static void add_with_carry(struct machine *m, uint8_t b)
{
	uint8_t carry = get_flag_bit(m, PS_CARRY);
	uint16_t res = m->reg[REG_A] + b + carry;
	
	check_overflow_flag(m, res);
	check_zero_flag(m, res);
	check_negative_flag(m, res);

	check_flag(m, PS_CARRY, (m->reg[REG_PS] & PS_OVERFLOW));
	m->reg[REG_A] = res;
}

static void sub_with_carry(struct machine *m, uint8_t b)
{
	uint8_t carry = ((m->reg[REG_PS] & PS_CARRY) > 0);
	uint16_t res = m->reg[REG_A] - b - !carry;

	check_overflow_flag(m, res);
	check_zero_flag(m, res);
	check_negative_flag(m, res);
	check_flag(m, PS_CARRY, !(m->reg[REG_PS] & PS_OVERFLOW));
	
	m->reg[REG_A] = res;
}

static inline void compare(struct machine *m, enum machine_register reg,
			   uint8_t b)
{
	uint8_t res = m->reg[reg] - b;
	check_zero_flag(m, res);
	check_negative_flag(m, res);
	check_flag(m, PS_CARRY, (res > 0));
}

static inline void increment_memory(struct machine *m, uint8_t *mem)
{
	mem[0]++;
	check_zero_flag(m, mem[0]);
	check_negative_flag(m, mem[0]);
}

static inline void increment_register(struct machine *m,
				      enum machine_register reg)
{
	m->reg[reg]++;
	check_zero_flag(m, m->reg[reg]);
	check_negative_flag(m, m->reg[reg]);
}

static inline void decrement_memory(struct machine *m, uint8_t *mem)
{
	mem[0]--;
	check_zero_flag(m, mem[0]);
	check_negative_flag(m, mem[0]);
}

static inline void decrement_register(struct machine *m,
				      enum machine_register reg)
{
	m->reg[reg]--;
	check_zero_flag(m, m->reg[reg]);
	check_negative_flag(m, m->reg[reg]);
}

static void arithmetic_shift_left(struct machine *m, uint8_t *mem)
{
	uint8_t msb = mem[0] >> 7;
	mem[0] >>= 1;
	check_zero_flag(m, mem[0]);
	check_negative_flag(m, mem[0]);
	check_flag(m, PS_CARRY, msb);
}

static void logical_shift_right(struct machine *m, uint8_t *mem)
{
	uint8_t lsb = mem[0] & 1;
	mem[0] >>= 1;
	check_zero_flag(m, mem[0]);
	check_negative_flag(m, mem[0]);
	check_flag(m, PS_CARRY, lsb);
}

static void rotate_left(struct machine *m, uint8_t *mem)
{
	uint8_t msb = mem[0] >> 7;
	mem[0] = (mem[0] << 1) | get_flag_bit(m, PS_CARRY);
	check_zero_flag(m, mem[0]);
	check_negative_flag(m, mem[0]);
	check_flag(m, PS_CARRY, msb);
}

static void rotate_right(struct machine *m, uint8_t *mem)
{
	uint8_t lsb = mem[0] & 1;
	mem[0] = (mem[0] >> 1) | (get_flag_bit(m, PS_CARRY) << 7);
	check_zero_flag(m, mem[0]);
	check_negative_flag(m, mem[0]);
	check_flag(m, PS_CARRY, lsb);
}

static inline void jump(struct machine *m, uint8_t *mem)
{
	m->pc = mem - m->memory;
}

static void subroutine_jump(struct machine *m, const uint8_t *mem)
{
	stack_push_u16(m, m->pc);
	m->pc = mem - m->memory;
}

static void subroutine_return(struct machine *m)
{
	m->pc = stack_pop_u16(m);
}

static inline void branch_if_clear(struct machine *m, uint8_t flag, int8_t off)
{
	if (!get_flag_bit(m, flag))
		m->pc = (int16_t)m->pc - off;
}

static inline void branch_if_set(struct machine *m, uint8_t flag, int8_t off)
{
	if (get_flag_bit(m, flag))
		m->pc = (int16_t)m->pc - off;
}

static inline void clear_flag(struct machine *m, uint8_t flag)
{
	m->reg[REG_PS] &= ~(1U << flag);
}

static inline void set_flag(struct machine *m, uint8_t flag)
{
	m->reg[REG_PS] |= (1U << flag);
}

static void force_interrupt(struct machine *m)
{
	stack_push_u16(m, m->pc);
	stack_push_u8(m, m->reg[REG_PS]);
	set_flag(m, PS_BREAK);
	// interrupt is handled externally, so just clear pc for now
	m->pc = 0;
}

static void no_op(void)
{
	// we get high we get fat
	return;
}

static inline void return_from_interrupt(struct machine *m)
{
	m->reg[REG_PS] = stack_pop_u8(m);
	m->pc = stack_pop_u16(m);
}

static uint8_t read_u8_from_rom(struct machine *m)
{
	if (m->pc + 1U <= m->rom_size) {
		return m->rom[m->pc++];
	} else {
		m->state = MACHINE_OUT_OF_BOUNDS;
		return 0;
	}
}

static uint16_t read_u16_from_rom(struct machine *m) {
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

static inline uint16_t read_u16_from_memory(const struct machine *m,
					    uint16_t addr)
{
	return (m->memory[addr + 1] << 8) | m->memory[addr];
}

static inline uint16_t read_u16_from_zero_page(const struct machine *m,
					       uint8_t off)
{
	uint8_t lo = m->zero_page[(uint8_t)(off + 1)];
	uint8_t hi = m->zero_page[off];
	return (hi << 8) | lo;
}

static inline bool machine_ok(const struct machine *m)
{
	return m->state == MACHINE_OK;
}

static inline uint8_t *fetch_zero_page_address(struct machine *m, uint8_t off)
{
	uint8_t addr = read_u8_from_rom(m);
	return (machine_ok(m)) ? &m->zero_page[addr + off] : 0;
}

static inline bool valid_address(uint16_t addr, uint16_t off)
{
	return (addr + off < MEMORY_AVAILABLE);
}

static inline uint8_t *fetch_absolute_address(struct machine *m, uint8_t off)
{
	uint16_t addr = read_u16_from_rom(m);
	return (machine_ok(m) && valid_address(addr, off))
		? &m->memory[addr + off] : 0;
}

static inline uint8_t *fetch_indirect_address(struct machine *m)
{
	uint16_t ind_addr = read_u16_from_rom(m);
	if (!machine_ok(m) || ind_addr + 2 > MEMORY_AVAILABLE)
		return 0;
	uint16_t addr = read_u16_from_memory(m, ind_addr);
	return &m->memory[addr];
}

static inline uint8_t *fetch_indexed_indirect_address(struct machine *m)
{
	uint16_t zp_addr = read_u8_from_rom(m);
	if (!machine_ok(m))
		return 0;
	uint16_t addr = read_u16_from_zero_page(m, zp_addr + m->reg[REG_X]);
	return &m->memory[addr];
}

static inline uint8_t *fetch_indirect_indexed_address(struct machine *m)
{
	uint16_t zp_addr = read_u8_from_rom(m);
	if (!machine_ok(m))
		return 0;
	uint16_t off = m->reg[REG_Y];
	uint16_t addr = read_u16_from_zero_page(m, zp_addr);
	return (valid_address(addr, off)) ? &m->memory[addr + off] : 0;
}

static uint8_t *fetch_memory_address(struct machine *m,
				     enum addressing_mode mode)
{
	switch (mode) {
	case ADDR_MODE_IMM:	return (uint8_t *)&m->rom[m->pc++];
	case ADDR_MODE_ACC:	return &m->reg[REG_A];
	case ADDR_MODE_REL:	return (uint8_t *)&m->rom[m->pc++];
		
	case ADDR_MODE_ZERO:	return fetch_zero_page_address(m, 0);
	case ADDR_MODE_ZERO_X:	return fetch_zero_page_address(m, m->reg[REG_X]);
	case ADDR_MODE_ZERO_Y:	return fetch_zero_page_address(m, m->reg[REG_Y]);
		
	case ADDR_MODE_ABS:	return fetch_absolute_address(m, 0);
	case ADDR_MODE_ABS_X:	return fetch_absolute_address(m, m->reg[REG_X]);
	case ADDR_MODE_ABS_Y:	return fetch_absolute_address(m, m->reg[REG_Y]);

	case ADDR_MODE_IND:	return fetch_indirect_address(m);
	case ADDR_MODE_IND_X:	return fetch_indexed_indirect_address(m);
	case ADDR_MODE_IND_Y:	return fetch_indirect_indexed_address(m);

	default: u_unreachable("fetch_memory_address");
	}
}

static int interpret_opcode(uint8_t op, enum instruction *out_ins,
	enum addressing_mode *out_addr_mode)
{
	enum instruction ins = special_instruction_from_opcode[op];
	if (ins != INS_INVALID)
		return 1;
	
	uint8_t grp = opcode_group(op);
	uint8_t mode = opcode_addressing_mode(op);
	uint8_t op_ins = opcode_instruction(op);
	
	enum addressing_mode addr_mode = addressing_mode_from_opcode[grp][mode];
	ins = instruction_from_opcode[grp][op_ins];
	if (ins == INS_INVALID || addr_mode == ADDR_MODE_INVALID)
		return 1;

	switch (ins) {
	case INS_LDX: {
		switch (addr_mode) {
		case ADDR_MODE_ZERO_X:	addr_mode = ADDR_MODE_ZERO_Y; break;
		case ADDR_MODE_ABS_X:	addr_mode = ADDR_MODE_ABS_Y; break;
		default: break;
		}
	} break;

	case INS_STA: {
		if (addr_mode == ADDR_MODE_IMM)
			addr_mode = ADDR_MODE_INVALID;
	} break;

	case INS_STX: {
		if (addr_mode == ADDR_MODE_ZERO_X)
			addr_mode = ADDR_MODE_ZERO_Y;
	} break;

	default: break;
	}

	*out_ins = ins;
	*out_addr_mode = addr_mode;
	return 0;
}

static void machine_execute_instruction(struct machine *m)
{
	uint8_t op = read_u8_from_rom(m);

	enum instruction ins = 0;
	enum addressing_mode addr_mode = 0;
	if (interpret_opcode(op, &ins, &addr_mode))
		goto invalid_opcode;

	uint8_t *mem = 0;
	if (addr_mode != ADDR_MODE_NONE) {
		mem = fetch_memory_address(m, addr_mode);
		if (!mem)
			return;
	}
	
	switch (ins) {
	case INS_LDA: load_register(m, REG_A, mem[0]); return;
	case INS_LDX: load_register(m, REG_X, mem[0]); return;
	case INS_LDY: load_register(m, REG_Y, mem[0]); return;
	case INS_STA: store_register(m, REG_A, mem); return;
	case INS_STX: store_register(m, REG_X, mem); return;
	case INS_STY: store_register(m, REG_Y, mem); return;

	case INS_TAX: transfer_registers(m, REG_A, REG_X); return;
	case INS_TAY: transfer_registers(m, REG_A, REG_Y); return;
	case INS_TXA: transfer_registers(m, REG_X, REG_A); return;
	case INS_TYA: transfer_registers(m, REG_Y, REG_A); return;

	case INS_TSX: transfer_registers(m, REG_S, REG_X); return;
	case INS_TXS: transfer_x_to_stack_pointer(m); return;
	case INS_PHA: stack_push_register(m, REG_A); return;
	case INS_PHP: stack_push_register(m, REG_PS); return;
	case INS_PLA: stack_pull_register(m, REG_A); return;
	case INS_PLP: stack_pull_register(m, REG_PS); return;
		
	case INS_AND: logical_and(m, mem[0]); return;
	case INS_EOR: logical_xor(m, mem[0]); return;
	case INS_ORA: logical_or(m, mem[0]); return;
	case INS_BIT: bit_test(m, mem[0]); return;

	case INS_ADC: add_with_carry(m, mem[0]); return;
	case INS_SBC: sub_with_carry(m, mem[0]); return;
	case INS_CMP: compare(m, REG_A, mem[0]); return;
	case INS_CPX: compare(m, REG_X, mem[0]); return;
	case INS_CPY: compare(m, REG_Y, mem[0]); return;

	case INS_INC: increment_memory(m, mem); return;
	case INS_INX: increment_register(m, REG_X); return;
	case INS_INY: increment_register(m, REG_Y); return;
 	case INS_DEC: decrement_memory(m, mem); return;
	case INS_DEX: decrement_register(m, REG_X); return;
	case INS_DEY: decrement_register(m, REG_Y); return;

	case INS_ASL: arithmetic_shift_left(m, mem); return;
	case INS_LSR: logical_shift_right(m, mem); return;

	case INS_ROL: rotate_left(m, mem); return;
	case INS_ROR: rotate_right(m, mem); return;

	case INS_JMP:
	case INS_JMA: jump(m, mem); return;
	case INS_JSR: subroutine_jump(m, mem); return;
	case INS_RTS: subroutine_return(m); return;

	case INS_BCC: branch_if_clear(m, PS_CARRY, (int8_t)mem[0]); return;
	case INS_BCS: branch_if_set(m, PS_CARRY, (int8_t)mem[0]); return;
	case INS_BEQ: branch_if_set(m, PS_ZERO, (int8_t)mem[0]); return;
	case INS_BNE: branch_if_clear(m, PS_ZERO, (int8_t)mem[0]); return;
	case INS_BMI: branch_if_set(m, PS_NEGATIVE, (int8_t)mem[0]); return;
	case INS_BPL: branch_if_clear(m, PS_NEGATIVE, (int8_t)mem[0]); return;
	case INS_BVC: branch_if_clear(m, PS_OVERFLOW, (int8_t)mem[0]); return;
	case INS_BVS: branch_if_set(m, PS_OVERFLOW, (int8_t)mem[0]); return;

	case INS_CLC: clear_flag(m, PS_CARRY); return;
	case INS_CLD: clear_flag(m, PS_DECIMAL_MODE); return;
	case INS_CLI: clear_flag(m, PS_INTERRUPT_DISABLE); return;
	case INS_CLV: clear_flag(m, PS_OVERFLOW); return;
	case INS_SEC: set_flag(m, PS_CARRY); return;
	case INS_SED: set_flag(m, PS_DECIMAL_MODE); return;
	case INS_SEI: set_flag(m, PS_INTERRUPT_DISABLE); return;

	// don't know what to do with these interrupts yet
	case INS_BRK: force_interrupt(m); return;
	case INS_NOP: no_op(); return;
	case INS_RTI: return_from_interrupt(m); return;

	default: goto invalid_opcode;
	}

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
