#include <cstdio>
#include "myshell.h"

// the word which was stored where the memory was lately updated
uint32_t mem_before_write = 0;

/*get certain bits in the instruction*/
// fetch the instruction
inline uint32_t getInstruction()
{
    CURRENT_STATE.REGS[0] = 0;
    NEXT_STATE = CURRENT_STATE;
    NEXT_STATE.PC += 4;
    return mem_read_32(CURRENT_STATE.PC);
}
// 6-bit operation code (31:26)
inline uint32_t get_op(uint32_t ins) { return (ins >> 26) & (0x3f); }
// 5-bit source register specifier (25:21)
inline uint32_t get_rs(uint32_t ins) { return (ins >> 21) & (0x1f); }
// 5-bit target (source/destination) or branch condition (20:16)
inline uint32_t get_rt(uint32_t ins) { return (ins >> 16) & (0x1f); }
// 5-bit destination register specifier (15:11)
inline uint32_t get_rd(uint32_t ins) { return (ins >> 11) & (0x1f); }
// 5-bit shift amount (10:6)
inline uint32_t get_shamt(uint32_t ins) { return (ins >> 6) & (0x1f); }
// 6-bit function field (5:0)
inline uint32_t get_funct(uint32_t ins) { return (ins) & (0x3f); }
// 16-bit immediate, branch displacement or address displacement (15:0)
inline uint32_t get_immediate(uint32_t ins) { return (ins) & (0xffff); }
// 26-bit jump target address (25:0)
inline uint32_t get_target(uint32_t ins) { return (ins) & (0x3ffffff); }

// sign extend 8-bit num
inline uint32_t extend_sign_8(uint32_t num) { return num | ((num & (1 << 7)) ? 0xffffff00 : 0); }
// sign extend 16-bit num
inline uint32_t extend_sign_16(uint32_t num) { return num | ((num & (1 << 15)) ? 0xffff0000 : 0); }
// extract byte
inline uint32_t extract_byte(uint32_t num, uint32_t pos)
{
    if ((pos & 0x01) == 0)
        num &= 0xffffff00;
    if ((pos & 0x02) == 0)
        num &= 0xffff00ff;
    if ((pos & 0x04) == 0)
        num &= 0xff00ffff;
    if ((pos & 0x08) == 0)
        num &= 0x00ffffff;
    return num;
}

/*Exception*/
typedef enum
{
    NoError,
    UnknownError,
    UnknownInstruction,
    UnalignedAddress,
    Overflow
} ErrorCode;
inline void alert_exception(uint32_t ins, uint32_t err)
{
    if (err == NoError)
        return;
    printf("\x1B[31m");
    switch (err)
    {
    case UnknownInstruction:
        printf("Unknown MIPS Instruction: ");
        break;
    case UnalignedAddress:
        printf("Operation of Unaligned Address: ");
        break;
    case Overflow:
        printf("Overflow During Calculation: ");
        break;
    default:
        printf("Unknown Error: ");
        break;
    }
    printf("Ins-%08x, Err-%08x\n", ins, err);
    NEXT_STATE.PC = CURRENT_STATE.PC;
    RUN_BIT = FALSE;
    printf("\x1B[0m");
}

/*
the unique ID for every instruction:
funct for R type instructions,
op for most I type instructions and all J type instructions,
rt for BLTZ, BGEZ, BLTZAL, BGEZAL (though they are I type instructions, their op are the same - 0x01)
*/
typedef enum
{
    /*funct*/
    JALR = 011,
    JR = 010, // Jump
    SLL = 000,
    SRL = 002,
    SRA = 003,
    SLLV = 004,
    SRLV = 006,
    SRAV = 007, // Shift
    ADD = 040,
    ADDU = 041,
    SUB = 042,
    SUBU = 043,
    AND = 044,
    OR = 045,
    XOR = 046,
    NOR = 047,
    SLT = 052,
    SLTU = 053, // Arithmetic & Logic & Compare
    MULT = 030,
    MULTU = 031,
    DIV = 032,
    DIVU = 033, // Mul & Div
    MFHI = 020,
    MTHI = 021,
    MFLO = 022,
    MTLO = 023,    // HI & LO
    SYSCALL = 014, // System

    /*op*/
    J = 002,
    JAL = 003, // Jump
    LB = 040,
    LH = 041,
    LW = 043,
    LBU = 044,
    LHU = 045, // Load
    SB = 050,
    SH = 051,
    SW = 053, // Store
    BEQ = 004,
    BNE = 005,
    BLEZ = 006,
    BGTZ = 007, // Branch
    ADDI = 010,
    ADDIU = 011,
    SLTI = 012,
    SLTIU = 013,
    ANDI = 014,
    ORI = 015,
    XORI = 016,
    LUI = 017, // Arithmetic & Logic & Compare

    /*rt*/
    BLTZ = 000,
    BGEZ = 001,
    BLTZAL = 020,
    BGEZAL = 021, // Branch
} InstructionID;

