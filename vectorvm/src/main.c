#include "../libvirtcore/libvirtcore.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <string.h>

uint8_t *ram;

uint8_t stub_implReadPort(uint8_t pid __attribute__((unused))) {return 0;}
void stub_implWritePort(uint8_t pid __attribute__((unused)), uint8_t val __attribute__((unused))) {return;}

uint8_t implReadMem(uint16_t addr) {
    return ram[addr & 0b111111111111];
}

void implWriteMem(uint16_t addr, uint8_t val) {
    ram[addr & 0b111111111111] = val;
}

uint16_t program[] = {
    MKI_LDI(REG_R0, 42),
    MKI_ENZR(),
    MKI_MOV(REG_R1, REG_R0),
    MKI_DSZR(),
    MKI_MOV(REG_R2, REG_R0),
    MKI_HALT(),
};

uint16_t fib[] = {
/* 00 */ MKI_LDI(REG_R1, 13),
/* 02 */ MKI_CALL(6),
/* 04 */ MKI_HALT(),
/* 06 fib: */ MKI_ADDI(REG_R2, REG_R1, 0),
/* 08 */ MKI_BEQ(32),
/* 10 */ MKI_ADDI(REG_R1, REG_R1, 0xff),
/* 12 */ MKI_BEQ(36),
/* 14 */ MKI_PUSH(REG_R1),
/* 16 */ MKI_CALL(6),
/* 18 */ MKI_POP(REG_R1),
/* 20 */ MKI_PUSH(REG_R0),
/* 22 */ MKI_ADDI(REG_R1, REG_R1, 0xff),
/* 24 */ MKI_CALL(6),
/* 26 */ MKI_POP(REG_R1),
/* 28 */ MKI_ADD(REG_R0, REG_R0, REG_R1),
/* 30 */ MKI_RET(),
/* 32 zc: */ MKI_LDI(REG_R0, 0),
/* 34 */ MKI_RET(),
/* 36 oc: */ MKI_LDI(REG_R0, 1),
/* 38 */ MKI_RET()
};


int main(int argc, char** argv) {
    printf("VectorVM  v0.0.0\n");
    struct vcpu_core core;

    core.readPort = &stub_implReadPort;
    core.writePort = &stub_implWritePort;

    core.readMem = &implReadMem;
    core.writeMem = &implWriteMem;

    for(int i = 0; i < 8; i++) {core.gpr[i] = 0;}

    core.pc = 0;
    core.ds = 0;
    core.ss_sp = 0;
    
    core.zren = false;
    core.halted = false;
    
    core.zf = false;
    core.cf = false;
    core.lts = false;
    core.ltu = false;

    ram = malloc(4096);

    memcpy(ram, &program, sizeof(program));

    //vcore_dump(&core);
 
    while(!core.halted) {
        //getchar();
        vcore_step(&core);
        //vcore_dump(&core);
        //printf("\n\n");
    }

    vcore_dump(&core);

    return 0;
}