/***************************************************************/
/*   MIPS-32 Instruction Level Simulator                       */
/***************************************************************/

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "myshell.h"

/***************************************************************/
/* Main memory.                                                */
/***************************************************************/

#define MEM_DATA_START 0x10000000
#define MEM_DATA_SIZE 0x00100000
// #define MEM_TEXT_START 0x00400000 原始定义
#define MEM_TEXT_START 0x00400028
#define MEM_TEXT_SIZE 0x00100000
#define MEM_STACK_START 0x7ff00000
#define MEM_STACK_SIZE 0x00100000
#define MEM_KDATA_START 0x90000000
#define MEM_KDATA_SIZE 0x00100000
#define MEM_KTEXT_START 0x80000000
#define MEM_KTEXT_SIZE 0x00100000

typedef struct
{
	uint32_t start, size;
	uint8_t *mem;
} mem_region_t;

/* memory will be dynamically allocated at initialization */
mem_region_t MEM_REGIONS[] = {
	{MEM_TEXT_START, MEM_TEXT_SIZE, NULL},
	{MEM_DATA_START, MEM_DATA_SIZE, NULL},
	{MEM_STACK_START, MEM_STACK_SIZE, NULL},
	{MEM_KDATA_START, MEM_KDATA_SIZE, NULL},
	{MEM_KTEXT_START, MEM_KTEXT_SIZE, NULL}};

#define MEM_NREGIONS (sizeof(MEM_REGIONS) / sizeof(mem_region_t))

/* CPU State info */
CPU_State CURRENT_STATE, NEXT_STATE;
int RUN_BIT = TRUE; /* run bit */
int INSTRUCTION_COUNT = 0;

/* debug parameters */
int show_assemble = FALSE, show_detail = FALSE;
int dump_stdout = TRUE, dump_file = TRUE;

/*
Procedure: mem_read_32
Purpose : Read a 32-bit word from memory
*/
uint32_t mem_read_32(uint32_t address)
{
	for (int i = 0; i < MEM_NREGIONS; i++)
	{
		if (address >= MEM_REGIONS[i].start &&
			address < (MEM_REGIONS[i].start + MEM_REGIONS[i].size))
		{
			uint32_t offset = address - MEM_REGIONS[i].start;
			return (MEM_REGIONS[i].mem[offset + 3] << 24) |
				   (MEM_REGIONS[i].mem[offset + 2] << 16) |
				   (MEM_REGIONS[i].mem[offset + 1] << 8) |
				   (MEM_REGIONS[i].mem[offset + 0] << 0);
		}
	}
	return 0;
}
/*
Procedure: mem_write_32
Purpose: Write a 32-bit word to memory
*/
void mem_write_32(uint32_t address, uint32_t value)
{
	for (int i = 0; i < MEM_NREGIONS; i++)
	{
		if (address >= MEM_REGIONS[i].start &&
			address < (MEM_REGIONS[i].start + MEM_REGIONS[i].size))
		{
			uint32_t offset = address - MEM_REGIONS[i].start;
			MEM_REGIONS[i].mem[offset + 3] = (value >> 24) & 0xFF;
			MEM_REGIONS[i].mem[offset + 2] = (value >> 16) & 0xFF;
			MEM_REGIONS[i].mem[offset + 1] = (value >> 8) & 0xFF;
			MEM_REGIONS[i].mem[offset + 0] = (value >> 0) & 0xFF;
			return;
		}
	}
}

