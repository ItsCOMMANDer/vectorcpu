#include "libvirtcore.h"

#include <stdint.h>

struct instruction {
    uint16_t checkMask;
    uint16_t opcodeMask;
    void (*instruction)(struct vcpu_core*, uint16_t);
};

#define INS_ADDR(x) (x & 0b0000111111111111)
#define INS_PID(x) ((x >> 2) & 0b111)
#define INS_IMM8(x) (x & 0xff)
#define INS_DST(x) ((x >> 8) & 0b111)
#define INS_SRC1(x) ((x >> 5) & 0b111)
#define INS_SRC2(x) ((x >> 2) & 0b111)
#define INS_SRC1ALT1(x) ((x >> 8) & 0b111)
#define INS_SRC1ALT2(x) ((x >> 11) & 0b111)

#define IMM8_MSB(x) ((x >> 7) & 1)

void instruction_nop(struct vcpu_core* cpu, uint16_t instruction) {return;}
void instruction_halt(struct vcpu_core* cpu, uint16_t instruction) {cpu->halted = true;}
void instruction_ret(struct vcpu_core* cpu, uint16_t instruction) {
    uint8_t pc_hi = cpu->readMem(cpu->ss_sp--);
    uint8_t pc_lo = cpu->readMem(cpu->ss_sp--);
    uint16_t new_pc = (pc_hi << 8 | pc_lo) & 0b111111111111;
    cpu->pc = new_pc;
}

void instruction_enzr(struct vcpu_core* cpu, uint16_t instruction) {cpu->zren = true;}
void instruction_dszr(struct vcpu_core* cpu, uint16_t instruction) {cpu->zren = false;}

void instruction_jmp(struct vcpu_core* cpu, uint16_t instruction) {cpu->pc = INS_ADDR(instruction);}
void instruction_call(struct vcpu_core* cpu, uint16_t instruction) {
    uint16_t old_pc = cpu->pc;
    uint8_t pc_lo = old_pc & 0xff;
    uint8_t pc_hi = (old_pc >> 8) && 0b1111;
    cpu->pc = INS_ADDR(instruction);
    cpu->writeMem(++cpu->ss_sp, pc_lo);
    cpu->writeMem(++cpu->ss_sp, pc_hi);
}

void instruction_bltu(struct vcpu_core* cpu, uint16_t instruction) {if(cpu->ltu) {cpu->pc = INS_ADDR(instruction );}}
void instruction_bleu(struct vcpu_core* cpu, uint16_t instruction) {if(cpu->ltu | cpu->zf) {cpu->pc = INS_ADDR(instruction );}}

void instruction_blts(struct vcpu_core* cpu, uint16_t instruction) {if(cpu->lts) {cpu->pc = INS_ADDR(instruction );}}
void instruction_bles(struct vcpu_core* cpu, uint16_t instruction) {if(cpu->lts | cpu->zf) {cpu->pc = INS_ADDR(instruction );}}

void instruction_beq(struct vcpu_core* cpu, uint16_t instruction) {if(cpu->zf) {cpu->pc = INS_ADDR(instruction );}}
void instruction_bne(struct vcpu_core* cpu, uint16_t instruction) {if(!cpu->zf) {cpu->pc = INS_ADDR(instruction );}}

void instruction_ldi(struct vcpu_core* cpu, uint16_t instruction) {cpu->gpr[INS_DST(instruction)] = INS_IMM8(instruction);}


void instruction_wrss(struct vcpu_core* cpu, uint16_t instruction) {cpu->ss_sp |= (cpu->gpr[INS_SRC1(instruction)] & 0b1111) << 8;}
void instruction_wrds(struct vcpu_core* cpu, uint16_t instruction) {cpu->ds = cpu->gpr[INS_SRC1(instruction)] & 0b1111;}
void instruction_wrsp(struct vcpu_core* cpu, uint16_t instruction) {cpu->ss_sp |= cpu->gpr[INS_SRC1(instruction)];}

