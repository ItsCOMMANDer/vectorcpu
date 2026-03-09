#ifndef LIB_VIRT_CORE_H
#define LIB_VIRT_CORE_H

#include <stdint.h>
#include <stdbool.h>

struct vcpu_core {
    uint8_t gpr[8];
    uint16_t pc;
    uint8_t ds;
    uint8_t ss_sp;

    uint8_t flags;
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

#endif