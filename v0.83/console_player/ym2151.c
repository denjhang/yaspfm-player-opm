#include <stdint.h>
#include "ym2151.h"
#include "spfm.h"
#include "chiptype.h"
#include "util.h" // For yasp_uspin

// YM2151 a.k.a OPM
extern volatile int g_flush_mode;

void ym2151_write_reg(uint8_t slot, uint8_t addr, uint8_t data) {
    spfm_write_reg(slot, 0, addr, data);
    if (g_flush_mode == 1) { // Register-level flush
        spfm_flush();
    }
}

void ym2151_mute(uint8_t slot) {
    int i;
    // A buffer large enough for all mute commands.
    // (0xFF-0xE0+1) for SL/RR + 8 for Key-Off + 256 for zeroing = 32 + 8 + 256 = 296
    spfm_reg_t regs[300];
    int count = 0;

    // 1. Set SL=15, RR=15 for all operators
    for (i = 0xE0; i <= 0xFF; i++) {
        regs[count++] = (spfm_reg_t){0, (uint8_t)i, 0xFF};
    }

    // 2. Key off all channels
    for (i = 0; i < 8; i++) {
        regs[count++] = (spfm_reg_t){0, 0x08, (uint8_t)i};
    }

    // 3. Zero all registers for a full reset
    for (i = 0; i <= 0xFF; i++) {
        regs[count++] = (spfm_reg_t){0, (uint8_t)i, 0x00};
    }

    // Write all commands in a single batch
    spfm_write_regs(slot, regs, count, 0);
}

void ym2151_init(uint8_t slot) {
    int i;
    spfm_reg_t regs[256];
    int count = 0;

    for (i = 0; i <= 0xFF; i++) {
        regs[count++] = (spfm_reg_t){0, (uint8_t)i, 0x00};
    }
    spfm_write_regs(slot, regs, count, 0);
    count = 0;

    regs[count++] = (spfm_reg_t){0, 0x01, 0x02};
    spfm_write_regs(slot, regs, count, 0);
    count = 0;

    for (i = 0; i < 8; i++) {
        regs[count++] = (spfm_reg_t){0, 0x08, (uint8_t)i};
    }
    spfm_write_regs(slot, regs, count, 0);
    count = 0;

    for (i = 0x60; i <= 0x7F; i++) {
        regs[count++] = (spfm_reg_t){0, (uint8_t)i, 0x7F};
    }
    spfm_write_regs(slot, regs, count, 0);
    
    spfm_flush();
}
