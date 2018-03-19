// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <err.h>
#include <reg.h>
#include <dev/usb.h>
#include <dev/usbc.h>
#include <kernel/vm.h>
#include <lk/init.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <platform/dwc3.h>

#include "dwc3.h"
#include "dwc3-regs.h"
#include "dwc3-types.h"

// MMIO indices
enum {
    MMIO_USB3OTG,
};

// IRQ indices
enum {
    IRQ_USB3,
};

status_t io_buffer_init(io_buffer_t* buffer, size_t size) {
    void* addr;
    status_t status = vmm_alloc_contiguous(vmm_get_kernel_aspace(), "dwc3-buffer", size, &addr, 0, 0, ARCH_MMU_FLAG_CACHED);
    if (status != NO_ERROR) {
        return status;
    }
    buffer->vaddr = (vaddr_t)addr;
    buffer->paddr = vaddr_to_paddr(addr);
    buffer->size = size;
    return NO_ERROR;
}

status_t io_buffer_init_physical(io_buffer_t* buffer, paddr_t paddr, size_t size) {
    void* addr;
    status_t status = vmm_alloc_physical(vmm_get_kernel_aspace(), "dwc3-regs", size, &addr, 0, paddr, 0, ARCH_MMU_FLAG_UNCACHED_DEVICE);
    if (status != NO_ERROR) {
        return status;
    }
    buffer->vaddr = (vaddr_t)addr;
    buffer->paddr = vaddr_to_paddr(addr);
    buffer->size = size;
    return NO_ERROR;
}

status_t usbc_set_active(bool active) {
    return NO_ERROR;
}

void usbc_set_address(uint8_t address) {
printf("usbc_set_address %u\n", address);
}

void usbc_ep0_ack(void) {
printf("usbc_ep0_ack\n");
}

void usbc_ep0_stall(void) {
printf("usbc_ep0_stall\n");
}

void usbc_ep0_send(const void *buf, size_t len, size_t maxlen) {
printf("usbc_ep0_send\n");
}

void usbc_ep0_recv(void *buf, size_t len, ep_callback cb) {
printf("usbc_ep0_recv\n");
}

bool usbc_is_highspeed(void) {
    return true;
}

void dwc3_wait_bits(volatile uint32_t* ptr, uint32_t bits, uint32_t expected) {
    uint32_t value = DWC3_READ32(ptr);
    while ((value & bits) != expected) {
        thread_sleep(1);
        value = DWC3_READ32(ptr);
    }
}

void dwc3_print_status(dwc3_t* dwc) {
    volatile void* mmio = dwc3_mmio(dwc);
    uint32_t status = DWC3_READ32(mmio + DSTS);
    dprintf(SPEW, "DSTS: ");
    dprintf(SPEW, "USBLNKST: %d ", DSTS_USBLNKST(status));
    dprintf(SPEW, "SOFFN: %d ", DSTS_SOFFN(status));
    dprintf(SPEW, "CONNECTSPD: %d ", DSTS_CONNECTSPD(status));
    if (status & DSTS_DCNRD) dprintf(SPEW, "DCNRD ");
    if (status & DSTS_SRE) dprintf(SPEW, "SRE ");
    if (status & DSTS_RSS) dprintf(SPEW, "RSS ");
    if (status & DSTS_SSS) dprintf(SPEW, "SSS ");
    if (status & DSTS_COREIDLE) dprintf(SPEW, "COREIDLE ");
    if (status & DSTS_DEVCTRLHLT) dprintf(SPEW, "DEVCTRLHLT ");
    if (status & DSTS_RXFIFOEMPTY) dprintf(SPEW, "RXFIFOEMPTY ");
    dprintf(SPEW, "\n");

    status = DWC3_READ32(mmio + GSTS);
    dprintf(SPEW, "GSTS: ");
    dprintf(SPEW, "CBELT: %d ", GSTS_CBELT(status));
    dprintf(SPEW, "CURMOD: %d ", GSTS_CURMOD(status));
    if (status & GSTS_SSIC_IP) dprintf(SPEW, "SSIC_IP ");
    if (status & GSTS_OTG_IP) dprintf(SPEW, "OTG_IP ");
    if (status & GSTS_BC_IP) dprintf(SPEW, "BC_IP ");
    if (status & GSTS_ADP_IP) dprintf(SPEW, "ADP_IP ");
    if (status & GSTS_HOST_IP) dprintf(SPEW, "HOST_IP ");
    if (status & GSTS_DEVICE_IP) dprintf(SPEW, "DEVICE_IP ");
    if (status & GSTS_CSR_TIMEOUT) dprintf(SPEW, "CSR_TIMEOUT ");
    if (status & GSTS_BUSERRADDRVLD) dprintf(SPEW, "BUSERRADDRVLD ");
    dprintf(SPEW, "\n");
}