/*Process Instruction*/
// R type
ErrorCode process_R_Jump(uint32_t funct, uint32_t rs, uint32_t rd)
{
    uint32_t tmp = CURRENT_STATE.REGS[rd];
    if (funct == JALR)
        tmp = CURRENT_STATE.PC + 4;
    else if (funct != JR)
        return UnknownInstruction;
    else if (CURRENT_STATE.REGS[rs] & 003)
        return UnalignedAddress;
    NEXT_STATE.PC = CURRENT_STATE.REGS[rs];
    NEXT_STATE.REGS[rd] = tmp;
    return NoError;
}
ErrorCode process_R_Shift(uint32_t funct, uint32_t rs, uint32_t rt, uint32_t rd, uint32_t shamt)
{
    if (funct & 070)
        return UnknownInstruction;
    if (funct & 004)
        shamt = CURRENT_STATE.REGS[rs] & 0x1f;
    funct &= 003;
    if (funct == 0)
        NEXT_STATE.REGS[rd] = CURRENT_STATE.REGS[rt] << shamt;
    else if (funct == 2)
        NEXT_STATE.REGS[rd] = CURRENT_STATE.REGS[rt] >> shamt;
    else if (funct == 3)
        NEXT_STATE.REGS[rd] = extend_sign_16(CURRENT_STATE.REGS[rt] >> shamt);
    else
        return UnknownInstruction;
    return NoError;
}
ErrorCode process_R_ALC(uint32_t funct, uint32_t rs, uint32_t rt, uint32_t rd)
{
    switch (funct)
    {
    case ADD:
    case ADDU:
        NEXT_STATE.REGS[rd] = CURRENT_STATE.REGS[rs] + CURRENT_STATE.REGS[rt];
        break;
    case SUB:
    case SUBU:
        NEXT_STATE.REGS[rd] = CURRENT_STATE.REGS[rs] - CURRENT_STATE.REGS[rt];
        break;
    case AND:
        NEXT_STATE.REGS[rd] = CURRENT_STATE.REGS[rs] & CURRENT_STATE.REGS[rt];
        break;
    case OR:
        NEXT_STATE.REGS[rd] = CURRENT_STATE.REGS[rs] | CURRENT_STATE.REGS[rt];
        break;
    case XOR:
        NEXT_STATE.REGS[rd] = CURRENT_STATE.REGS[rs] ^ CURRENT_STATE.REGS[rt];
        break;
    case NOR:
        NEXT_STATE.REGS[rd] = ~(CURRENT_STATE.REGS[rs] | CURRENT_STATE.REGS[rt]);
        break;
    case SLT:
        NEXT_STATE.REGS[rd] = ((int32_t)CURRENT_STATE.REGS[rs] < (int32_t)CURRENT_STATE.REGS[rt]);
        break;
    case SLTU:
        NEXT_STATE.REGS[rd] = (CURRENT_STATE.REGS[rs] < CURRENT_STATE.REGS[rt]);
        break;
    default:
        return UnknownInstruction;
    }
    return NoError;
}
ErrorCode process_R_MulDiv(uint32_t funct, uint32_t rs, uint32_t rt)
{
    if ((funct & 074) != 030)
        return UnknownInstruction;
    funct &= 003;
    switch (funct)
    {
    case 0:
    {
        int64_t prod = (int64_t)((int32_t)CURRENT_STATE.REGS[rs]) * (int32_t)CURRENT_STATE.REGS[rt];
        NEXT_STATE.HI = (prod >> 32) & 0xffffffff;
        NEXT_STATE.LO = (prod) & 0xffffffff;
        break;
    }
    case 1:
    {
        uint64_t prod = (uint64_t)CURRENT_STATE.REGS[rs] * CURRENT_STATE.REGS[rt];
        NEXT_STATE.HI = (prod >> 32) & 0xffffffff;
        NEXT_STATE.LO = (prod) & 0xffffffff;
        break;
    }
    case 2:
    {
        NEXT_STATE.HI = (int32_t)CURRENT_STATE.REGS[rs] % (int32_t)CURRENT_STATE.REGS[rt];
        NEXT_STATE.LO = (int32_t)CURRENT_STATE.REGS[rs] / (int32_t)CURRENT_STATE.REGS[rt];
        break;
    }
    case 3:
    {
        NEXT_STATE.HI = CURRENT_STATE.REGS[rs] % CURRENT_STATE.REGS[rt];
        NEXT_STATE.LO = CURRENT_STATE.REGS[rs] / CURRENT_STATE.REGS[rt];
        break;
    }
    }
    return NoError;
}
ErrorCode process_R_HILO(uint32_t funct, uint32_t rs, uint32_t rd)
{
    if ((funct & 074) != 020)
        return UnknownInstruction;
    funct &= 003;
    switch (funct)
    {
    case 0:
        NEXT_STATE.REGS[rd] = CURRENT_STATE.HI;
        break;
    case 1:
        NEXT_STATE.HI = CURRENT_STATE.REGS[rs];
        break;
    case 2:
        NEXT_STATE.REGS[rd] = CURRENT_STATE.LO;
        break;
    case 3:
        NEXT_STATE.LO = CURRENT_STATE.REGS[rs];
        break;
    }
    return NoError;
}
ErrorCode process_R_SYSCALL(uint32_t funct)
{
    if (funct == 014)
    {
        NEXT_STATE.REGS[2] = 0x0A;
        RUN_BIT = FALSE;
        return NoError;
    }
    else
        return UnknownInstruction;
}
// J type
ErrorCode process_J_Jump(uint32_t op, uint32_t targt)
{
    uint32_t target_address = (CURRENT_STATE.PC & 0xf0000000) | ((targt * 4) & 0x0fffffff);
    NEXT_STATE.PC = target_address;
    if (op == JAL)
        NEXT_STATE.REGS[31] = CURRENT_STATE.PC + 4;
    else if (op != J)
        return UnknownInstruction;
    return NoError;
}
// I type
ErrorCode process_I_Load(uint32_t op, uint32_t rs, uint32_t rt, uint32_t imm)
{
    if ((op & 070) != 040 || (op & 003) == 002 || (op & 007) == 007)
        return UnknownInstruction;
    uint32_t src_address = CURRENT_STATE.REGS[rs] + extend_sign_16(imm);
    if (((op & 003) == 003 && (src_address & 003)) || ((op & 003) == 001 && (src_address & 001)))
        return UnalignedAddress;
    uint32_t src_word = mem_read_32(src_address / 4 * 4);
    if (src_address & 002)
        src_word >>= 16;
    if (src_address & 001)
        src_word >>= 8;
    if ((op & 003) == 001)
    {
        src_word &= 0xffff;
        if ((op & 004) == 000)
            src_word = extend_sign_16(src_word);
    }
    else if ((op & 003) == 000)
    {
        src_word &= 0xff;
        if ((op & 004) == 000)
            src_word = extend_sign_8(src_word);
    }
    NEXT_STATE.REGS[rt] = src_word;
    return NoError;
}
ErrorCode process_I_Store(uint32_t op, uint32_t rs, uint32_t rt, uint32_t imm)
{
    if ((op & 070) != 050 || (op & 004) || (op & 003) == 002)
        return UnknownInstruction;
    uint32_t des_address = CURRENT_STATE.REGS[rs] + extend_sign_16(imm);
    if (((op & 003) == 003 && (des_address & 003)) || ((op & 003) == 001 && (des_address & 001)))
        return UnalignedAddress;
    uint32_t des_word = CURRENT_STATE.REGS[rt], org_word = mem_read_32(des_address / 4 * 4);
    mem_before_write = des_word;
    uint32_t des_pos = 0x0f, org_pos = 0x00;
    if ((op & 003) == 000)
        des_pos = 0x01;
    else if ((op & 003) == 001)
        des_pos = 0x03;
    des_word = extract_byte(des_word, des_pos);
    if ((op & 003) == 000)
    {
        des_pos = 1 << (des_address & 003);
        des_word <<= 8 * (des_address & 003);
    }
    else if ((op & 003) == 001)
    {
        des_pos = 3 << (des_address & 002);
        des_word <<= 8 * (des_address & 002);
    }
    org_pos = 0x0f ^ des_pos;
    des_word = des_word | extract_byte(org_word, org_pos);
    mem_write_32(des_address / 4 * 4, des_word);
    return NoError;
}
ErrorCode process_I_Branch(uint32_t op, uint32_t rs, uint32_t rt, uint32_t imm)
{
    uint32_t target_branch = CURRENT_STATE.PC + 4 + extend_sign_16(imm) * 4;
    if (op == 001)
    {
        if (rt & 0x0e)
            return UnknownInstruction;
        if (rt & 0x10)
            NEXT_STATE.REGS[31] = CURRENT_STATE.PC + 4;
        if ((rt & 0x01) && (CURRENT_STATE.REGS[rs] & 0x80000000) == 0)
            NEXT_STATE.PC = target_branch;
        else if ((rt & 0x01) == 0 && (CURRENT_STATE.REGS[rs] & 0x80000000))
            NEXT_STATE.PC = target_branch;
        return NoError;
    }
    if ((op & 070) != 000 || (op & 004) == 0)
        return UnknownInstruction;
    uint32_t branch_flag = FALSE;
    if ((op & 002) == 0 && (CURRENT_STATE.REGS[rs] == CURRENT_STATE.REGS[rt]))
        branch_flag = TRUE;
    else if ((op & 002) && ((int32_t)CURRENT_STATE.REGS[rs] <= 0))
        branch_flag = TRUE;
    if (op & 001)
        branch_flag = !branch_flag;
    if (branch_flag)
        NEXT_STATE.PC = target_branch;
    return NoError;
}
ErrorCode process_I_ALC(uint32_t op, uint32_t rs, uint32_t rt, uint32_t imm)
{

    switch (op)
    {
    case ADDI:
    case ADDIU:
        NEXT_STATE.REGS[rt] = CURRENT_STATE.REGS[rs] + extend_sign_16(imm);
        break;
    case ANDI:
        NEXT_STATE.REGS[rt] = CURRENT_STATE.REGS[rs] & imm;
        break;
    case ORI:
        NEXT_STATE.REGS[rt] = CURRENT_STATE.REGS[rs] | imm;
        break;
    case XORI:
        NEXT_STATE.REGS[rt] = CURRENT_STATE.REGS[rs] ^ imm;
        break;
    case SLTI:
        NEXT_STATE.REGS[rt] = ((int32_t)CURRENT_STATE.REGS[rs] < (int32_t)extend_sign_16(imm));
        break;
    case SLTIU:
        NEXT_STATE.REGS[rt] = (CURRENT_STATE.REGS[rs] < extend_sign_16(imm));
        break;
    case LUI:
        NEXT_STATE.REGS[rt] = imm << 16;
        break;
    default:
        return UnknownInstruction;
    }
    return NoError;
}
// general
void process_instruction()
{
    /* execute one instruction here. You should use CURRENT_STATE and modify
     * values in NEXT_STATE. You can call mem_read_32() and mem_write_32() to
     * access memory. */
    uint32_t ins = getInstruction();
    uint32_t op = get_op(ins);
    uint32_t rs = get_rs(ins), rt = get_rt(ins), rd = get_rd(ins);
    uint32_t shamt = get_shamt(ins), funct = get_funct(ins);
    uint32_t imm = get_immediate(ins), targt = get_target(ins);
    uint32_t err = NoError;

    if (op == 000)
    {
        // R type
        switch (funct & 070)
        {
        case 000:
            err = process_R_Shift(funct, rs, rt, rd, shamt);
            break;
        case 010:
        {
            if (funct == SYSCALL)
                err = process_R_SYSCALL(funct);
            else
                err = process_R_Jump(funct, rs, rd);
            break;
        }
        case 020:
            err = process_R_HILO(funct, rs, rd);
            break;
        case 030:
            err = process_R_MulDiv(funct, rs, rt);
            break;
        case 040:
        case 050:
            err = process_R_ALC(funct, rs, rt, rd);
            break;
        default:
            err = UnknownInstruction;
            break;
        }
    }
    else
    {
        switch (op & 070)
        {
        case 000:
        {
            if (op == J || op == JAL)
                err = process_J_Jump(op, targt);
            else
                err = process_I_Branch(op, rs, rt, imm);
            break;
        }
        case 010:
            err = process_I_ALC(op, rs, rt, imm);
            break;
        case 040:
            err = process_I_Load(op, rs, rt, imm);
            break;
        case 050:
            err = process_I_Store(op, rs, rt, imm);
            break;
        default:
            err = UnknownInstruction;
            break;
        }
    }
    // printf("@debug in sim.cpp: ins=%08x\n", ins);
    if (err != NoError)
        alert_exception(ins, err);
    if (show_assemble)
        explain_instruction(CURRENT_STATE.PC, err, show_detail);
}

