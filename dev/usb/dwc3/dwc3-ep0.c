// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dwc3.h"
#include "dwc3-regs.h"
#include "dwc3-types.h"

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <dev/usb.h>

#define EP0_LOCK(dwc)   (&(dwc)->eps[EP0_OUT].lock)

static void dwc3_queue_setup_locked(dwc3_t* dwc) {
    arch_clean_invalidate_cache_range(dwc->ep0_buffer.vaddr, sizeof(usb_setup_t));
    dwc3_ep_start_transfer(dwc, EP0_OUT, TRB_TRBCTL_SETUP, dwc->ep0_buffer.paddr,
                           sizeof(usb_setup_t));
    dwc->ep0_state = EP0_STATE_SETUP;
}

status_t dwc3_ep0_init(dwc3_t* dwc) {
    // fifo only needed for physical endpoint 0
    status_t status = dwc3_ep_fifo_init(dwc, EP0_OUT);
    if (status != NO_ERROR) {
        return status;
    }

    for (unsigned i = EP0_OUT; i <= EP0_IN; i++) {
        dwc3_endpoint_t* ep = &dwc->eps[i];
        ep->enabled = false;
        ep->max_packet_size = EP0_MAX_PACKET_SIZE;
        ep->type = USB_ENDPOINT_CONTROL;
        ep->interval = 0;
    }

    return NO_ERROR;
}

void dwc3_ep0_reset(dwc3_t* dwc) {
    mutex_acquire(EP0_LOCK(dwc));
    dwc3_cmd_ep_end_transfer(dwc, EP0_OUT);
    dwc->ep0_state = EP0_STATE_NONE;
    mutex_release(EP0_LOCK(dwc));
}

void dwc3_ep0_start(dwc3_t* dwc) {
    mutex_acquire(EP0_LOCK(dwc));
    dwc3_cmd_start_new_config(dwc, EP0_OUT, 0);
    dwc3_ep_set_config(dwc, EP0_OUT, true);
    dwc3_ep_set_config(dwc, EP0_IN, true);

    dwc3_queue_setup_locked(dwc);
    mutex_release(EP0_LOCK(dwc));
}

static status_t dwc3_handle_setup(dwc3_t* dwc, usb_setup_t* setup, void* buffer, size_t length,
                                     size_t* out_actual) {
    if (setup->bmRequestType == (USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE)) {
        // handle some special setup requests in this driver
        switch (setup->bRequest) {
        case USB_REQ_SET_ADDRESS:
            dprintf(SPEW, "SET_ADDRESS %d\n", setup->wValue);
            dwc3_set_address(dwc, setup->wValue);
            *out_actual = 0;
            return NO_ERROR;
        case USB_REQ_SET_CONFIGURATION:
            dprintf(SPEW, "SET_CONFIGURATION %d\n", setup->wValue);
            dwc3_reset_configuration(dwc);
            dwc->configured = false;
 
            union usb_callback_args args;
            args.setup = setup;
            usbc_callback(USB_CB_SETUP_MSG, &args);

//            status = usb_dci_control(&dwc->dci_intf, setup, buffer, length, out_actual);
//            if (status == NO_ERROR && setup->wValue) {
                dwc->configured = true;
                dwc3_start_eps(dwc);
//            }
            return NO_ERROR;
        default:
            // fall through to usb_dci_control()
            break;
        }
    } else if (setup->bmRequestType == (USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_INTERFACE) &&
               setup->bRequest == USB_REQ_SET_INTERFACE) {
        dprintf(SPEW, "SET_INTERFACE %d\n", setup->wValue);
        dwc3_reset_configuration(dwc);
        dwc->configured = false;

        union usb_callback_args args;
        args.setup = setup;
        usbc_callback(USB_CB_SETUP_MSG, &args);

//        status = usb_dci_control(&dwc->dci_intf, setup, buffer, length, out_actual);
//        if (status == NO_ERROR) {
            dwc->configured = true;
            dwc3_start_eps(dwc);
//        }
        return NO_ERROR;
    }

    union usb_callback_args args;
    args.setup = setup;
    usbc_callback(USB_CB_SETUP_MSG, &args);
    return NO_ERROR;
//    return usb_dci_control(&dwc->dci_intf, setup, buffer, length, out_actual);
}

