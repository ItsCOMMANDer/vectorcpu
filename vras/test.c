#include <stdio.h>
#include <stdint.h>

#define MKC_ADDR(x) (x & 0b111111111111)
#define MKC_PID(x) ((x & 0b111) << 2)
#define MKC_IMM8(x) (x & 0xff)
#define MKC_DST(x) ((x & 0b111) << 8)
#define MKC_SRC1(x) ((x & 0b111) << 5)
#define MKC_SRC2(x) ((x & 0b111) << 2)
#define MKC_SRC1ALT1(x) ((x & 0b111) << 8)
#define MKC_SRC1ALT2(x) ((x & 0b111) << 11)

#define MKI_JMP(addr)  (0b0000000000000000 | MKC_ADDR(addr))
#define MKI_CALL(addr) (0b0001000000000000 | MKC_ADDR(addr))
#define MKI_BLTU(addr) (0b0010000000000000 | MKC_ADDR(addr))
#define MKI_BLEU(addr) (0b0011000000000000 | MKC_ADDR(addr))
#define MKI_BLTS(addr) (0b0100000000000000 | MKC_ADDR(addr))
#define MKI_BLES(addr) (0b0101000000000000 | MKC_ADDR(addr))
#define MKI_BEQ(addr)  (0b0110000000000000 | MKC_ADDR(addr))
#define MKI_BNE(addr)	(0b0111000000000000 | MKC_ADDR(addr))

#define MKI_NOP()	(0b1001000000000011)
#define MKI_HALT()	(0b1001000000000111)
#define MKI_RET()	(0b1001000000001011)
#define MKI_ENZR()	(0b1001000000001111)
#define MKI_DSZR()	(0b1001000000010011)

#define MKI_PUSH(src)	(0b1001000000000000 | MKC_SRC1(src))
#define MKI_WRSS(src)	(0b1001000000000100 | MKC_SRC1(src))
#define MKI_WRDS(src)	(0b1001000000001000 | MKC_SRC1(src))
#define MKI_WRSP(src)	(0b1001000000001100 | MKC_SRC1(src))

#define MKI_POP(dst)	(0b1001000000000001 | MKC_DST(dst))
#define MKI_RDSS(dst)	(0b1001000000000101 | MKC_DST(dst))
#define MKI_RDDS(dst)	(0b1001000000001001 | MKC_DST(dst))
#define MKI_RDSP(dst)	(0b1001000000001101 | MKC_DST(dst))

#define MKI_OUT(pid, src)	(0b1001100000000000 | MKC_SRC1(src) | MKC_PID(pid))
#define MKI_IN(dst, pid)	(0b1001100000000001 | MKC_DST(dst) | MKC_PID(pid))

#define MKI_MOV(dst, src)	(0b1001000000000010 | MKC_DST(dst) | MKC_SRC1(src))
#define MKI_NOT(dst, src)	(0b1001000000000110 | MKC_DST(dst) | MKC_SRC1(src))
#define MKI_LDR(dst, src)	(0b1001000000001010 | MKC_DST(dst) | MKC_SRC1(src))
#define MKI_STR(dst, src)	(0b1001000000001110 | MKC_DST(dst) | MKC_SRC1(src))

#define MKI_ADD(dst, src1, src2)	(0b1000000000000000 | MKC_DST(dst) | MKC_SRC1(src1) | MKC_SRC2(src2))
#define MKI_SUB(dst, src1, src2)	(0b1000000000000001 | MKC_DST(dst) | MKC_SRC1(src1) | MKC_SRC2(src2))
#define MKI_ADC(dst, src1, src2)	(0b1000000000000010 | MKC_DST(dst) | MKC_SRC1(src1) | MKC_SRC2(src2))
#define MKI_SBC(dst, src1, src2)	(0b1000000000000011 | MKC_DST(dst) | MKC_SRC1(src1) | MKC_SRC2(src2))
#define MKI_AND(dst, src1, src2)	(0b1000100000000000 | MKC_DST(dst) | MKC_SRC1(src1) | MKC_SRC2(src2))
#define MKI_OR(dst, src1, src2)		(0b1000100000000001 | MKC_DST(dst) | MKC_SRC1(src1) | MKC_SRC2(src2))
#define MKI_XOR(dst, src1, src2)	(0b1000100000000010 | MKC_DST(dst) | MKC_SRC1(src1) | MKC_SRC2(src2))

#define MKI_LDI(dst, imm8)		(0b1010000000000000 | MKC_DST(dst) | MKC_IMM8(imm8))
#define MKI_SRLDR(dst, imm8)	(0b1010100000000000 | MKC_DST(dst) | MKC_IMM8(imm8))

#define MKI_SRSTR(src, imm8)		(0b1011000000000000 | MKC_SRC1ALT1(src) | MKC_IMM8(imm8))
#define MKI_ADDI(dst, src, imm8)	(0b1100000000000000 | MKC_DST(dst) | MKC_SRC1ALT2(src) | MKC_IMM8(imm8))

#define REG_R0 0
#define REG_R1 1
#define REG_R2 2
#define REG_R3 3
#define REG_R4 4
#define REG_R5 5
#define REG_R6 6
#define REG_R7 7

uint16_t fib[] = {
/* 00 */ MKI_LDI(REG_R1, 0xff),
/* 02 */ MKI_WRSS(REG_R1),
/* 04 */ MKI_WRSP(REG_R1),
/* 06 */ MKI_LDI(REG_R1, 13),
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

uint16_t prog[] = {
    MKI_POP(REG_R1),
};

int main() {
    size_t len = sizeof(fib) / sizeof(fib[0]);

    FILE *f = fopen("fib.bin", "wb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    fwrite(fib, sizeof(uint16_t), len, f);

    fclose(f);
    return 0;
}