/*
Procedure : help
Purpose   : Print out a list of commands
*/
void help()
{
	printf("-------------------MIPS SIM Help----------------------\n");

	printf("g[o] [a [v]]\n");
	printf("\trun until completion or halt\n");
	printf("\ta: show assemble\n");
	printf("\tv: show detailed info\n");

	printf("o[perate] {num} [a [v]]\n");
	printf("\trun at most {num}(dec) instructions without halt\n");
	printf("\ta: show assemble\n");
	printf("\tv: show detailed info\n");

	printf("d[ump] m {low} {high} [s] [f]\n");
	printf("\tdump content in memory from {low}(hex) to {high}(hex)\n");
	printf("\ts: not dump to stdout\n");
	printf("\tf: not dump to file\n");

	printf("d[ump] r [s] [f]\n");
	printf("\tdump content of registers\n");
	printf("\ts: not dump to stdout\n");
	printf("\tf: not dump to file\n");

	printf("s[et] {reg} {val}\n");
	printf("\tset the value of register {reg} to {val}(hex)\n");
	printf("\t{reg} can be pc/hi/lo/0/.../1f(hex)\n");

	printf("r[ecover]\n");
	printf("\tset RUN_BIT to TRUE\n");

	printf("h[elp]\n");
	printf("\tshow usage of commands\n");

	printf("q[uit]\n");
	printf("\texit simulator\n");

	printf("------------------------------------------------------\n");
}

/*
Procedure : cycle
Purpose   : Execute a cycle
*/
void cycle()
{
	process_instruction();
	CURRENT_STATE = NEXT_STATE;
	INSTRUCTION_COUNT++;
}

/*
Procedure : run n
Purpose   : Simulate MIPS for n cycles
*/
void run(int num_cycles)
{
	if (RUN_BIT == TRUE)
		printf("@ Simulating for %d cycles...\n\n", num_cycles);
	for (int i = 0; i < num_cycles; i++)
	{
		if (RUN_BIT == FALSE)
		{
			printf("@ Simulator is halted\n\n");
			break;
		}
		cycle();
	}
}

/*
Procedure : go
Purpose   : Simulate MIPS until HALTed
*/
void go()
{
	if (RUN_BIT == TRUE)
		printf("@ Simulating...\n\n");
	while (RUN_BIT)
		cycle();
	printf("@ Simulator is halted\n\n");
}

/*
Procedure : mdump
Purpose   : Dump a word-aligned region of memory to the output file.
*/
void mdump(FILE *dumpsim_file, int start, int stop)
{
	int address;
	if (dump_stdout)
	{
		printf("@ Memory content [0x%08x..0x%08x] :\n", start, stop);
		printf("-------------------------------------\n");
		printf("address : content\n");
		for (address = start; address <= stop; address += 4)
			printf("%08x: %08x\n", address, mem_read_32(address));
		printf("-------------------------------------\n");
	}
	if (dump_file)
	{
		fprintf(dumpsim_file, "@ Memory content [0x%08x..0x%08x] :\n", start, stop);
		fprintf(dumpsim_file, "-------------------------------------\n");
		fprintf(dumpsim_file, "address : content\n");
		for (address = start; address <= stop; address += 4)
			fprintf(dumpsim_file, "%08x: %08x\n", address, mem_read_32(address));
		fprintf(dumpsim_file, "-------------------------------------\n");
	}
}

/*
Procedure : rdump
Purpose   : Dump current register and bus values to the output file.
*/
void rdump(FILE *dumpsim_file)
{
	if (dump_stdout)
	{
		printf("@ Current register/bus values :\n");
		printf("-------------------------------------\n");
		printf("Ins Count : %08x\n", INSTRUCTION_COUNT);
		printf("PC        : %08x\n", CURRENT_STATE.PC);
		printf("HI        : %08x\n", CURRENT_STATE.HI);
		printf("LO        : %08x\n", CURRENT_STATE.LO);
		printf("Reg File  :\n");
		for (int k = 0; k < MIPS_REGS; k++)
		{
			printf("$%02d: %08x, ", k, CURRENT_STATE.REGS[k]);
			if (k % 4 == 3)
				printf("\n");
		}
		printf("-------------------------------------\n");
	}
	if (dump_file)
	{
		fprintf(dumpsim_file, "@ Current register/bus values :\n");
		fprintf(dumpsim_file, "-------------------------------------\n");
		fprintf(dumpsim_file, "Ins Count : %08x\n", INSTRUCTION_COUNT);
		fprintf(dumpsim_file, "PC        : %08x\n", CURRENT_STATE.PC);
		fprintf(dumpsim_file, "HI        : %08x\n", CURRENT_STATE.HI);
		fprintf(dumpsim_file, "LO        : %08x\n", CURRENT_STATE.LO);
		fprintf(dumpsim_file, "Reg File  :\n");
		for (int k = 0; k < MIPS_REGS; k++)
			fprintf(dumpsim_file, "$%d: %08x\n", k, CURRENT_STATE.REGS[k]);
		fprintf(dumpsim_file, "-------------------------------------\n");
	}
}

