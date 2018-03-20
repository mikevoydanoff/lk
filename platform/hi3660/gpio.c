// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dev/gpio.h>
#include <kernel/vm.h>
#include <platform/hi3660-hw.h>

#include <assert.h>
#include <err.h>
#include <reg.h>
#include <stdlib.h>
#include <stdio.h>

// GPIO register offsets
#define GPIODATA(mask)  ((mask) << 2)   // Data registers, mask provided as index
#define GPIODIR         0x400           // Data direction register (0 = IN, 1 = OUT)
#define GPIOIS          0x404           // Interrupt sense register (0 = edge, 1 = level)
#define GPIOIBE         0x408           // Interrupt both edges register (1 = both)
#define GPIOIEV         0x40C           // Interrupt event register (0 = falling, 1 = rising)
#define GPIOIE          0x410           // Interrupt mask register (1 = interrupt masked)
#define GPIORIS         0x414           // Raw interrupt status register
#define GPIOMIS         0x418           // Masked interrupt status register
#define GPIOIC          0x41C           // Interrupt clear register
#define GPIOAFSEL       0x420           // Mode control select register

#define GPIOS_PER_PAGE  8

typedef struct {
    paddr_t  base;
    size_t      length;
    uint32_t    start_pin;
    uint32_t    pin_count;
} gpio_block_t;

static const gpio_block_t gpio_blocks[] = {
    {
        // GPIO groups 0 - 17
        .base = MMIO_GPIO0_BASE,
        .length = 18 * 4096,
        .start_pin = 0,
        .pin_count = 18 * 8,
    },
    {
        // GPIO groups 18 and 19
        .base = MMIO_GPIO18_BASE,
        .length = 2 * 4096,
        .start_pin = 18 * 8,
        .pin_count = 2 * 8,
    },
    {
        // GPIO groups 20 and 21
        .base = MMIO_GPIO20_BASE,
        .length = 2 * 4096,
        .start_pin = 20 * 8,
        .pin_count = 2 * 8,
    },
    {
        // GPIO groups 22 - 27
        .base = MMIO_GPIO22_BASE,
        .length = 6 * 4096,
        .start_pin = 22 * 8,
        .pin_count = 6 * 8,
    },
    {
        // GPIO group 28
        .base = MMIO_GPIO28_BASE,
        .length = 1 * 4096,
        .start_pin = 28 * 8,
        .pin_count = 1 * 8,
    },
};

static const gpio_block_t* find_gpio_block(unsigned nr) {
    const gpio_block_t* block = gpio_blocks;
    const gpio_block_t* end = block + countof(gpio_blocks);
    
    while (block < end) {
        if (nr >= block->start_pin && nr < block->start_pin + block->pin_count) {
            return block;
        }
        block++;
    }
    dprintf(ALWAYS, "find_gpio_block failed for nr %u\n", nr);
    return NULL;
}

int gpio_config(unsigned nr, unsigned flags) {
    const gpio_block_t* block = find_gpio_block(nr);
    if (!block) {
        return ERR_INVALID_ARGS;
    }

    nr -= block->start_pin;
    volatile uint8_t* regs = paddr_to_kvaddr(block->base) + PAGE_SIZE * (nr / GPIOS_PER_PAGE);
    uint8_t bit = 1 << (nr % GPIOS_PER_PAGE);

    uint8_t dir = readb(regs + GPIODIR);
    if (flags & GPIO_OUTPUT) {
        dir |= bit;
    } else {
        dir &= ~bit;
    }
    writeb(dir, regs + GPIODIR);

    uint8_t trigger = readb(regs + GPIOIS);
    if (flags & GPIO_LEVEL) {
        trigger |= bit;
    } else {
        trigger &= ~bit;
    }
    writeb(trigger, regs + GPIOIS);

    uint8_t be = readb(regs + GPIOIBE);
    uint8_t iev = readb(regs + GPIOIEV);

    if ((flags & GPIO_EDGE) && (flags & GPIO_RISING) && (flags & GPIO_FALLING)) {
        be |= bit;
     } else {
        be &= ~bit;
     }
    if ((flags & GPIO_EDGE) && (flags & GPIO_RISING) && !(flags & GPIO_FALLING)) {
        iev |= bit;
     } else {
        iev &= ~bit;
     }

    writeb(be, regs + GPIOIBE);
    writeb(iev, regs + GPIOIEV);

    return NO_ERROR;
}

int gpio_get(unsigned nr) {
    const gpio_block_t* block = find_gpio_block(nr);
    assert(block);

    nr -= block->start_pin;
    volatile uint8_t* regs = paddr_to_kvaddr(block->base) + PAGE_SIZE * (nr / GPIOS_PER_PAGE);
    uint8_t bit = 1 << (nr % GPIOS_PER_PAGE);

    return !!(readb(regs + GPIODATA(bit)) & bit);
}

void gpio_set(unsigned nr, unsigned on) {
    const gpio_block_t* block = find_gpio_block(nr);
    assert(block);

    nr -= block->start_pin;

    volatile uint8_t* regs = paddr_to_kvaddr(block->base) + PAGE_SIZE * (nr / GPIOS_PER_PAGE);
    uint8_t bit = 1 << (nr % GPIOS_PER_PAGE);

    writeb((on ? bit : 0), regs + GPIODATA(bit));
}
