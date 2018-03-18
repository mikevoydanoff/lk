// Copyright 2016 The Fuchsia Authors
// Copyright (c) 2014-2015 Travis Geiselbrecht
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <reg.h>
#include <stdio.h>
#include <trace.h>
#include <lib/cbuf.h>
#include <kernel/thread.h>
#include <platform/interrupts.h>
#include <platform/hi3660.h>
#include <dev/uart.h>

/* PL011 implementation */
#define UART_DR    (0x00)
#define UART_RSR   (0x04)
#define UART_FR    (0x18)
#define UART_ILPR  (0x20)
#define UART_IBRD  (0x24)
#define UART_FBRD  (0x28)
#define UART_LCRH  (0x2c)
#define UART_CR    (0x30)
#define UART_IFLS  (0x34)
#define UART_IMSC  (0x38)
#define UART_TRIS  (0x3c)
#define UART_TMIS  (0x40)
#define UART_ICR   (0x44)
#define UART_DMACR (0x48)

#define UARTREG(base, reg)  (*REG32((base)  + (reg)))

#define RXBUF_SIZE 16

static cbuf_t uart_rx_buf;
static uint64_t uart_base = 0;

/*
 * Tx driven irq:
 * NOTE: For the pl011, txim is the "ready to transmit" interrupt. So we must
 * mask it when we no longer care about it and unmask it when we start
 * xmitting.
 */
static event_t uart_dputc_event = EVENT_INITIAL_VALUE(uart_dputc_event, true, 0);
static spin_lock_t uart_spinlock = SPIN_LOCK_INITIAL_VALUE;

static inline void pl011_mask_tx(void)
{
    UARTREG(uart_base, UART_IMSC) &= ~(1<<5);
}

static inline void pl011_unmask_tx(void)
{
    UARTREG(uart_base, UART_IMSC) |= (1<<5);
}

static enum handler_return pl011_uart_irq(void *arg)
{
    bool resched = false;

    /* read interrupt status and mask */
    uint32_t isr = UARTREG(uart_base, UART_TMIS);

    if (isr & ((1<<4) | (1<<6))) { // rxmis
        /* while fifo is not empty, read chars out of it */
        while ((UARTREG(uart_base, UART_FR) & (1<<4)) == 0) {
            /* if we're out of rx buffer, mask the irq instead of handling it */
            if (cbuf_space_avail(&uart_rx_buf) == 0) {
                UARTREG(uart_base, UART_IMSC) &= ~((1<<4)|(1<<6)); // !rxim
                break;
            }

            char c = UARTREG(uart_base, UART_DR);
            cbuf_write_char(&uart_rx_buf, c, false);
            resched = true;
        }
    }
    spin_lock(&uart_spinlock);
    if (isr & (1<<5)) {
        /*
         * Signal any waiting Tx and mask Tx interrupts once we
         * wakeup any blocked threads
         */
        event_signal(&uart_dputc_event, true);
        pl011_mask_tx();
    }
    spin_unlock(&uart_spinlock);

    return resched ? INT_RESCHEDULE : INT_NO_RESCHEDULE;
}

void uart_init(void)
{
    // create circular buffer to hold received data
    cbuf_initialize(&uart_rx_buf, RXBUF_SIZE);

    // assumes interrupts are contiguous
    register_int_handler(UART0_IRQ, &pl011_uart_irq, NULL);

    // clear all irqs
    UARTREG(uart_base, UART_ICR) = 0x3ff;

    // set fifo trigger level
    UARTREG(uart_base, UART_IFLS) = 0; // 1/8 rxfifo, 1/8 txfifo

    // enable rx interrupt
    UARTREG(uart_base, UART_IMSC) = (1 << 4 ) |  //  rxim
                                    (1 << 6);    //  rtim

    // enable receive
    UARTREG(uart_base, UART_CR) |= (1<<9); // rxen

    // enable interrupt
    unmask_interrupt(UART0_IRQ);
}

int uart_getc(int port, bool wait)
{
    char c;
    if (cbuf_read_char(&uart_rx_buf, &c, wait) == 1) {
        UARTREG(uart_base, UART_IMSC) |= ((1<<4)|(1<<6)); // rxim
        return c;
    }

    return -1;
}

/* panic-time getc/putc */
int uart_pputc(int port, char c)
{
    /* spin while fifo is full */
    while (UARTREG(uart_base, UART_FR) & (1<<5))
        ;
    UARTREG(uart_base, UART_DR) = c;

    return 1;
}

int uart_pgetc(int port)
{
    if ((UARTREG(uart_base, UART_FR) & (1<<4)) == 0) {
        return UARTREG(uart_base, UART_DR);
    } else {
        return -1;
    }
}

int uart_putc(int port, char c)
{
    spin_lock_saved_state_t state;

    spin_lock_irqsave(&uart_spinlock, state);
    // Is FIFO Full ?
    while (UARTREG(uart_base, UART_FR) & (1<<5)) {
        /* Unmask Tx interrupts before we block on the event */
        pl011_unmask_tx();
        spin_unlock_irqrestore(&uart_spinlock, state);
        event_wait(&uart_dputc_event);
        spin_lock_irqsave(&uart_spinlock, state);
    }

    UARTREG(uart_base, UART_DR) = c;
    spin_unlock_irqrestore(&uart_spinlock, state);

    return 1;
}

void uart_init_early(void)
{
    uart_base = UART0_BASE;
*((char *)UART0_BASE) = 'c';

    UARTREG(uart_base, UART_CR) = (1<<8)|(1<<0); // tx_enable, uarten
}
