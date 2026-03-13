#include "virtcore.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <math.h>

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


#define FIB 16
#define ZC 48
#define OC 54

uint16_t program[] = {
/* 00 */    MKI_LDI(REG_R1, 0xff),
/* 02 */    MKI_WRSS(REG_R1),
/* 04 */    MKI_WRSP(REG_R1),
/* 06 */    MKI_LDI(REG_R1, 0x0f),
/* 08 */    MKI_WRDS(REG_R1),
/* 10 */    MKI_LDI(REG_R0, 30),
/* 12 */    MKI_CALL(FIB),
/* 14 */    MKI_HALT(),
/* 16 FIB*/ MKI_ADDI(REG_R0, REG_R0, 0),
/* 18 */    MKI_BEQ(ZC),
/* 20 */    MKI_ADDI(REG_R0, REG_R0, 0xff),
/* 22 */    MKI_BEQ(OC),
/* 24 */    MKI_PUSH(REG_R0),
/* 26 */    MKI_CALL(FIB),
/* 28 */    MKI_POP(REG_R0),
/* 30 */    MKI_PUSH(REG_R1),
/* 32 */    MKI_PUSH(REG_R2),
/* 34 */    MKI_ADDI(REG_R0, REG_R0, 0xff),
/* 36 */    MKI_CALL(FIB),
/* 38 */    MKI_POP(REG_R4), 
/* 40 */    MKI_POP(REG_R3),
/* 42 */    MKI_ADD(REG_R1, REG_R1, REG_R3),
/* 44 */    MKI_ADC(REG_R2, REG_R2, REG_R4),
/* 46 */    MKI_RET(),
/* 48 ZC*/  MKI_LDI(REG_R1, 0),
/* 50 */    MKI_LDI(REG_R2, 0),
/* 52 */    MKI_RET(),
/* 54 OC*/  MKI_LDI(REG_R1, 1),
/* 56 */    MKI_LDI(REG_R2, 0),
/* 58 */    MKI_RET(),
};

uint16_t fib[] = {
/* 00 */ MKI_LDI(REG_R1, 0xff),
/* 02 */ MKI_WRSS(REG_R1),
/* 04 */ MKI_WRSP(REG_R1),
/* 06 */ MKI_LDI(REG_R1, 20),
/* 08 */ MKI_CALL(12),
/* 10 */ MKI_HALT(),
/* 12 fib: */ MKI_ADDI(REG_R2, REG_R1, 0),
/* 14 */ MKI_BEQ(38),
/* 16 */ MKI_ADDI(REG_R1, REG_R1, 0xff),
/* 18 */ MKI_BEQ(42),
/* 20 */ MKI_PUSH(REG_R1),
/* 22 */ MKI_CALL(12),
/* 24 */ MKI_POP(REG_R1),
/* 26 */ MKI_PUSH(REG_R0),
/* 28 */ MKI_ADDI(REG_R1, REG_R1, 0xff),
/* 30 */ MKI_CALL(12),
/* 32 */ MKI_POP(REG_R1),
/* 34 */ MKI_ADD(REG_R0, REG_R0, REG_R1),
/* 36 */ MKI_RET(),
/* 38 zc: */ MKI_LDI(REG_R0, 0),
/* 40 */ MKI_RET(),
/* 42 oc: */ MKI_LDI(REG_R0, 1),
/* 44 */ MKI_RET()
};

#define MAX(a,b) (((a)>(b))?(a):(b))

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

    while(!core.halted) {
        //getchar();
        vcore_step(&core);
        //vcore_dump(&core);
        //printf("\n\n");
    }

    vcore_dump(&core);

    return 0;
}