static void dwc3_stop(dwc3_t* dwc) {
    volatile void* mmio = dwc3_mmio(dwc);
    uint32_t temp;

    mutex_acquire(&dwc->lock);

    temp = DWC3_READ32(mmio + DCTL);
    temp &= ~DCTL_RUN_STOP;
    temp |= DCTL_CSFTRST;
    DWC3_WRITE32(mmio + DCTL, temp);
    dwc3_wait_bits(mmio + DCTL, DCTL_CSFTRST, 0);

    mutex_release(&dwc->lock);
}

static void dwc3_start_peripheral_mode(dwc3_t* dwc) {
    volatile void* mmio = dwc3_mmio(dwc);
    uint32_t temp;

    mutex_acquire(&dwc->lock);

    // configure and enable PHYs
    temp = DWC3_READ32(mmio + GUSB2PHYCFG(0));
    temp &= ~(GUSB2PHYCFG_USBTRDTIM_MASK | GUSB2PHYCFG_SUSPENDUSB20);
    temp |= GUSB2PHYCFG_USBTRDTIM(9);
    DWC3_WRITE32(mmio + GUSB2PHYCFG(0), temp);

    temp = DWC3_READ32(mmio + GUSB3PIPECTL(0));
    temp &= ~(GUSB3PIPECTL_DELAYP1TRANS | GUSB3PIPECTL_SUSPENDENABLE);
    temp |= GUSB3PIPECTL_LFPSFILTER | GUSB3PIPECTL_SS_TX_DE_EMPHASIS(1);
    DWC3_WRITE32(mmio + GUSB3PIPECTL(0), temp);

    // configure for device mode
    DWC3_WRITE32(mmio + GCTL, GCTL_U2EXIT_LFPS | GCTL_PRTCAPDIR_DEVICE | GCTL_U2RSTECN |
                              GCTL_PWRDNSCALE(2));

    temp = DWC3_READ32(mmio + DCFG);
    uint32_t nump = 16;
    uint32_t max_speed = DCFG_DEVSPD_SUPER;
    temp &= ~DWC3_MASK(DCFG_NUMP_START, DCFG_NUMP_BITS);
    temp |= nump << DCFG_NUMP_START;
    temp &= ~DWC3_MASK(DCFG_DEVSPD_START, DCFG_DEVSPD_BITS);
    temp |= max_speed << DCFG_DEVSPD_START;
    temp &= ~DWC3_MASK(DCFG_DEVADDR_START, DCFG_DEVADDR_BITS);  // clear address
    DWC3_WRITE32(mmio + DCFG, temp);

    dwc3_events_start(dwc);
    mutex_release(&dwc->lock);

    dwc3_ep0_start(dwc);

    mutex_acquire(&dwc->lock);

    // start the controller
    DWC3_WRITE32(mmio + DCTL, DCTL_RUN_STOP);

    mutex_release(&dwc->lock);
}

static void dwc3_start_host_mode(dwc3_t* dwc) {
    volatile void* mmio = dwc3_mmio(dwc);

    mutex_acquire(&dwc->lock);

    // configure for host mode
    DWC3_WRITE32(mmio + GCTL, GCTL_U2EXIT_LFPS | GCTL_PRTCAPDIR_HOST | GCTL_U2RSTECN |
                              GCTL_PWRDNSCALE(2));
    mutex_release(&dwc->lock);

}

