/*
 * Copyright (c) 2018 The Fuchsia Authors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <arch/arm64.h>
#include <trace.h>
#include <assert.h>
#include <err.h>
#include <bits.h>
#include <kernel/spinlock.h>
#include <kernel/thread.h>
#include <kernel/mp.h>
#include <platform/interrupts.h>
#include <lk/init.h>
#include <kernel/vm.h>
#include <kernel/spinlock.h>
#include <dev/timer/arm_generic.h>
#include <platform.h>
#include <dev/interrupt/arm_gic.h>
#include <dev/timer/arm_generic.h>
#include <dev/usb/dwc3/dwc3.h>
#include <dev/uart.h>
#include <platform/hi3660.h>
#include <platform/usb.h>

typedef struct arm64_iframe_long arm_platform_iframe_t;

/* initial memory mappings. parsed by start.S */
struct mmu_initial_mapping mmu_initial_mappings[] = {
    {
        .phys = SDRAM_BASE,
        .virt = KERNEL_BASE,
        .size = MEMSIZE,
        .flags = 0,
        .name = "memory"
    },

    /* peripherals */
    {
        .phys = HI3660_PERIPH_BASE_PHYS,
        .virt = HI3660_PERIPH_BASE_VIRT,
        .size = HI3660_PERIPH_BASE_SIZE,
        .flags = MMU_INITIAL_MAPPING_FLAG_DEVICE,
        .name = "peripherals"
    },
    /* null entry to terminate the list */
    { 0 }
};

static pmm_arena_t arena = {
    .name = "sdram",
    .base = SDRAM_BASE,
    .size = MEMSIZE,
    .flags = PMM_ARENA_FLAG_KMAP,
};

#define DEBUG_UART 0

void platform_dputc(char c)
{
    if (c == '\n')
        uart_putc(DEBUG_UART, '\r');
    uart_putc(DEBUG_UART, c);
}

int platform_dgetc(char *c, bool wait)
{
    int ret = uart_getc(DEBUG_UART, wait);
    if (ret == -1)
        return -1;
    *c = ret;
    return 0;
}

void platform_init(void)
{
    uart_init();
    hi3660_usb_init();
    usb_dwc3_init();
}

void platform_early_init(void)
{
*((char *)0xfffffffffff32000) = 'b';

    uart_init_early();

    /* initialize the interrupt controller */
    arm_gic_init();

    arm_generic_timer_init(30, 0);

    pmm_add_arena(&arena);

    struct list_node list = LIST_INITIAL_VALUE(list);
    // memory to reserve to avoid stomping on bootloader data
    pmm_alloc_range(0x00000000, 0x00080000 / PAGE_SIZE, &list);
    // bl31
    pmm_alloc_range(0x20200000, 0x200000 / PAGE_SIZE, &list);
    // pstore
    pmm_alloc_range(0x20a00000, 0x100000 / PAGE_SIZE, &list);
    // lpmx-core
    pmm_alloc_range(0x89b80000, 0x100000 / PAGE_SIZE, &list);
    // lpmcu
    pmm_alloc_range(0x89c80000, 0x40000 / PAGE_SIZE, &list);
}

