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
#include "mocom_app.hpp"
#include <app/mocom.h>

#include <err.h>
#include <debug.h>
#include <stdio.h>
#include <trace.h>
#include <target.h>
#include <compiler.h>
#include <app.h>
#include <kernel/thread.h>

#include "transport.hpp"
#include "usb.hpp"

#define LOCAL_TRACE 1

namespace mocom {

mocom_app::mocom_app(transport &t)
:   m_transport(t),
    m_mux(m_transport)
{
}

mocom_app::~mocom_app()
{
}

status_t mocom_app::worker()
{
    status_t err;

    // let the transport layer do some work
    err = m_transport.do_work();
    if (err < 0)
        return err;

    return NO_ERROR;
}

status_t mocom_app::init()
{
    m_transport.set_mux(&m_mux);
    m_transport.init();
    m_mux.init();

    return NO_ERROR;
}

extern "C" void mocom_init(const struct app_descriptor *app)
{
}

extern "C" void mocom_entry(const struct app_descriptor *app, void *args)
{
    LTRACE_ENTRY;

    // construct a transport for us to talk over
#if WITH_DEV_USB
    transport *t = create_usb_transport();
#endif

    // construct the main mocom app
    mocom_app theapp(*t);

    theapp.init();

    for (;;) {
        // run the worker thread tasks
        if (theapp.worker() < 0)
            break;
    }
}

} // namespace mocom