int cmdbuf_pointer = 0, quit_process = FALSE;
char command_buffer[64] = {' '};
inline uint32_t ch2digit(char ch)
{
	if ('0' <= ch && ch <= '9')
		return ch - '0';
	else if ('a' <= ch && ch <= 'f')
		return ch - 'a' + 10;
	else if ('A' <= ch && ch <= 'F')
		return ch - 'A' + 10;
	else
		return 0xffffffff;
}
uint32_t readnum(uint32_t base = 16)
{
	uint32_t num = 0, tmp;
	while ((tmp = ch2digit(command_buffer[cmdbuf_pointer])) < base)
	{
		num = num * base + tmp;
		cmdbuf_pointer++;
	}
	return num;
}
inline uint32_t is_space(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\t';
}
char skip()
{
	while (command_buffer[cmdbuf_pointer] && !is_space(command_buffer[cmdbuf_pointer]))
		cmdbuf_pointer++;
	while (is_space(command_buffer[cmdbuf_pointer]))
		cmdbuf_pointer++;
	return command_buffer[cmdbuf_pointer];
}
/*
Procedure : get_command
Purpose   : Read a command from standard input.
*/
void get_command(FILE *dumpsim_file)
{
	int start, stop, cycles;
	int register_no, register_value;
	int hi_reg_value, lo_reg_value;

	printf("\x1B[35mMIPS-SIM > \x1B[0m");
	// read a line
	if (scanf("%[^\n]", command_buffer + 1) == EOF)
		exit(0);
	getchar();
	cmdbuf_pointer = 0;
	show_assemble = show_detail = FALSE;
	dump_stdout = dump_file = TRUE;
	uint32_t legal_command = TRUE;
	char firstch = skip();
	switch (firstch)
	{
	case 'g':
	{
		char now = skip();
		if (now == 'a')
		{
			show_assemble = TRUE;
			now = skip();
			if (now == 'v')
				show_detail = TRUE;
			else if (now)
				legal_command = FALSE;
		}
		else if (now)
			legal_command = FALSE;
		if (legal_command)
			go();
		break;
	}
	case 'o':
	{
		if (skip())
		{
			cycles = readnum(10);
			char now = skip();
			if (now == 'a')
			{
				show_assemble = TRUE;
				now = skip();
				if (now == 'v')
					show_detail = TRUE;
				else if (now)
					legal_command = FALSE;
			}
			else if (now)
				legal_command = FALSE;
		}
		else
			legal_command = FALSE;
		if (legal_command)
			run(cycles);
		break;
	}
	case 'd':
	{
		char now = skip();
		if (now == 'm')
		{
			if (skip())
			{
				uint32_t low_addr = 0, hig_addr = 0;
				low_addr = readnum(16);
				if (skip())
				{
					hig_addr = readnum(16);
					while (now = skip())
					{
						if (now == 's')
							dump_stdout = FALSE;
						else if (now == 'f')
							dump_file = FALSE;
						else
							legal_command = FALSE;
					}
					if (legal_command)
						mdump(dumpsim_file, low_addr, hig_addr);
				}
				else
					legal_command = FALSE;
			}
			else
				legal_command = FALSE;
		}
		else if (now == 'r')
		{
			while (now = skip())
			{
				if (now == 's')
					dump_stdout = FALSE;
				else if (now == 'f')
					dump_file = FALSE;
				else
					legal_command = FALSE;
			}
			if (legal_command)
				rdump(dumpsim_file);
		}
		else
			legal_command = FALSE;
		if (legal_command && dump_file)
			fflush(dumpsim_file);
		break;
	}
	case 's':
	{
		char now = skip();
		uint32_t val = 0;
		if (ch2digit(now) < 16)
		{
			uint32_t rid = readnum(16);
			if (skip())
			{
				val = readnum(16);
				if (0 < rid && rid < 32)
					CURRENT_STATE.REGS[rid] = val;
				else
					legal_command = FALSE;
			}
			else
				legal_command = FALSE;
		}
		else if (skip())
		{
			val = readnum(16);
			switch (now)
			{
			case 'P':
			case 'p':
				CURRENT_STATE.PC = val;
				break;
			case 'H':
			case 'h':
				CURRENT_STATE.HI = val;
				break;
			case 'L':
			case 'l':
				CURRENT_STATE.LO = val;
				break;
			default:
				legal_command = FALSE;
				break;
			}
		}
		else
			legal_command = FALSE;
		break;
	}
	case 'r':
		RUN_BIT = TRUE;
		break;
	case 'h':
		help();
		break;
	case 'q':
		printf("@ Bye.\n");
		quit_process = TRUE;
		return;
	default:
		legal_command = FALSE;
		break;
	}
	if (!legal_command)
		printf("\x1B[31m@ Invalid command was given, use \"help\" to look up commands\n\x1B[0m");
	else
		printf("\x1B[32m@ Task finished\n\x1B[0m");
}

