#ifndef LIB_VIRT_CORE_H
#define LIB_VIRT_CORE_H

#include <stdint.h>
#include <stdbool.h>

struct vcpu_core {
	uint8_t gpr[8];
	uint16_t pc;
	uint8_t ds;
	uint16_t ss_sp;

	bool zren;
	bool halted;

	bool zf;
	bool cf;
	bool lts;
	bool ltu;


	void (*writeMem)(uint16_t address, uint8_t data);
	uint8_t (*readMem)(uint16_t address);

	void (*writePort)(uint8_t port, uint8_t data);
	uint8_t (*readPort)(uint8_t port);
};

void vcore_step(struct vcpu_core* cpu);
void vcore_dump(struct vcpu_core* cpu);


#define MKC_ADDR(x) (x & 0b111111111111)
#define MKC_PID(x) ((x & 0b111) << 2)
#define MKC_IMM8(x) (x & 0xff)
#define MKC_DST(x) ((x & 0b111) << 8)
#define MKC_SRC1(x) ((x & 0b111) << 5)
#define MKC_SRC2(x) ((x & 0b111) << 2)
#define MKC_SRC1ALT1(x) ((x >> 0b111) << 8)
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

#define MKI_SRSTR(dst, imm8)		(0b1011000000000000 | MKC_DST(dst) | MKC_IMM8(imm8))
#define MKI_ADDI(dst, src, imm8)	(0b1100000000000000 | MKC_DST(dst) | MKC_SRC1ALT2(src) | MKC_IMM8(imm8))

#define REG_R0 0
#define REG_R1 1
#define REG_R2 2
#define REG_R3 3
#define REG_R4 4
#define REG_R5 5
#define REG_R6 6
#define REG_R7 7

#endif