// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <reg.h>
#include <dev/gpio.h>
#include <kernel/thread.h>
#include <kernel/vm.h>
#include <platform/hi3660-hw.h>
#include <platform/hi3660-regs.h>
#include <platform/hikey960-hw.h>

#include <stdio.h>

void hi3660_usb_init(void) {
    volatile void* usb3otg_bc = paddr_to_kvaddr(MMIO_USB3OTG_BC_BASE);
    volatile void* peri_crg = paddr_to_kvaddr(MMIO_PERI_CRG_BASE);
    volatile void* pctrl = paddr_to_kvaddr(MMIO_PCTRL_BASE);
    uint32_t temp;

    writel(PERI_CRG_ISODIS_REFCLK_ISO_EN, peri_crg + PERI_CRG_ISODIS);
    writel(PCTRL_CTRL3_USB_TCXO_EN | (PCTRL_CTRL3_USB_TCXO_EN << PCTRL_CTRL3_MSK_START),
           pctrl + PCTRL_CTRL3);

    temp = readl(pctrl + PCTRL_CTRL24);
    temp &= ~PCTRL_CTRL24_SC_CLK_USB3PHY_3MUX1_SEL;
    writel(temp, pctrl + PCTRL_CTRL24);

    writel(PERI_CRG_GT_CLK_USB3OTG_REF | PERI_CRG_GT_ACLK_USB3OTG, peri_crg + PERI_CRG_CLK_EN4);
    writel(PERI_CRG_IP_RST_USB3OTG_MUX | PERI_CRG_IP_RST_USB3OTG_AHBIF
           | PERI_CRG_IP_RST_USB3OTG_32K,  peri_crg + PERI_CRG_RSTDIS4);

    writel(PERI_CRG_IP_RST_USB3OTGPHY_POR | PERI_CRG_IP_RST_USB3OTG, peri_crg + PERI_CRG_RSTEN4);

    // enable PHY REF CLK
    temp = readl(usb3otg_bc + USB3OTG_CTRL0);
    temp |= USB3OTG_CTRL0_ABB_GT_EN;
    writel(temp, usb3otg_bc + USB3OTG_CTRL0);

    temp = readl(usb3otg_bc + USB3OTG_CTRL7);
    temp |= USB3OTG_CTRL7_REF_SSP_EN;
    writel(temp, usb3otg_bc + USB3OTG_CTRL7);

    // exit from IDDQ mode
    temp = readl(usb3otg_bc + USB3OTG_CTRL2);
    temp &= ~(USB3OTG_CTRL2_POWERDOWN_HSP | USB3OTG_CTRL2_POWERDOWN_SSP);
    writel(temp, usb3otg_bc + USB3OTG_CTRL2);
    thread_sleep(1);

    writel(PERI_CRG_IP_RST_USB3OTGPHY_POR, peri_crg + PERI_CRG_RSTDIS4);
    writel(PERI_CRG_IP_RST_USB3OTG, peri_crg + PERI_CRG_RSTDIS4);
    thread_sleep(20);

    temp = readl(usb3otg_bc + USB3OTG_CTRL3);
    temp |= (USB3OTG_CTRL3_VBUSVLDEXT | USB3OTG_CTRL3_VBUSVLDEXTSEL);
    writel(temp, usb3otg_bc + USB3OTG_CTRL3);
    thread_sleep(1);

    gpio_config(GPIO_HUB_VDD33_EN, GPIO_OUTPUT);
    gpio_config(GPIO_VBUS_TYPEC, GPIO_OUTPUT);
    gpio_config(GPIO_USBSW_SW_SEL, GPIO_OUTPUT);

    // these are set 0 for device, 1 for host
    gpio_set(GPIO_HUB_VDD33_EN, 0);
    gpio_set(GPIO_VBUS_TYPEC, 0);
    gpio_set(GPIO_USBSW_SW_SEL, 0);
}