/*
Procedure : init_memory
Purpose   : Allocate and zero memory.
*/
void init_memory()
{
	for (int i = 0; i < MEM_NREGIONS; i++)
	{
		MEM_REGIONS[i].mem = new uint8_t[MEM_REGIONS[i].size];
		memset(MEM_REGIONS[i].mem, 0, MEM_REGIONS[i].size);
	}
}

/*
Procedure : load_program
Purpose   : Load program and service routines into mem.
*/
void load_program(char *program_filename)
{
	/* Open program file. */
	FILE *prog = fopen(program_filename, "rb");
	if (prog == NULL)
	{
		printf("@ Error: Can't open program file %s\n", program_filename);
		exit(-1);
	}

	/* Read in the program. */
	int offset = 0, word;
	while (TRUE)
	{
		if (!(int)fread(&word, sizeof(word), 1, prog))
			break;
		mem_write_32(MEM_TEXT_START + offset, word);
		offset += 4;
	}

	CURRENT_STATE.PC = MEM_TEXT_START;
	printf("@ Read %d words from program into memory.\n\n", offset / 4);
	fclose(prog);
}

/*
Procedure : initialize
Purpose   : Load machine language program and set up initial state of the machine.
*/
void initialize(char *program_filename, int num_prog_files)
{
	init_memory();
	for (int i = 0; i < num_prog_files; i++)
	{
		load_program(program_filename);
		while (*program_filename++ != '\0')
			;
	}
	NEXT_STATE = CURRENT_STATE;
	RUN_BIT = TRUE;
}

/* Procedure : main */
int main(int argc, char *argv[])
{
	/* Error Checking */
	if (argc < 2)
	{
		printf("@ Error: usage: %s <program_file_1> <program_file_2> ...\n", argv[0]);
		exit(1);
	}
	printf("@ MIPS Simulator Start\n\n");

	initialize(argv[1], argc - 1);

	FILE *dumpsim_file = fopen("dumpsim", "w");
	if (dumpsim_file == NULL)
	{
		printf("@ Error: Can't open dumpsim file\n");
		exit(-1);
	}

	while (!quit_process)
		get_command(dumpsim_file);
	fclose(dumpsim_file);
}