void instruction_rdss(struct vcpu_core* cpu, uint16_t instruction) {cpu->gpr[INS_DST(instruction)] = cpu->ss_sp >> 8;}
void instruction_rdds(struct vcpu_core* cpu, uint16_t instruction) {cpu->gpr[INS_DST(instruction)] = cpu->ds;}
void instruction_rdsp(struct vcpu_core* cpu, uint16_t instruction) {cpu->gpr[INS_DST(instruction)] = cpu->ss_sp & 0xff;}

void instruction_mov(struct vcpu_core* cpu, uint16_t instruction) {cpu->gpr[INS_DST(instruction)] = cpu->gpr[INS_SRC1(instruction)];}
void instruction_not(struct vcpu_core* cpu, uint16_t instruction) {cpu->gpr[INS_DST(instruction)] = ~cpu->gpr[INS_SRC1(instruction)];}
void instruction_ldr(struct vcpu_core* cpu, uint16_t instruction) {cpu->gpr[INS_DST(instruction)] = cpu->readMem((cpu->ds << 8) | cpu->gpr[INS_SRC1(instruction)]);}
void instruction_str(struct vcpu_core* cpu, uint16_t instruction) {cpu->writeMem((cpu->ds << 8) | cpu->gpr[INS_DST(instruction)], cpu->gpr[INS_DST(instruction)]);}

void instruction_add(struct vcpu_core* cpu, uint16_t instruction) {
    uint8_t a = INS_SRC1(instruction) & 0xff;
    uint8_t b = INS_SRC2(instruction) & 0xff;

    uint16_t result = (uint16_t)b + (uint16_t)a;

    cpu->gpr[INS_DST(instruction)] = (uint8_t)(result & 0xff);

    cpu->cf = (result >> 8) & 1 == 1;
    cpu->zf = result & 0xff == 0;

    if(IMM8_MSB(a) & IMM8_MSB(b)) cpu->ltu = false;
    if(IMM8_MSB(a) ^ IMM8_MSB(b) == 1) cpu->ltu = IMM8_MSB(result);
    if(!(IMM8_MSB(a) | IMM8_MSB(b))) cpu->ltu = true;

    if(IMM8_MSB(a) & IMM8_MSB(b)) cpu->lts = true;
    if(IMM8_MSB(a) ^ IMM8_MSB(b) == 1) cpu->lts = IMM8_MSB(result);
    if(!(IMM8_MSB(a) | IMM8_MSB(b))) cpu->lts = false;
}

void instruction_sub(struct vcpu_core* cpu, uint16_t instruction) {
    uint8_t a = INS_SRC1(instruction) & 0xff;
    uint8_t b = INS_SRC2(instruction) & 0xff;

    uint16_t result = (uint16_t)b + (uint16_t)(~a + 1);

    cpu->gpr[INS_DST(instruction)] = (uint8_t)(result & 0xff);

    cpu->cf = (result >> 8) & 1 == 1;
    cpu->zf = result & 0xff == 0;

    if(IMM8_MSB(a) & IMM8_MSB(b)) cpu->ltu = false;
    if(IMM8_MSB(a) ^ IMM8_MSB(b) == 1) cpu->ltu = IMM8_MSB(result);
    if(!(IMM8_MSB(a) | IMM8_MSB(b))) cpu->ltu = true;

    if(IMM8_MSB(a) & IMM8_MSB(b)) cpu->lts = true;
    if(IMM8_MSB(a) ^ IMM8_MSB(b) == 1) cpu->lts = IMM8_MSB(result);
    if(!(IMM8_MSB(a) | IMM8_MSB(b))) cpu->lts = false;
}

void instruction_adc(struct vcpu_core* cpu, uint16_t instruction) {
    uint8_t a = INS_SRC1(instruction) & 0xff;
    uint8_t b = INS_SRC2(instruction) & 0xff;

    uint16_t result = (uint16_t)b + (uint16_t)a + (cpu->cf == true ? 1 : 0);

    cpu->gpr[INS_DST(instruction)] = (uint8_t)(result & 0xff);

    cpu->cf = (result >> 8) & 1 == 1;
    cpu->zf = result & 0xff == 0;

    if(IMM8_MSB(a) & IMM8_MSB(b)) cpu->ltu = false;
    if(IMM8_MSB(a) ^ IMM8_MSB(b) == 1) cpu->ltu = IMM8_MSB(result);
    if(!(IMM8_MSB(a) | IMM8_MSB(b))) cpu->ltu = true;

    if(IMM8_MSB(a) & IMM8_MSB(b)) cpu->lts = true;
    if(IMM8_MSB(a) ^ IMM8_MSB(b) == 1) cpu->lts = IMM8_MSB(result);
    if(!(IMM8_MSB(a) | IMM8_MSB(b))) cpu->lts = false;
}

