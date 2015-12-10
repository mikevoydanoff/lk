/*
 * Copyright (c) 2015 Travis Geiselbrecht
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
#include "channel.hpp"

#include <err.h>
#include <debug.h>
#include <stdio.h>
#include <assert.h>
#include <trace.h>
#include <string.h>
#include <rand.h>
#include <target.h>
#include <compiler.h>
#include <platform.h>

#include "mux.hpp"
#include "prot/mux.h"
#include "prot/packet_struct.h"

#define LOCAL_TRACE 1

namespace mocom {

status_t channel::queue_tx(const void *buf, size_t len)
{
    LTRACEF("buf %p, len %zu\n", buf, len);

    if (m_tx_buf)
        return ERR_BUSY;

    m_tx_buf = (const uint8_t *)buf;
    m_tx_len = len;

    return NO_ERROR;
}

status_t channel::close()
{
    return NO_ERROR;
}

void control_channel::process_rx_packet(const uint8_t *buf, size_t len)
{
    LTRACEF("buf %p, len %zu\n", buf, len);

    const struct pmux_control_header *header = (const struct pmux_control_header *)buf;

    if (len < sizeof(struct pmux_control_header))
        return;

    switch (header->op) {
        default:
        case PMUX_CONTROL_NONE:
            break;
        case PMUX_CONTROL_CHANNEL_CLOSED:
            PANIC_UNIMPLEMENTED;
            break;
        case PMUX_CONTROL_OPEN:
            // they're asking us to open a new channel
            PANIC_UNIMPLEMENTED;
            break;
        case PMUX_CONTROL_OPEN_COMMAND: {
            // they're asking us to open a command interpreter channel
            channel *c = new command_channel(m_mux, header->channel);
            if (!m_mux.add_channel(c)) {
                // already exists
                delete c;
            }
            break;
        }
        case PMUX_CONTROL_CLOSE: {
            // they're asking us to close a channel
            channel *c = m_mux.find_channel(header->channel);
            if (c) {
                c->close();

                m_mux.remove_channel(c);
            }
            break;
        }
    }
}

void command_channel::process_rx_packet(const uint8_t *buf, size_t len)
{
    LTRACEF("buf %p, len %zu\n", buf, len);

    hexdump8_ex(buf, len, 0);

    if (m_state == STATE_INITIAL) {
        // try to parse the incoming command
        /// XXX ignore it for now
        channel::queue_tx(strdup("ok 0\n"), strlen("ok 0\n"));
        m_state = STATE_ESTABLISHED;
    } else if (m_state == STATE_ESTABLISHED) {
        const struct packet_header *header = (const struct packet_header *)buf;

        if (len < sizeof(struct packet_header))
            return; // XXX shoudl probably close the channel
        if (header->magic != PACKET_HEADER_MAGIC)
            return;
        if (header->version != PACKET_HEADER_VERSION)
            return;

        if (header->type == PACKET_HEADER_TYPE_DATA) {
            const uint8_t *data = (const uint8_t *)header->data;
            hexdump8_ex(data, header->size, 0);
        }

        queue_tx("hello\n", sizeof("hello\n"));
    }
}

status_t command_channel::queue_tx(const void *buf, size_t len)
{
    /// XXX remove
    uint8_t *newbuf = new uint8_t[len + sizeof(struct packet_header)];
    if (!buf)
        return ERR_NO_MEMORY;

    struct packet_header *header = (struct packet_header *)newbuf;
    header->magic = PACKET_HEADER_MAGIC;
    header->version = PACKET_HEADER_VERSION;
    header->type = PACKET_HEADER_TYPE_DATA;
    header->size = len;

    memcpy(header->data, buf, len);

    return channel::queue_tx(newbuf, sizeof(struct packet_header) + len);
}

void command_channel::tx_complete()
{
    delete[] m_tx_buf;

    channel::tx_complete();
}

};

