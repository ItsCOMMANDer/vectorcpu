#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

struct instruction {
    uint16_t checkMask;
    uint16_t opcodeMask;
    void (*instruction)(uint16_t);
};

#define EXTRC_ADDR(x) (x & 0b111111111111)
#define EXTRC_PID(x) ((x >> 2) & 0b111)
#define EXTRC_IMM8(x) (x & 0xff)
#define EXTRC_DST(x) ((x >> 8) & 0b111)
#define EXTRC_SRC1(x) ((x >> 5) & 0b111)
#define EXTRC_SRC2(x) ((x >> 2) & 0b111)
#define EXTRC_SRC1ALT1(x) ((x >> 8) & 0b111)
#define EXTRC_SRC1ALT2(x) ((x >> 11) & 0b111)

void instruction_jmp(uint16_t opcode)   {printf("jmp 0x%03x\n", EXTRC_ADDR(opcode));}
void instruction_call(uint16_t opcode)  {printf("call 0x%03x\n", EXTRC_ADDR(opcode));}
void instruction_bltu(uint16_t opcode)  {printf("bltu 0x%03x\n", EXTRC_ADDR(opcode));}
void instruction_bleu(uint16_t opcode)  {printf("bleu 0x%03x\n", EXTRC_ADDR(opcode));}
void instruction_blts(uint16_t opcode)  {printf("blts 0x%03x\n", EXTRC_ADDR(opcode));}
void instruction_bles(uint16_t opcode)  {printf("bles 0x%03x\n", EXTRC_ADDR(opcode));}
void instruction_beq(uint16_t opcode)   {printf("bleq 0x%03x\n", EXTRC_ADDR(opcode));}
void instruction_bne(uint16_t opcode)   {printf("bne 0x%03x\n", EXTRC_ADDR(opcode));}

void instruction_nop(uint16_t __attribute__((unused)) opcode)   {printf("nop\n");}
void instruction_halt(uint16_t __attribute__((unused)) opcode)  {printf("halt\n");}
void instruction_ret(uint16_t __attribute__((unused)) opcode)   {printf("ret\n");}
void instruction_enzr(uint16_t __attribute__((unused)) opcode)  {printf("enzr\n");}
void instruction_dszr(uint16_t __attribute__((unused)) opcode)  {printf("dszr\n");}

void instruction_push(uint16_t opcode)  {printf("push r%c\n", '0' + EXTRC_SRC1(opcode));}
void instruction_wrss(uint16_t opcode)  {printf("wrss r%c\n", '0' + EXTRC_SRC1(opcode));}
void instruction_wrds(uint16_t opcode)  {printf("wrds r%c\n", '0' + EXTRC_SRC1(opcode));}
void instruction_wrsp(uint16_t opcode)  {printf("wrsp r%c\n", '0' + EXTRC_SRC1(opcode));}

void instruction_pop(uint16_t opcode)   {printf("pop t%c\n", '0' + EXTRC_DST(opcode));}
void instruction_rdss(uint16_t opcode)  {printf("rdss t%c\n", '0' + EXTRC_DST(opcode));}
void instruction_rdds(uint16_t opcode)  {printf("rdds t%c\n", '0' + EXTRC_DST(opcode));}
void instruction_rdsp(uint16_t opcode)  {printf("rdsp t%c\n", '0' + EXTRC_DST(opcode));}

void instruction_out(uint16_t opcode)   {printf("out r%c, %u\n", '0' + EXTRC_SRC1(opcode), EXTRC_PID(opcode));}

void instruction_in(uint16_t opcode)    {printf("in r%c, %u\n", '0' + EXTRC_DST(opcode), EXTRC_PID(opcode));}

void instruction_mov(uint16_t opcode)   {printf("mov r%c, r%c\n", '0' + EXTRC_DST(opcode), '0' + EXTRC_SRC1(opcode));}
void instruction_not(uint16_t opcode)   {printf("not r%c, r%c\n", '0' + EXTRC_DST(opcode), '0' + EXTRC_SRC1(opcode));}
void instruction_ldr(uint16_t opcode)   {printf("ldr r%c, r%c\n", '0' + EXTRC_DST(opcode), '0' + EXTRC_SRC1(opcode));}
void instruction_str(uint16_t opcode)   {printf("str r%c, r%c\n", '0' + EXTRC_DST(opcode), '0' + EXTRC_SRC1(opcode));}