void instruction_sbc(struct vcpu_core* cpu, uint16_t instruction) {
    uint8_t a = INS_SRC1(instruction) & 0xff;
    uint8_t b = INS_SRC2(instruction) & 0xff;

    uint16_t result = (uint16_t)b + (uint16_t)(~a + 1) + (cpu->cf == true ? 1 : 0);

    cpu->gpr[INS_DST(instruction)] = (uint8_t)(result & 0xff);

    cpu->cf = (result >> 8) & 1 == 1;
    cpu->zf = result & 0xff == 0;

    if(IMM8_MSB(a) & IMM8_MSB(b)) cpu->ltu = false;
    if(IMM8_MSB(a) ^ IMM8_MSB(b) == 1) cpu->ltu = IMM8_MSB(result);
    if(!(IMM8_MSB(a) | IMM8_MSB(b))) cpu->ltu = true;

    if(IMM8_MSB(a) & IMM8_MSB(b)) cpu->lts = true;
    if(IMM8_MSB(a) ^ IMM8_MSB(b) == 1) cpu->lts = IMM8_MSB(result);
    if(!(IMM8_MSB(a) | IMM8_MSB(b))) cpu->lts = false;
}

void instruction_and(struct vcpu_core* cpu, uint16_t instruction) {cpu->gpr[INS_DST(instruction)] = cpu->gpr[INS_SRC1(instruction)] & cpu->gpr[INS_SRC2(instruction)];}
void instruction_or(struct vcpu_core* cpu, uint16_t instruction) {cpu->gpr[INS_DST(instruction)] = cpu->gpr[INS_SRC1(instruction)] | cpu->gpr[INS_SRC2(instruction)];}
void instruction_xor(struct vcpu_core* cpu, uint16_t instruction) {cpu->gpr[INS_DST(instruction)] = cpu->gpr[INS_SRC1(instruction)] ^ cpu->gpr[INS_SRC2(instruction)];}

void instruction_in(struct vcpu_core* cpu, uint16_t instruction) {cpu->gpr[INS_DST(instruction)] = cpu->readPort(INS_PID(instruction));}
void instruction_out(struct vcpu_core* cpu, uint16_t instruction) {cpu->writePort(INS_PID(instruction), cpu->gpr[INS_SRC1(instruction)]);}

void instruction_push(struct vcpu_core* cpu, uint16_t instruction) {cpu->writeMem(++cpu->ss_sp, cpu->gpr[INS_SRC1(instruction)]); cpu->ss_sp &= 0xfff;}
void instruction_pop(struct vcpu_core* cpu, uint16_t instruction) {cpu->gpr[INS_DST(instruction)] = cpu->readMem(cpu->ss_sp--); cpu->ss_sp &= 0xfff;}

void instruction_srldr(struct vcpu_core* cpu, uint16_t instruction) {cpu->gpr[INS_DST(instruction)] = cpu->readMem(((int16_t)(cpu->ss_sp) + (int16_t)INS_IMM8(instruction) & 0xfff));}
void instruction_srstr(struct vcpu_core* cpu, uint16_t instruction) {cpu->writeMem(((int16_t)(cpu->ss_sp) + (int16_t)INS_IMM8(instruction)) & 0xfff, cpu->gpr[INS_SRC1ALT1(instruction)]);}