void dwc3_ep0_xfer_not_ready(dwc3_t* dwc, unsigned ep_num, unsigned stage) {
    mutex_acquire(EP0_LOCK(dwc));

    switch (dwc->ep0_state) {
    case EP0_STATE_SETUP:
        if (stage == DEPEVT_XFER_NOT_READY_STAGE_DATA ||
            stage == DEPEVT_XFER_NOT_READY_STAGE_STATUS) {
            // Stall if we receive xfer not ready data/status while waiting for setup to complete
           dwc3_cmd_ep_set_stall(dwc, EP0_OUT);
           dwc3_queue_setup_locked(dwc);
        }
        break;
    case EP0_STATE_DATA_OUT:
        if (ep_num == EP0_IN && stage == DEPEVT_XFER_NOT_READY_STAGE_DATA) {
            // end transfer and stall if we receive xfer not ready in the opposite direction
            dwc3_cmd_ep_end_transfer(dwc, EP0_OUT);
            dwc3_cmd_ep_set_stall(dwc, EP0_OUT);
            dwc3_queue_setup_locked(dwc);
        }
        break;
    case EP0_STATE_DATA_IN:
        if (ep_num == EP0_OUT && stage == DEPEVT_XFER_NOT_READY_STAGE_DATA) {
            // end transfer and stall if we receive xfer not ready in the opposite direction
            dwc3_cmd_ep_end_transfer(dwc, EP0_IN);
            dwc3_cmd_ep_set_stall(dwc, EP0_OUT);
            dwc3_queue_setup_locked(dwc);
        }
        break;
    case EP0_STATE_WAIT_NRDY_OUT:
        if (ep_num == EP0_OUT) {
            if (dwc->cur_setup.wLength > 0) {
                dwc3_ep_start_transfer(dwc, EP0_OUT, TRB_TRBCTL_STATUS_3, 0, 0);
            } else {
                dwc3_ep_start_transfer(dwc, EP0_OUT, TRB_TRBCTL_STATUS_2, 0, 0);
            }
            dwc->ep0_state = EP0_STATE_STATUS;
        }
        break;
    case EP0_STATE_WAIT_NRDY_IN:
        if (ep_num == EP0_IN) {
            if (dwc->cur_setup.wLength > 0) {
                dwc3_ep_start_transfer(dwc, EP0_IN, TRB_TRBCTL_STATUS_3, 0, 0);
            } else {
                dwc3_ep_start_transfer(dwc, EP0_IN, TRB_TRBCTL_STATUS_2, 0, 0);
            }
            dwc->ep0_state = EP0_STATE_STATUS;
        }
        break;
    default:
        dprintf(ALWAYS, "dwc3_ep0_xfer_not_ready unhandled state %u\n", dwc->ep0_state);
        break;
    }

    mutex_release(EP0_LOCK(dwc));
}

void dwc3_ep0_xfer_complete(dwc3_t* dwc, unsigned ep_num) {
    mutex_acquire(EP0_LOCK(dwc));

    switch (dwc->ep0_state) {
    case EP0_STATE_SETUP: {
        usb_setup_t* setup = &dwc->cur_setup;

        memcpy(setup, (void *)dwc->ep0_buffer.vaddr, sizeof(*setup));

        dprintf(SPEW, "got setup: type: 0x%02X req: %d value: %d index: %d length: %d\n",
                setup->bmRequestType, setup->bRequest, setup->wValue, setup->wIndex,
                setup->wLength);

        bool is_out = ((setup->bmRequestType & USB_DIR_MASK) == USB_DIR_OUT);
        if (setup->wLength > 0 && is_out) {
            // queue a read for the data phase
            dwc3_ep_start_transfer(dwc, EP0_OUT, TRB_TRBCTL_CONTROL_DATA,
                                   dwc->ep0_buffer.paddr, setup->wLength);
            dwc->ep0_state = EP0_STATE_DATA_OUT;
        } else {
            size_t actual;
            status_t status = dwc3_handle_setup(dwc, setup, (void *)dwc->ep0_buffer.vaddr,
                                                   dwc->ep0_buffer.size, &actual);
            dprintf(SPEW, "dwc3_handle_setup returned %d actual %zu\n", status, actual);
            if (status != NO_ERROR) {
                dwc3_cmd_ep_set_stall(dwc, EP0_OUT);
                dwc3_queue_setup_locked(dwc);
                break;
            }

            if (setup->wLength > 0) {
                // queue a write for the data phase
                arch_clean_cache_range(dwc->ep0_buffer.vaddr, actual);
                dwc3_ep_start_transfer(dwc, EP0_IN, TRB_TRBCTL_CONTROL_DATA, dwc->ep0_buffer.paddr,
                                       actual);
                dwc->ep0_state = EP0_STATE_DATA_IN;
            } else {
                dwc->ep0_state = EP0_STATE_WAIT_NRDY_IN;
            }
       }
       break;
    }
    case EP0_STATE_DATA_OUT:
        dwc->ep0_state = EP0_STATE_WAIT_NRDY_IN;
        break;
    case EP0_STATE_DATA_IN:
        dwc->ep0_state = EP0_STATE_WAIT_NRDY_OUT;
        break;
    case EP0_STATE_STATUS:
        dwc3_queue_setup_locked(dwc);
        break;
    default:
        break;
    }

    mutex_release(EP0_LOCK(dwc));
}