/*Explain Instruction*/
// R type
void explain_R_Jump(uint32_t funct, uint32_t rs, uint32_t rd, uint32_t verbose)
{
    if (funct == JALR)
    {
        printf("JALR rd=$%d, rs=$%d\n", rd, rs);
        if (verbose)
        {
            printf("\t$%d <- (($PC: %08x) + 4): $%d changes from %08x to %08x\n",
                   rd, CURRENT_STATE.PC, rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
            printf("\t$PC <- ($%d: %08x): $PC changes from %08x to %08x\n",
                   rs, CURRENT_STATE.REGS[rs], CURRENT_STATE.PC, NEXT_STATE.PC);
        }
    }
    else if (funct == JR)
    {
        printf("JR rs=$%d\n", rs);
        if (verbose)
        {
            printf("\t$PC <- ($%d: %08x): $PC changes from %08x to %08x\n",
                   rs, CURRENT_STATE.REGS[rs], CURRENT_STATE.PC, NEXT_STATE.PC);
        }
    }
}
void explain_R_Shift(uint32_t funct, uint32_t rs, uint32_t rt, uint32_t rd, uint32_t shamt, uint32_t verbose)
{
    switch (funct)
    {
    case SLL:
    {
        printf("SLL rd=$%d, rt=$%d, shamt=%d\n", rd, rt, shamt);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) << %d), $%d changes from %08x to %08x\n",
                   rd, rt, CURRENT_STATE.REGS[rt], shamt, rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    case SRL:
    {
        printf("SRL rd=$%d, rt=$%d, shamt=%d\n", rd, rt, shamt);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) >> %d) (zero extend), $%d changes from %08x to %08x\n",
                   rd, rt, CURRENT_STATE.REGS[rt], shamt, rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    case SRA:
    {
        printf("SRA rd=$%d, rt=$%d, shamt=%d\n", rd, rt, shamt);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) >> %d) (sign extend), $%d changes from %08x to %08x\n",
                   rd, rt, CURRENT_STATE.REGS[rt], shamt, rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    case SLLV:
    {
        printf("SLLV rd=$%d, rt=$%d, rs=$%d\n", rd, rt, rs);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) << (($%d: %08x) & 0x1f)), $%d changes from %08x to %08x\n",
                   rd, rt, CURRENT_STATE.REGS[rt], rs, CURRENT_STATE.REGS[rs],
                   rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    case SRLV:
    {
        printf("SRL rd=$%d, rt=$%d, rs=$%d\n", rd, rt, rs);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) >> (($%d: %08x) & 0x1f)) (zero extend), $%d changes from %08x to %08x\n",
                   rd, rt, CURRENT_STATE.REGS[rt], rs, CURRENT_STATE.REGS[rs],
                   rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    case SRAV:
    {
        printf("SRAV rd=$%d, rt=$%d, rs=$%d\n", rd, rt, rs);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) >> (($%d: %08x) & 0x1f)) (sign extend), $%d changes from %08x to %08x\n",
                   rd, rt, CURRENT_STATE.REGS[rt], rs, CURRENT_STATE.REGS[rs],
                   rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    }
}
void explain_R_ALC(uint32_t funct, uint32_t rs, uint32_t rt, uint32_t rd, uint32_t verbose)
{
    switch (funct)
    {
    case ADD:
    {
        printf("ADD rd=$%d, rs=$%d, rt=$%d\n", rd, rs, rt);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) + ($%d: %08x)): $%d changes from %08x to %08x\n",
                   rd, rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt],
                   rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    case ADDU:
    {
        printf("ADDU rd=$%d, rs=$%d, rt=$%d\n", rd, rs, rt);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) + ($%d: %08x)): $%d changes from %08x to %08x\n",
                   rd, rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt],
                   rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    case SUB:
    {
        printf("SUB rd=$%d, rs=$%d, rt=$%d\n", rd, rs, rt);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) - ($%d: %08x)): $%d changes from %08x to %08x\n",
                   rd, rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt],
                   rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    case SUBU:
    {
        printf("SUBU rd=$%d, rs=$%d, rt=$%d\n", rd, rs, rt);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) - ($%d: %08x)): $%d changes from %08x to %08x\n",
                   rd, rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt],
                   rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    case AND:
    {
        printf("AND rd=$%d, rs=$%d, rt=$%d\n", rd, rs, rt);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) & ($%d: %08x)): $%d changes from %08x to %08x\n",
                   rd, rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt],
                   rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    case OR:
    {
        printf("OR rd=$%d, rs=$%d, rt=$%d\n", rd, rs, rt);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) | ($%d: %08x)): $%d changes from %08x to %08x\n",
                   rd, rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt],
                   rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    case XOR:
    {
        printf("XOR rd=$%d, rs=$%d, rt=$%d\n", rd, rs, rt);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) ^ ($%d: %08x)): $%d changes from %08x to %08x\n",
                   rd, rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt],
                   rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    case NOR:
    {
        printf("NOR rd=$%d, rs=$%d, rt=$%d\n", rd, rs, rt);
        if (verbose)
        {
            printf("\t$%d <- ~(($%d: %08x) | ($%d: %08x)): $%d changes from %08x to %08x\n",
                   rd, rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt],
                   rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    case SLT:
    {
        printf("SLT rd=$%d, rs=$%d, rt=$%d\n", rd, rs, rt);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) < ($%d: %08x)) (sign compare): $%d changes from %08x to %08x\n",
                   rd, rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt],
                   rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    case SLTU:
    {
        printf("SLT rd=$%d, rs=$%d, rt=$%d\n", rd, rs, rt);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) < ($%d: %08x)) (unsg compare): $%d changes from %08x to %08x\n",
                   rd, rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt],
                   rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    }
}
void explain_R_MulDiv(uint32_t funct, uint32_t rs, uint32_t rt, uint32_t verbose)
{
    switch (funct)
    {
    case MULT:
    {
        printf("MULT rs=$%d, rt=$%d\n", rs, rt);
        if (verbose)
        {
            printf("\tprod <- (($%d: %08x) * ($%d: %08x)) (sign extend): $HI changes from %08x to %08x, $LO changes from %08x to %08x\n",
                   rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt],
                   CURRENT_STATE.HI, NEXT_STATE.HI, CURRENT_STATE.LO, NEXT_STATE.LO);
        }
        break;
    }
    case MULTU:
    {
        printf("MULTU rs=$%d, rt=$%d\n", rs, rt);
        if (verbose)
        {
            printf("\tprod <- (($%d: %08x) * ($%d: %08x)) (unsg extend): $HI changes from %08x to %08x, $LO changes from %08x to %08x\n",
                   rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt],
                   CURRENT_STATE.HI, NEXT_STATE.HI, CURRENT_STATE.LO, NEXT_STATE.LO);
        }
        break;
    }
    case DIV:
    {
        printf("DIV rs=$%d, rt=$%d\n", rs, rt);
        if (verbose)
        {
            printf("\t$LO <- (($%d: %08x) / ($%d: %08x)) (sign extend): $LO changes from %08x to %08x\n",
                   rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt],
                   CURRENT_STATE.LO, NEXT_STATE.LO);
            printf("\t$HI <- (($%d: %08x) %% ($%d: %08x)) (sign extend): $HI changes from %08x to %08x\n",
                   rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt],
                   CURRENT_STATE.HI, NEXT_STATE.HI);
        }
        break;
    }
    case DIVU:
    {
        printf("DIVU rs=$%d, rt=$%d\n", rs, rt);
        if (verbose)
        {
            printf("\t$LO <- (($%d: %08x) / ($%d: %08x)) (unsg extend): $LO changes from %08x to %08x\n",
                   rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt],
                   CURRENT_STATE.LO, NEXT_STATE.LO);
            printf("\t$HI <- (($%d: %08x) %% ($%d: %08x)) (unsg extend): $HI changes from %08x to %08x\n",
                   rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt],
                   CURRENT_STATE.HI, NEXT_STATE.HI);
        }
        break;
    }
    }
}
void explain_R_HILO(uint32_t funct, uint32_t rs, uint32_t rd, uint32_t verbose)
{
    switch (funct)
    {
    case MFHI:
    {
        printf("MFHI rd=$%d\n", rd);
        if (verbose)
        {
            printf("\t$%d <- ($HI: %08x): $%d changes from %08x to %08x\n",
                   rd, CURRENT_STATE.HI, rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    case MTHI:
    {
        printf("MTHI rs=$%d\n", rs);
        if (verbose)
        {
            printf("\t$HI <- ($%d: %08x): $HI changes from %08x to %08x\n",
                   rs, CURRENT_STATE.REGS[rs], CURRENT_STATE.HI, NEXT_STATE.HI);
        }
        break;
    }
    case MFLO:
    {
        printf("MFLO rd=$%d\n", rd);
        if (verbose)
        {
            printf("\t$%d <- ($LO: %08x): $%d changes from %08x to %08x\n",
                   rd, CURRENT_STATE.LO, rd, CURRENT_STATE.REGS[rd], NEXT_STATE.REGS[rd]);
        }
        break;
    }
    case MTLO:
    {
        printf("MTLO rs=$%d\n", rs);
        if (verbose)
        {
            printf("\t$LO <- ($%d: %08x): $LO changes from %08x to %08x\n",
                   rs, CURRENT_STATE.REGS[rs], CURRENT_STATE.LO, NEXT_STATE.LO);
        }
        break;
    }
    }
}
void explain_R_SYSCALL(uint32_t funct, uint32_t verbose)
{
    if (funct == SYSCALL)
    {
        printf("SYSCALL\n");
        if (verbose)
        {
            printf("\t$2 <- %08x: $2 changes from %08x to %08x\n",
                   0x0A, CURRENT_STATE.REGS[2], NEXT_STATE.REGS[2]);
            printf("\tRUN_BIT <- FALSE\n");
        }
    }
}
// J type
void explain_J_Jump(uint32_t op, uint32_t targt, uint32_t verbose)
{
    if (op == JAL)
    {
        printf("JAL target=$%08x\n", targt);
        if (verbose)
        {
            printf("\t$%d <- (($PC: %08x) + 4): $%d changes from %08x to %08x\n",
                   31, CURRENT_STATE.PC, 31, CURRENT_STATE.REGS[31], NEXT_STATE.REGS[31]);
            printf("\t$PC <- ((($PC: %08x) & 0xf0000000) | (((target: %08x) * 4) & 0x0fffffff)): $PC changes from %08x to %08x\n",
                   CURRENT_STATE.PC, targt, CURRENT_STATE.PC, NEXT_STATE.PC);
        }
    }
    else if (op == J)
    {
        printf("J target=$%08x\n", targt);
        if (verbose)
        {
            printf("\t$PC <- ((($PC: %08x) & 0xf0000000) | (((target: %08x) * 4) & 0x0fffffff)): $PC changes from %08x to %08x\n",
                   CURRENT_STATE.PC, targt, CURRENT_STATE.PC, NEXT_STATE.PC);
        }
    }
}
// I type
void explain_I_Load(uint32_t op, uint32_t rs, uint32_t rt, uint32_t imm, uint32_t verbose)
{
    uint32_t src_address = CURRENT_STATE.REGS[rs] + extend_sign_16(imm);
    uint32_t src_word_address = src_address / 4 * 4;
    uint32_t src_word = mem_read_32(src_word_address);
    switch (op)
    {
    case LB:
    {
        printf("LB rt=$%d, imm=%08x(rs=$%d)\n", rt, imm, rs);
        if (verbose)
        {
            printf("\tsrc_address <- (($%d: %08x) + (imm: %08x)): src_address is %08x\n",
                   rs, CURRENT_STATE.REGS[rs], imm, src_address);
            printf("\t$%d <- (byte %d of word ([%08x]: %08x)) (sign extend): $%d changes from %08x to %08x\n",
                   rt, src_address & 003, src_word_address, src_word,
                   rt, CURRENT_STATE.REGS[rt], NEXT_STATE.REGS[rt]);
        }
        break;
    }
    case LH:
    {
        printf("LH rt=$%d, imm=%08x(rs=$%d)\n", rt, imm, rs);
        if (verbose)
        {
            printf("\tsrc_address <- (($%d: %08x) + (imm: %08x)): src_address is %08x\n",
                   rs, CURRENT_STATE.REGS[rs], imm, src_address);
            printf("\t$%d <- (hfwd %d of word ([%08x]: %08x)) (sign extend): $%d changes from %08x to %08x\n",
                   rt, src_address & 002, src_word_address, src_word,
                   rt, CURRENT_STATE.REGS[rt], NEXT_STATE.REGS[rt]);
        }
        break;
    }
    case LW:
    {
        printf("LW rt=$%d, imm=%08x(rs=$%d)\n", rt, imm, rs);
        if (verbose)
        {
            printf("\tsrc_address <- (($%d: %08x) + (imm: %08x)): src_address is %08x\n",
                   rs, CURRENT_STATE.REGS[rs], imm, src_address);
            printf("\t$%d <- ([%08x]: %08x): $%d changes from %08x to %08x\n",
                   rt, src_word_address, src_word,
                   rt, CURRENT_STATE.REGS[rt], NEXT_STATE.REGS[rt]);
        }
        break;
    }
    case LBU:
    {
        printf("LBU rt=$%d, imm=%08x(rs=$%d)\n", rt, imm, rs);
        if (verbose)
        {
            printf("\tsrc_address <- (($%d: %08x) + (imm: %08x)): src_address is %08x\n",
                   rs, CURRENT_STATE.REGS[rs], imm, src_address);
            printf("\t$%d <- (byte %d of word ([%08x]: %08x)) (zero extend): $%d changes from %08x to %08x\n",
                   rt, src_address & 003, src_word_address, src_word,
                   rt, CURRENT_STATE.REGS[rt], NEXT_STATE.REGS[rt]);
        }
        break;
    }
    case LHU:
    {
        printf("LHU rt=$%d, imm=%08x(rs=$%d)\n", rt, imm, rs);
        if (verbose)
        {
            printf("\tsrc_address <- (($%d: %08x) + (imm: %08x)): src_address is %08x\n",
                   rs, CURRENT_STATE.REGS[rs], imm, src_address);
            printf("\t$%d <- (hfwd %d of word ([%08x]: %08x)) (zero extend): $%d changes from %08x to %08x\n",
                   rt, src_address & 002, src_word_address, src_word,
                   rt, CURRENT_STATE.REGS[rt], NEXT_STATE.REGS[rt]);
        }
        break;
    }
    }
}
void explain_I_Store(uint32_t op, uint32_t rs, uint32_t rt, uint32_t imm, uint32_t verbose)
{
    uint32_t des_address = CURRENT_STATE.REGS[rs] + extend_sign_16(imm);
    uint32_t des_word_address = des_address / 4 * 4;
    uint32_t des_word = mem_read_32(des_word_address);
    switch (op)
    {
    case SB:
    {
        printf("SB rt=$%d, imm=%08x(rs=$%d)\n", rt, imm, rs);
        if (verbose)
        {
            printf("\tdes_address <- (($%d: %08x) + (imm: %08x)): des_address is %08x, byte %d of word %08x\n",
                   rs, CURRENT_STATE.REGS[rs], imm, des_address, des_address & 003, des_word_address);
            printf("\t[%08x] <- ($%d: %08x): [%08x] changes from %08x to %08x\n",
                   des_word_address, rt, CURRENT_STATE.REGS[rt], des_word_address, mem_before_write, des_word);
        }
        break;
    }
    case SH:
    {
        printf("SH rt=$%d, imm=%08x(rs=$%d)\n", rt, imm, rs);
        if (verbose)
        {
            printf("\tdes_address <- (($%d: %08x) + (imm: %08x)): des_address is %08x, hfwd %d of word %08x\n",
                   rs, CURRENT_STATE.REGS[rs], imm, des_address, des_address & 002, des_word_address);
            printf("\t[%08x] <- ($%d: %08x): [%08x] changes from %08x to %08x\n",
                   des_word_address, rt, CURRENT_STATE.REGS[rt], des_word_address, mem_before_write, des_word);
        }
        break;
    }
    case SW:
    {
        printf("SW rt=$%d, imm=%08x(rs=$%d)\n", rt, imm, rs);
        if (verbose)
        {
            printf("\tdes_address <- (($%d: %08x) + (imm: %08x)): des_address is %08x\n",
                   rs, CURRENT_STATE.REGS[rs], imm, des_address);
            printf("\t[%08x] <- ($%d: %08x): [%08x] changes from %08x to %08x\n",
                   des_word_address, rt, CURRENT_STATE.REGS[rt], des_word_address, mem_before_write, des_word);
        }
        break;
    }
    }
}
void explain_I_Branch(uint32_t op, uint32_t rs, uint32_t rt, uint32_t imm, uint32_t verbose)
{
    uint32_t branch_address = CURRENT_STATE.PC + 4 + extend_sign_16(imm) * 4;
    if (op == 001)
    {
        switch (rt)
        {
        case BLTZ:
        {
            printf("BLTZ rs=$%d, imm=%08x\n", rs, imm);
            if (verbose)
            {
                printf("\tbranch_address <- (($PC: %08x) + 4 + ((sign extend (imm: %08x)) * 4)): branch_address is %08x\n",
                       CURRENT_STATE.PC, imm, branch_address);
                printf("\t$PC <- ((($%d: %08x) < 0) ? (branch_address: %08x) : ((($PC: %08x) + 4): %08x)): $PC changes from %08x to %08x\n",
                       rs, CURRENT_STATE.REGS[rs], branch_address, CURRENT_STATE.PC, CURRENT_STATE.PC + 4, CURRENT_STATE.PC, NEXT_STATE.PC);
            }
            break;
        }
        case BLTZAL:
        {
            printf("BLTZAL rs=$%d, imm=%08x\n", rs, imm);
            if (verbose)
            {
                printf("\tbranch_address <- (($PC: %08x) + 4 + ((sign extend (imm: %08x)) * 4)): branch_address is %08x\n",
                       CURRENT_STATE.PC, imm, branch_address);
                printf("\t$%d <- (($PC: %08x) + 4): %08x): $%d changes from %08x to %08x\n",
                       31, CURRENT_STATE.PC, CURRENT_STATE.PC + 4, 31, CURRENT_STATE.REGS[31], NEXT_STATE.REGS[31]);
                printf("\t$PC <- ((($%d: %08x) < 0) ? (branch_address: %08x) : ((($PC: %08x) + 4): %08x)): $PC changes from %08x to %08x\n",
                       rs, CURRENT_STATE.REGS[rs], branch_address, CURRENT_STATE.PC, CURRENT_STATE.PC + 4, CURRENT_STATE.PC, NEXT_STATE.PC);
            }
            break;
        }
        case BGEZ:
        {
            printf("BGEZ rs=$%d, imm=%08x\n", rs, imm);
            if (verbose)
            {
                printf("\tbranch_address <- (($PC: %08x) + 4 + ((sign extend (imm: %08x)) * 4)): branch_address is %08x\n",
                       CURRENT_STATE.PC, imm, branch_address);
                printf("\t$PC <- ((($%d: %08x) >= 0) ? (branch_address: %08x) : ((($PC: %08x) + 4): %08x)): $PC changes from %08x to %08x\n",
                       rs, CURRENT_STATE.REGS[rs], branch_address, CURRENT_STATE.PC, CURRENT_STATE.PC + 4, CURRENT_STATE.PC, NEXT_STATE.PC);
            }
            break;
        }
        case BGEZAL:
        {
            printf("BGEZAL rs=$%d, imm=%08x\n", rs, imm);
            if (verbose)
            {
                printf("\tbranch_address <- (($PC: %08x) + 4 + ((sign extend (imm: %08x)) * 4)): branch_address is %08x\n",
                       CURRENT_STATE.PC, imm, branch_address);
                printf("\t$%d <- (($PC: %08x) + 4): %08x): $%d changes from %08x to %08x\n",
                       31, CURRENT_STATE.PC, CURRENT_STATE.PC + 4, 31, CURRENT_STATE.REGS[31], NEXT_STATE.REGS[31]);
                printf("\t$PC <- ((($%d: %08x) >= 0) ? (branch_address: %08x) : ((($PC: %08x) + 4): %08x)): $PC changes from %08x to %08x\n",
                       rs, CURRENT_STATE.REGS[rs], branch_address, CURRENT_STATE.PC, CURRENT_STATE.PC + 4, CURRENT_STATE.PC, NEXT_STATE.PC);
            }
            break;
        }
        }
    }
    else
    {
        switch (op)
        {
        case BEQ:
        {
            printf("BEQ rs=$%d, rt=$%d, imm=%08x\n", rs, rt, imm);
            if (verbose)
            {
                printf("\tbranch_address <- (($PC: %08x) + 4 + ((sign extend (imm: %08x)) * 4)): branch_address is %08x\n",
                       CURRENT_STATE.PC, imm, branch_address);
                printf("\t$PC <- ((($%d: %08x) == ($%d: %08x)) ? (branch_address: %08x) : ((($PC: %08x) + 4): %08x)): $PC changes from %08x to %08x\n",
                       rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt], branch_address, CURRENT_STATE.PC, CURRENT_STATE.PC + 4, CURRENT_STATE.PC, NEXT_STATE.PC);
            }
            break;
        }
        case BNE:
        {
            printf("BNE rs=$%d, rt=$%d, imm=%08x\n", rs, rt, imm);
            if (verbose)
            {
                printf("\tbranch_address <- (($PC: %08x) + 4 + ((sign extend (imm: %08x)) * 4)): branch_address is %08x\n",
                       CURRENT_STATE.PC, imm, branch_address);
                printf("\t$PC <- ((($%d: %08x) != ($%d: %08x)) ? (branch_address: %08x) : ((($PC: %08x) + 4): %08x)): $PC changes from %08x to %08x\n",
                       rs, CURRENT_STATE.REGS[rs], rt, CURRENT_STATE.REGS[rt], branch_address, CURRENT_STATE.PC, CURRENT_STATE.PC + 4, CURRENT_STATE.PC, NEXT_STATE.PC);
            }
            break;
        }
        case BLEZ:
        {
            printf("BLEZ rs=$%d, imm=%08x\n", rs, imm);
            if (verbose)
            {
                printf("\tbranch_address <- (($PC: %08x) + 4 + ((sign extend (imm: %08x)) * 4)): branch_address is %08x\n",
                       CURRENT_STATE.PC, imm, branch_address);
                printf("\t$PC <- ((($%d: %08x) <= 0) ? (branch_address: %08x) : ((($PC: %08x) + 4): %08x)): $PC changes from %08x to %08x\n",
                       rs, CURRENT_STATE.REGS[rs], branch_address, CURRENT_STATE.PC, CURRENT_STATE.PC + 4, CURRENT_STATE.PC, NEXT_STATE.PC);
            }
            break;
        }
        case BGTZ:
        {
            printf("BGTZ rs=$%d, imm=%08x\n", rs, imm);
            if (verbose)
            {
                printf("\tbranch_address <- (($PC: %08x) + 4 + ((sign extend (imm: %08x)) * 4)): branch_address is %08x\n",
                       CURRENT_STATE.PC, imm, branch_address);
                printf("\t$PC <- ((($%d: %08x) > 0) ? (branch_address: %08x) : ((($PC: %08x) + 4): %08x)): $PC changes from %08x to %08x\n",
                       rs, CURRENT_STATE.REGS[rs], branch_address, CURRENT_STATE.PC, CURRENT_STATE.PC + 4, CURRENT_STATE.PC, NEXT_STATE.PC);
            }
            break;
        }
        }
    }
}
void explain_I_ALC(uint32_t op, uint32_t rs, uint32_t rt, uint32_t imm, uint32_t verbose)
{
    switch (op)
    {
    case ADDI:
    {
        printf("ADDI rt=$%d, rs=$%d, imm=%08x\n", rt, rs, imm);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) + (sign extend imm: %08x)): $%d changes from %08x to %08x\n",
                   rt, rs, CURRENT_STATE.REGS[rs], extend_sign_16(imm), rt, CURRENT_STATE.REGS[rt], NEXT_STATE.REGS[rt]);
        }
        break;
    }
    case ADDIU:
    {
        printf("ADDIU rt=$%d, rs=$%d, imm=%08x\n", rt, rs, imm);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) + (sign extend imm: %08x)): $%d changes from %08x to %08x\n",
                   rt, rs, CURRENT_STATE.REGS[rs], extend_sign_16(imm), rt, CURRENT_STATE.REGS[rt], NEXT_STATE.REGS[rt]);
        }
        break;
    }
    case ANDI:
    {
        printf("ANDI rt=$%d, rs=$%d, imm=%08x\n", rt, rs, imm);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) & (zero extend imm: %08x)): $%d changes from %08x to %08x\n",
                   rt, rs, CURRENT_STATE.REGS[rs], imm, rt, CURRENT_STATE.REGS[rt], NEXT_STATE.REGS[rt]);
        }
        break;
    }
    case ORI:
    {
        printf("ORI rt=$%d, rs=$%d, imm=%08x\n", rt, rs, imm);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) | (zero extend imm: %08x)): $%d changes from %08x to %08x\n",
                   rt, rs, CURRENT_STATE.REGS[rs], imm, rt, CURRENT_STATE.REGS[rt], NEXT_STATE.REGS[rt]);
        }
        break;
    }
    case XORI:
    {
        printf("XORI rt=$%d, rs=$%d, imm=%08x\n", rt, rs, imm);
        if (verbose)
        {
            printf("\t$%d <- (($%d: %08x) ^ (zero extend imm: %08x)): $%d changes from %08x to %08x\n",
                   rt, rs, CURRENT_STATE.REGS[rs], imm, rt, CURRENT_STATE.REGS[rt], NEXT_STATE.REGS[rt]);
        }
        break;
    }
    case SLTI:
    {
        printf("SLTI rt=$%d, rs=$%d, imm=%08x\n", rt, rs, imm);
        if (verbose)
        {
            printf("\t$%d <- ((($%d: %08x) < (sign extend imm: %08x)) (sign compare) ? (1) : (0)): $%d changes from %08x to %08x\n",
                   rt, rs, CURRENT_STATE.REGS[rs], extend_sign_16(imm), rt, CURRENT_STATE.REGS[rt], NEXT_STATE.REGS[rt]);
        }
        break;
    }
    case SLTIU:
    {
        printf("SLTIU rt=$%d, rs=$%d, imm=%08x\n", rt, rs, imm);
        if (verbose)
        {
            printf("\t$%d <- ((($%d: %08x) < (sign extend imm: %08x)) (unsg compare) ? (1) : (0)): $%d changes from %08x to %08x\n",
                   rt, rs, CURRENT_STATE.REGS[rs], extend_sign_16(imm), rt, CURRENT_STATE.REGS[rt], NEXT_STATE.REGS[rt]);
        }
        break;
    }
    case LUI:
    {
        printf("LUI rt=$%d, imm=%08x\n", rt, imm);
        if (verbose)
        {
            printf("\t$%d <- ((imm: %08x) << 16): $%d changes from %08x to %08x\n",
                   rt, imm, rt, CURRENT_STATE.REGS[rt], NEXT_STATE.REGS[rt]);
        }
        break;
    }
    }
}
// general
void explain_instruction(uint32_t ins_address, uint32_t err, uint32_t verbose = 0)
{
    if (err == UnknownInstruction)
        return;
    if (ins_address != CURRENT_STATE.PC || err != NoError)
        verbose = 0;
    uint32_t ins = mem_read_32(ins_address);
    uint32_t op = get_op(ins);
    uint32_t rs = get_rs(ins), rt = get_rt(ins), rd = get_rd(ins);
    uint32_t shamt = get_shamt(ins), funct = get_funct(ins);
    uint32_t imm = get_immediate(ins), targt = get_target(ins);
    if (op == 000)
    {
        // R type
        switch (funct & 070)
        {
        case 000:
            explain_R_Shift(funct, rs, rt, rd, shamt, verbose);
            break;
        case 010:
        {
            if (funct == SYSCALL)
                explain_R_SYSCALL(funct, verbose);
            else
                explain_R_Jump(funct, rs, rd, verbose);
            break;
        }
        case 020:
            explain_R_HILO(funct, rs, rd, verbose);
            break;
        case 030:
            explain_R_MulDiv(funct, rs, rt, verbose);
            break;
        case 040:
        case 050:
            explain_R_ALC(funct, rs, rt, rd, verbose);
            break;
        }
    }
    else
    {
        switch (op & 070)
        {
        case 000:
        {
            if (op == J || op == JAL)
                explain_J_Jump(op, targt, verbose);
            else
                explain_I_Branch(op, rs, rt, imm, verbose);
            break;
        }
        case 010:
            explain_I_ALC(op, rs, rt, imm, verbose);
            break;
        case 040:
            explain_I_Load(op, rs, rt, imm, verbose);
            break;
        case 050:
            explain_I_Store(op, rs, rt, imm, verbose);
            break;
        }
    }
}