void instruction_addi(struct vcpu_core* cpu, uint16_t instruction) {
    uint8_t a = INS_IMM8(instruction) & 0xff;
    uint8_t b = INS_SRC1ALT2(instruction) & 0xff;

    uint16_t result = (uint16_t)a + (uint16_t)b;

    cpu->gpr[INS_DST(instruction)] = (uint8_t)(result & 0xff);

    cpu->cf = (result >> 8) & 1 == 1;
    cpu->zf = result & 0xff == 0;

    if(IMM8_MSB(a) & IMM8_MSB(b)) cpu->ltu = false;
    if(IMM8_MSB(a) ^ IMM8_MSB(b) == 1) cpu->ltu = IMM8_MSB(result);
    if(!(IMM8_MSB(a) | IMM8_MSB(b))) cpu->ltu = true;

    if(IMM8_MSB(a) & IMM8_MSB(b)) cpu->lts = true;
    if(IMM8_MSB(a) ^ IMM8_MSB(b) == 1) cpu->lts = IMM8_MSB(result);
    if(!(IMM8_MSB(a) | IMM8_MSB(b))) cpu->lts = false;
}

/*
ULT: 
2i = 0
1i = N
0i = 1

SLT:
2i = 1
i1 = N
0i = 0
*/

const struct instruction isa[]= {
    {0b1111000000000000, 0b0000000000000000, &instruction_jmp},
    {0b1111000000000000, 0b0001000000000000, &instruction_call},
    {0b1111000000000000, 0b0010000000000000, &instruction_bltu},
    {0b1111000000000000, 0b0011000000000000, &instruction_bleu},
    {0b1111000000000000, 0b0100000000000000, &instruction_blts},
    {0b1111000000000000, 0b0101000000000000, &instruction_bles},
    {0b1111000000000000, 0b0110000000000000, &instruction_beq},
    {0b1111000000000000, 0b0111000000000000, &instruction_bne},
    {0b1111111111111111, 0b1001000000000011, &instruction_nop},
    {0b1111111111111111, 0b1001000000000111, &instruction_halt},
    {0b1111111111111111, 0b1001000000001011, &instruction_ret},
    {0b1111111111111111, 0b1001000000001111, &instruction_enzr},
    {0b1111111111111111, 0b1001000000010011, &instruction_dszr},
    {0b1111111100011111, 0b1001000000000000, &instruction_push},
    {0b1111111100011111, 0b1001000000000100, &instruction_wrss},
    {0b1111111100011111, 0b1001000000001000, &instruction_wrds},
    {0b1111111100011111, 0b1001000000001100, &instruction_wrsp},
    {0b1111100011111111, 0b1001000000000001, &instruction_pop},
    {0b1111100011111111, 0b1001000000000101, &instruction_rdss},
    {0b1111100011111111, 0b1001000000001001, &instruction_rdds},
    {0b1111100011111111, 0b1001000000001101, &instruction_rdsp},
    {0b1111111100000011, 0b1001100000000000, &instruction_out},
    {0b1111100011100011, 0b1001100000000001, &instruction_in},
    {0b1111100000011111, 0b1001000000000010, &instruction_mov},
    {0b1111100000011111, 0b1001000000000110, &instruction_not},
    {0b1111100000011111, 0b1001000000001010, &instruction_ldr},
    {0b1111100000011111, 0b1001000000001110, &instruction_str},
    {0b1111100000000011, 0b1000000000000000, &instruction_add},
    {0b1111100000000011, 0b1000000000000001, &instruction_sub},
    {0b1111100000000011, 0b1000000000000010, &instruction_adc},
    {0b1111100000000011, 0b1000000000000011, &instruction_sbc},
    {0b1111100000000011, 0b1000100000000000, &instruction_and},
    {0b1111100000000011, 0b1000100000000001, &instruction_or},
    {0b1111100000000011, 0b1000100000000010, &instruction_xor},
    {0b1111100000000000, 0b1010000000000000, &instruction_ldi},
    {0b1111100000000000, 0b1010100000000000, &instruction_srstr},
    {0b1111100000000000, 0b1011000000000000, &instruction_srldr},
    {0b1100000000000000, 0b1100000000000000, &instruction_addi},
};

void vcore_step(struct vcpu_core* cpu) {
    if(cpu->halted) return;
    uint16_t instruction = 0;
    instruction = cpu->readMem(cpu->pc++);
    instruction |= cpu->readMem(cpu->pc++) << 8;
}