void dwc3_usb_reset(dwc3_t* dwc) {
    dprintf(INFO, "dwc3_usb_reset\n");

    dwc3_ep0_reset(dwc);

    for (unsigned i = 2; i < countof(dwc->eps); i++) {
        dwc3_ep_end_transfers(dwc, i, ERR_OFFLINE);
        dwc3_ep_set_stall(dwc, i, false);
    }

    dwc3_set_address(dwc, 0);
    dwc3_ep0_start(dwc);

    usbc_callback(USB_CB_RESET, NULL);
//    usb_dci_set_connected(&dwc->dci_intf, true);
}

void dwc3_disconnected(dwc3_t* dwc) {
    dprintf(INFO, "dwc3_disconnected\n");

    dwc3_cmd_ep_end_transfer(dwc, EP0_OUT);
    dwc->ep0_state = EP0_STATE_NONE;

// is this right?
    usbc_callback(USB_CB_ONLINE, NULL);

//    if (dwc->dci_intf.ops) {
//        usb_dci_set_connected(&dwc->dci_intf, false);
//    }

    for (unsigned i = 2; i < countof(dwc->eps); i++) {
        dwc3_ep_end_transfers(dwc, i, ERR_OFFLINE);
        dwc3_ep_set_stall(dwc, i, false);
    }
}

void dwc3_connection_done(dwc3_t* dwc) {
    volatile void* mmio = dwc3_mmio(dwc);

    mutex_acquire(&dwc->lock);
    uint32_t status = DWC3_READ32(mmio + DSTS);
    uint32_t speed = DSTS_CONNECTSPD(status);
    unsigned ep0_max_packet = 0;

    switch (speed) {
    case DSTS_CONNECTSPD_HIGH:
        dwc->speed = USB_SPEED_HIGH;
        ep0_max_packet = 64;
        break;
    case DSTS_CONNECTSPD_FULL:
        dwc->speed = USB_SPEED_FULL;
        ep0_max_packet = 64;
        break;
    case DSTS_CONNECTSPD_SUPER:
    case DSTS_CONNECTSPD_ENHANCED_SUPER:
        dwc->speed = USB_SPEED_SUPER;
        ep0_max_packet = 512;
        break;
    default:
        dprintf(ALWAYS, "dwc3_connection_done: unsupported speed %u\n", speed);
        dwc->speed = USB_SPEED_UNDEFINED;
        break;
    }

    mutex_release(&dwc->lock);

    if (ep0_max_packet) {
        dwc->eps[EP0_OUT].max_packet_size = ep0_max_packet;
        dwc->eps[EP0_IN].max_packet_size = ep0_max_packet;
        dwc3_cmd_ep_set_config(dwc, EP0_OUT, USB_ENDPOINT_CONTROL, ep0_max_packet, 0, true);
        dwc3_cmd_ep_set_config(dwc, EP0_IN, USB_ENDPOINT_CONTROL, ep0_max_packet, 0, true);
    }

//    usb_dci_set_speed(&dwc->dci_intf, dwc->speed);
}

void dwc3_set_address(dwc3_t* dwc, unsigned address) {
    volatile void* mmio = dwc3_mmio(dwc);
    mutex_acquire(&dwc->lock);
    DWC3_SET_BITS32(mmio + DCFG, DCFG_DEVADDR_START, DCFG_DEVADDR_BITS, address);
    mutex_release(&dwc->lock);
}

void dwc3_reset_configuration(dwc3_t* dwc) {
    volatile void* mmio = dwc3_mmio(dwc);

    mutex_acquire(&dwc->lock);

    // disable all endpoints except EP0_OUT and EP0_IN
    DWC3_WRITE32(mmio + DALEPENA, (1 << EP0_OUT) | (1 << EP0_IN));

    mutex_release(&dwc->lock);

    for (unsigned i = 2; i < countof(dwc->eps); i++) {
        dwc3_ep_end_transfers(dwc, i, ERR_OFFLINE);
        dwc3_ep_set_stall(dwc, i, false);
    }
}

