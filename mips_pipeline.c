#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define MEM_SIZE 1024

uint32_t REG[32];
uint32_t INST_MEM[MEM_SIZE];
uint32_t DATA_MEM[MEM_SIZE];
uint32_t PC = 0;

typedef struct { uint32_t instr, pc; } IF_ID;
typedef struct {
    uint32_t pc, rs, rt, rd, imm;
    uint32_t regA, regB;
    uint32_t opcode, funct, shamt;
} ID_EX;
typedef struct {
    uint32_t aluResult, regB, destReg;
    uint32_t memRead, memWrite, regWrite, memToReg;
} EX_MEM;
typedef struct {
    uint32_t memData, aluResult, destReg;
    uint32_t regWrite, memToReg;
} MEM_WB;

IF_ID if_id;
ID_EX id_ex;
EX_MEM ex_mem;
MEM_WB mem_wb;

void load_file(const char *name, uint32_t *array) {
    FILE *f = fopen(name, "r");
    if (!f) { printf("Error abriendo %s\n", name); exit(1); }
    int i = 0;
    while (fscanf(f, "%x", &array[i]) != EOF) i++;
    fclose(f);
}

void load_regs(const char *name) {
    FILE *f = fopen(name, "r");
    if (!f) { printf("Error abriendo reg.txt\n"); exit(1); }
    for (int i = 0; i < 32; i++) fscanf(f, "%u", &REG[i]);
    fclose(f);
}

void save_regs() {
    FILE *f = fopen("reg.txt", "w");
    for (int i = 0; i < 32; i++) fprintf(f, "%u\n", REG[i]);
    fclose(f);
}

void save_data() {
    FILE *f = fopen("data_mem.txt", "w");
    for (int i = 0; i < 32; i++) fprintf(f, "%u\n", DATA_MEM[i]);
    fclose(f);
}

void WB() {
    if (mem_wb.regWrite && mem_wb.destReg != 0) {
        REG[mem_wb.destReg] =
            mem_wb.memToReg ? mem_wb.memData : mem_wb.aluResult;
    }
}

void MEM() {
    mem_wb.regWrite = ex_mem.regWrite;
    mem_wb.memToReg = ex_mem.memToReg;
    mem_wb.destReg  = ex_mem.destReg;
    mem_wb.aluResult = ex_mem.aluResult;

    if (ex_mem.memRead)
        mem_wb.memData = DATA_MEM[ex_mem.aluResult / 4];

    if (ex_mem.memWrite)
        DATA_MEM[ex_mem.aluResult / 4] = ex_mem.regB;
}

void EX() {
    ex_mem.memRead = ex_mem.memWrite = ex_mem.regWrite = ex_mem.memToReg = 0;

    switch (id_ex.opcode) {
        case 0x00: // R-type
            ex_mem.regWrite = 1;
            ex_mem.destReg = id_ex.rd;
            switch (id_ex.funct) {
                case 0x20: ex_mem.aluResult = id_ex.regA + id_ex.regB; break;
                case 0x22: ex_mem.aluResult = id_ex.regA - id_ex.regB; break;
                case 0x24: ex_mem.aluResult = id_ex.regA & id_ex.regB; break;
                case 0x25: ex_mem.aluResult = id_ex.regA | id_ex.regB; break;
                case 0x27: ex_mem.aluResult = ~(id_ex.regA | id_ex.regB); break;
                case 0x26: ex_mem.aluResult = id_ex.regA ^ id_ex.regB; break;
                case 0x2A: ex_mem.aluResult = (id_ex.regA < id_ex.regB); break;
                case 0x00: ex_mem.aluResult = id_ex.regB << id_ex.shamt; break;
            }
            break;

        case 0x08: // addi
            ex_mem.regWrite = 1;
            ex_mem.destReg = id_ex.rt;
            ex_mem.aluResult = id_ex.regA + id_ex.imm;
            break;

        case 0x23: // lw
            ex_mem.memRead = 1;
            ex_mem.regWrite = 1;
            ex_mem.memToReg = 1;
            ex_mem.destReg = id_ex.rt;
            ex_mem.aluResult = id_ex.regA + id_ex.imm;
            break;

        case 0x2B: // sw
            ex_mem.memWrite = 1;
            ex_mem.aluResult = id_ex.regA + id_ex.imm;
            ex_mem.regB = id_ex.regB;
            break;

        case 0x04: // beq
            if (id_ex.regA == id_ex.regB)
                PC = id_ex.pc + (id_ex.imm << 2);
            break;

        case 0x05: // bne
            if (id_ex.regA != id_ex.regB)
                PC = id_ex.pc + (id_ex.imm << 2);
            break;

        case 0x02: // j
            PC = (id_ex.imm << 2);
            break;
    }
}

void ID() {
    uint32_t instr = if_id.instr;
    id_ex.pc = if_id.pc;

    id_ex.opcode = (instr >> 26) & 0x3F;
    id_ex.rs = (instr >> 21) & 0x1F;
    id_ex.rt = (instr >> 16) & 0x1F;
    id_ex.rd = (instr >> 11) & 0x1F;
    id_ex.shamt = (instr >> 6) & 0x1F;
    id_ex.funct = instr & 0x3F;
    id_ex.imm = (int16_t)(instr & 0xFFFF);

    id_ex.regA = REG[id_ex.rs];
    id_ex.regB = REG[id_ex.rt];
}

void IF() {
    if_id.pc = PC;
    if_id.instr = INST_MEM[PC / 4];
    PC += 4;
}

void print_cycle(int c) {
    printf("\n=========== CICLO %d ===========\n", c);
    printf("IF  : PC=%u  INSTR=0x%08X\n", if_id.pc, if_id.instr);
    printf("ID  : PC=%u  OPCODE=0x%02X\n", id_ex.pc, id_ex.opcode);
    printf("EX  : ALU=%u  DEST=%u\n", ex_mem.aluResult, ex_mem.destReg);
    printf("MEM : ALU=%u  DEST=%u\n", mem_wb.aluResult, mem_wb.destReg);
    if (mem_wb.regWrite)
        printf("WB  : WRITE R%u = %u\n",
               mem_wb.destReg,
               mem_wb.memToReg ? mem_wb.memData : mem_wb.aluResult);
    else
        printf("WB  : ---\n");
}

int main() {
    load_file("inst_mem.txt", INST_MEM);
    load_file("data_mem.txt", DATA_MEM);
    load_regs("reg.txt");

    for (int cycle = 1; cycle <= 20; cycle++) {
        WB();
        MEM();
        EX();
        ID();
        IF();
        print_cycle(cycle);
    }

    save_regs();
    save_data();

    printf("\nSimulacion terminada\n");
    return 0;
}
