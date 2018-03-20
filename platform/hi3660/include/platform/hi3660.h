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


#pragma once

#define SDRAM_BASE                      (0x0U)
#define HI3660_PERIPH_BASE_PHYS         (0xe8100000)
#define HI3660_PERIPH_BASE_SIZE         (0x17f00000)
#define HI3660_PERIPH_BASE_VIRT         (0xFFFFFFFFe8100000ULL)
#define HI3660_PERIPH_VIRT_OFFSET       (HI3660_PERIPH_BASE_VIRT - HI3660_PERIPH_BASE_PHYS)

#define UART0_BASE                      (HI3660_PERIPH_VIRT_OFFSET + 0xfff32000)
#define GIC_BASE                        (HI3660_PERIPH_VIRT_OFFSET + 0xe82b0000)

#define UART0_IRQ                       111
