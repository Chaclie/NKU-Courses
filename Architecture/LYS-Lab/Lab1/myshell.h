/***************************************************************/
/*   MIPS-32 Instruction Level Simulator                       */
/***************************************************************/

#ifndef _SIM_SHELL_H_
#define _SIM_SHELL_H_

#include <cstdint>

#define FALSE 0
#define TRUE 1
#define MIPS_REGS 32

typedef struct CPU_State_Struct
{
  uint32_t PC;              /* program counter */
  uint32_t REGS[MIPS_REGS]; /* register file. */
  uint32_t HI, LO;          /* special regs for mult/div. */
} CPU_State;

/* CPU State info */
extern CPU_State CURRENT_STATE, NEXT_STATE;
extern int RUN_BIT; /* run bit */

/* debug parameters */
extern int show_assemble, show_detail;
extern int dump_stdout, dump_file;

uint32_t mem_read_32(uint32_t address);
void mem_write_32(uint32_t address, uint32_t value);

void process_instruction();
void explain_instruction(uint32_t ins_address, uint32_t err, uint32_t verbose);
#endif