void instruction_add(uint16_t opcode)   {printf("add r%c, r%c, r%c\n", '0' + EXTRC_DST(opcode), '0' + EXTRC_SRC1(opcode), '0' + EXTRC_SRC2(opcode));}
void instruction_sub(uint16_t opcode)   {printf("sub r%c, r%c, r%c\n", '0' + EXTRC_DST(opcode), '0' + EXTRC_SRC1(opcode), '0' + EXTRC_SRC2(opcode));}
void instruction_adc(uint16_t opcode)   {printf("adc r%c, r%c, r%c\n", '0' + EXTRC_DST(opcode), '0' + EXTRC_SRC1(opcode), '0' + EXTRC_SRC2(opcode));}
void instruction_sbc(uint16_t opcode)   {printf("sbc r%c, r%c, r%c\n", '0' + EXTRC_DST(opcode), '0' + EXTRC_SRC1(opcode), '0' + EXTRC_SRC2(opcode));}
void instruction_and(uint16_t opcode)   {printf("and r%c, r%c, r%c\n", '0' + EXTRC_DST(opcode), '0' + EXTRC_SRC1(opcode), '0' + EXTRC_SRC2(opcode));}
void instruction_or(uint16_t opcode)    {printf("or r%c, r%c, r%c\n", '0' + EXTRC_DST(opcode), '0' + EXTRC_SRC1(opcode), '0' + EXTRC_SRC2(opcode));}
void instruction_xor(uint16_t opcode)   {printf("xor r%c, r%c, r%c\n", '0' + EXTRC_DST(opcode), '0' + EXTRC_SRC1(opcode), '0' + EXTRC_SRC2(opcode));}

void instruction_ldi(uint16_t opcode)   {printf("ldi r%c, %u\n", '0' + EXTRC_DST(opcode), EXTRC_IMM8(opcode));}
void instruction_srldr(uint16_t opcode) {printf("srldr r%c, %u\n", '0' + EXTRC_DST(opcode), EXTRC_IMM8(opcode));}

void instruction_srstr(uint16_t opcode) {printf("srstr r%c, %u\n", '0' + EXTRC_SRC1ALT1(opcode), EXTRC_IMM8(opcode));}

void instruction_addi(uint16_t opcode)  {printf("addi r%c, r%c, %u\n", '0' + EXTRC_DST(opcode), '0' + EXTRC_SRC1ALT2(opcode), EXTRC_IMM8(opcode));}

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
    {0b1111100000000000, 0b1010100000000000, &instruction_srldr},
    {0b1111100000000000, 0b1011000000000000, &instruction_srstr},
    {0b1100000000000000, 0b1100000000000000, &instruction_addi},
};

#define INS_NUM sizeof(isa) / sizeof(isa[0])

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("Too little args\n");
    }

    FILE* inputFile_fd = fopen(argv[1], "rb");

    if(!inputFile_fd) {
        perror("fopen");
        return 1;
    }

    fseek(inputFile_fd, 0, SEEK_END);
    size_t inputFile_size = ftell(inputFile_fd);
    fseek(inputFile_fd, 0, SEEK_SET);

    uint16_t *buf = calloc(inputFile_size, 1);

    fread(buf, 1, inputFile_size, inputFile_fd);

    fclose(inputFile_fd);

    for(size_t i = 0; i < inputFile_size/2; i++) {
        bool found = false;
        for(size_t j = 0; j < INS_NUM; j++) {
            //printf("INSTR: 0x%04x\n", buf[i]);
            if((buf[i] & isa[j].checkMask) == isa[j].opcodeMask) {
                printf("0x%03lx:\t", (i*2) & 0xfff);
                isa[j].instruction(buf[i]);
                found = true;
                break;
            }
        }
        if(!found) printf("????");
    }

    free(buf);
    return 0;
}