/*
static void dwc3_request_queue(void* ctx, usb_request_t* req) {
    dwc3_t* dwc = ctx;

    dprintf(SPEW, "dwc3_request_queue ep: %u\n", req->header.ep_address);
    unsigned ep_num = dwc3_ep_num(req->header.ep_address);
    if (ep_num < 2 || ep_num >= countof(dwc->eps)) {
        dprintf(ALWAYS, "dwc3_request_queue: bad ep address 0x%02X\n", req->header.ep_address);
        usb_request_complete(req, ERR_INVALID_ARGS, 0);
        return;
    }

    dwc3_ep_queue(dwc, ep_num, req);
}

static status_t dwc3_config_ep(void* ctx, usb_endpoint_descriptor_t* ep_desc,
                                  usb_ss_ep_comp_descriptor_t* ss_comp_desc) {
    dwc3_t* dwc = ctx;
    return dwc3_ep_config(dwc, ep_desc, ss_comp_desc);
}

static status_t dwc3_disable_ep(void* ctx, uint8_t ep_addr) {
    dwc3_t* dwc = ctx;
    return dwc3_ep_disable(dwc, ep_addr);
}

static status_t dwc3_set_stall(void* ctx, uint8_t ep_address) {
    dwc3_t* dwc = ctx;
    return dwc3_ep_set_stall(dwc, dwc3_ep_num(ep_address), true);
}

static status_t dwc3_clear_stall(void* ctx, uint8_t ep_address) {
    dwc3_t* dwc = ctx;
    return dwc3_ep_set_stall(dwc, dwc3_ep_num(ep_address), false);
}

static status_t dwc3_set_mode(void* ctx, usb_mode_t mode) {
    dwc3_t* dwc = ctx;
    status_t status = NO_ERROR;

    if (mode == USB_MODE_OTG) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    if (dwc->usb_mode == mode) {
        return NO_ERROR;
    }

    // Shutdown if we are in device mode
    if (dwc->usb_mode == USB_MODE_DEVICE) {
        dwc3_events_stop(dwc);
        zx_handle_close(dwc->irq_handle);
        dwc->irq_handle = ZX_HANDLE_INVALID;
        dwc3_disconnected(dwc);
        dwc3_stop(dwc);
    }

    if (mode == USB_MODE_HOST) {
        dwc3_start_host_mode(dwc);
    }

    status = usb_mode_switch_set_mode(&dwc->ums, mode);
    if (status != NO_ERROR) {
        goto fail;
    }

    if (mode == USB_MODE_DEVICE) {
        status = pdev_map_interrupt(&dwc->pdev, IRQ_USB3, &dwc->irq_handle);
        if (status != NO_ERROR) {
            dprintf(ALWAYS, "dwc3_set_mode: pdev_map_interrupt failed\n");
            goto fail;
        }

        dwc3_start_peripheral_mode(dwc);
    }

    dwc->usb_mode = mode;
    return NO_ERROR;

fail:
    usb_mode_switch_set_mode(&dwc->ums, USB_MODE_NONE);
    dwc->usb_mode = USB_MODE_NONE;

    return status;
}

*/

static void dwc3_init(uint level) {
    dprintf(INFO, "dwc3_bind\n");

    dwc3_t* dwc = calloc(1, sizeof(dwc3_t));
    if (!dwc) {
        return;
    }

    mutex_init(&dwc->lock);

    for (unsigned i = 0; i < countof(dwc->eps); i++) {
        dwc3_endpoint_t* ep = &dwc->eps[i];
        ep->ep_num = i;
        mutex_init(&ep->lock);
        list_initialize(&ep->queued_reqs);
    }

    status_t status = io_buffer_init_physical(&dwc->mmio, DWC3_REG_BASE, DWC3_REG_LENGTH);
    if (status != NO_ERROR) {
        dprintf(ALWAYS, "dwc3_bind: io_buffer_init_physical failed\n");
        return;
    }

    status = io_buffer_init(&dwc->event_buffer, EVENT_BUFFER_SIZE);
    if (status != NO_ERROR) {
        dprintf(ALWAYS, "dwc3_bind: io_buffer_init failed\n");
        return;
    }

    status = io_buffer_init(&dwc->ep0_buffer, 65536);
    if (status != NO_ERROR) {
        dprintf(ALWAYS, "dwc3_bind: io_buffer_init failed\n");
        return;
    }

    status = dwc3_ep0_init(dwc);
    if (status != NO_ERROR) {
        dprintf(ALWAYS, "dwc3_bind: dwc3_ep_init failed\n");
        return;
    }
}

LK_INIT_HOOK(dwc3, dwc3_init, LK_INIT_LEVEL_THREADING);
