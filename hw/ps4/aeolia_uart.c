/*
 * QEMU model of the Aeolia UART block.
 *
 * Copyright (c) 2017 Alexandro Sanchez Bach
 *
 * Based on lm32_uart.c
 * Copyright (c) 2010 Michael Walle <michael@walle.cc>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "aeolia.h"
#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/sysbus.h"
#include "chardev/char-fe.h"
#include "qemu/error-report.h"

/* registers */
#define REG_RXTX  0
#define REG_IER   1
#define REG_IIR   2
#define REG_LCR   3
#define REG_MCR   4
#define REG_LSR   5
#define REG_MSR   6
#define REGS_MAX  7

/* flags */
#define IER_RBRI  (1<<0)
#define IER_THRI  (1<<1)
#define IER_RLSI  (1<<2)
#define IER_MSI   (1<<3)

#define IIR_STAT  (1<<0)
#define IIR_ID0   (1<<1)
#define IIR_ID1   (1<<2)

#define LCR_WLS0  (1<<0)
#define LCR_WLS1  (1<<1)
#define LCR_STB   (1<<2)
#define LCR_PEN   (1<<3)
#define LCR_EPS   (1<<4)
#define LCR_SP    (1<<5)
#define LCR_SB    (1<<6)

#define MCR_DTR   (1<<0)
#define MCR_RTS   (1<<1)

#define LSR_DR    (1<<0)
#define LSR_OE    (1<<1)
#define LSR_PE    (1<<2)
#define LSR_FE    (1<<3)
#define LSR_BI    (1<<4)
#define LSR_THRE  (1<<5)
#define LSR_TEMT  (1<<6)

#define MSR_DCTS  (1<<0)
#define MSR_DDSR  (1<<1)
#define MSR_TERI  (1<<2)
#define MSR_DDCD  (1<<3)
#define MSR_CTS   (1<<4)
#define MSR_DSR   (1<<5)
#define MSR_RI    (1<<6)
#define MSR_DCD   (1<<7)

#define AEOLIA_UART(obj) OBJECT_CHECK(AeoliaUartState, (obj), TYPE_AEOLIA_UART)

struct AeoliaUartState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq;

    uint32_t regs[REGS_MAX];
};
typedef struct AeoliaUartState AeoliaUartState;

static void uart_update_irq(AeoliaUartState *s)
{
    unsigned int irq;

    if ((s->regs[REG_LSR] & (LSR_OE | LSR_PE | LSR_FE | LSR_BI))
            && (s->regs[REG_IER] & IER_RLSI)) {
        irq = 1;
        s->regs[REG_IIR] = IIR_ID1 | IIR_ID0;
    } else if ((s->regs[REG_LSR] & LSR_DR) && (s->regs[REG_IER] & IER_RBRI)) {
        irq = 1;
        s->regs[REG_IIR] = IIR_ID1;
    } else if ((s->regs[REG_LSR] & LSR_THRE) && (s->regs[REG_IER] & IER_THRI)) {
        irq = 1;
        s->regs[REG_IIR] = IIR_ID0;
    } else if ((s->regs[REG_MSR] & 0x0f) && (s->regs[REG_IER] & IER_MSI)) {
        irq = 1;
        s->regs[REG_IIR] = 0;
    } else {
        irq = 0;
        s->regs[REG_IIR] = IIR_STAT;
    }

    qemu_set_irq(s->irq, irq);
}

static uint64_t uart_read(void *opaque, hwaddr addr,
                          unsigned size)
{
    AeoliaUartState *s = opaque;
    uint32_t r = 0;

    addr >>= 2;
    switch (addr) {
    case REG_RXTX:
        r = 0; // TODO: getc(stdin);
        s->regs[REG_LSR] &= ~LSR_DR;
        uart_update_irq(s);
        break;
    case REG_IIR:
    case REG_LSR:
    case REG_MSR:
        r = s->regs[addr];
        break;
    case REG_IER:
    case REG_MCR:
        warn_report("aeolia_uart: read access to unimplemented register 0x"
                TARGET_FMT_plx, addr << 2);
        break;
    case REG_LCR:
        error_report("aeolia_uart: read access to write only register 0x"
                TARGET_FMT_plx, addr << 2);
        break;
    default:
        error_report("aeolia_uart: read access to unknown register 0x"
                TARGET_FMT_plx, addr << 2);
        break;
    }

    return r;
}

static void uart_write(void *opaque, hwaddr addr,
                       uint64_t value, unsigned size)
{
    AeoliaUartState *s = opaque;
    unsigned char ch = value;

    addr >>= 2;
    switch (addr) {
    case REG_RXTX:
        putc(ch, stdout);
        break;
    case REG_IER:
    case REG_LCR:
    case REG_MCR:
        s->regs[addr] = value;
        break;
    case REG_IIR:
        warn_report("aeolia_uart: write access to unimplemented register 0x"
                TARGET_FMT_plx, addr << 2);
        break;
    case REG_LSR:
    case REG_MSR:
        error_report("aeolia_uart: write access to read only register 0x"
                TARGET_FMT_plx, addr << 2);
        break;
    default:
        error_report("aeolia_uart: write access to unknown register 0x"
                TARGET_FMT_plx, addr << 2);
        break;
    }
    uart_update_irq(s);
}

static const MemoryRegionOps uart_ops = {
    .read = uart_read,
    .write = uart_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void uart_reset(DeviceState *d)
{
    AeoliaUartState *s = AEOLIA_UART(d);
    int i;

    for (i = 0; i < REGS_MAX; i++) {
        s->regs[i] = 0;
    }

    /* defaults */
    s->regs[REG_LSR] = LSR_THRE | LSR_TEMT;
}

static void aeolia_uart_init(Object *obj)
{
    AeoliaUartState *s = AEOLIA_UART(obj);
    SysBusDevice *dev = SYS_BUS_DEVICE(obj);

    sysbus_init_irq(dev, &s->irq);

    memory_region_init_io(&s->iomem, obj, &uart_ops, s,
                          "uart", REGS_MAX * 4);
    sysbus_init_mmio(dev, &s->iomem);
}

static const VMStateDescription vmstate_aeolia_uart = {
    .name = "aeolia-uart",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, AeoliaUartState, REGS_MAX),
        VMSTATE_END_OF_LIST()
    }
};

static Property aeolia_uart_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void aeolia_uart_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = uart_reset;
    dc->vmsd = &vmstate_aeolia_uart;
    dc->props = aeolia_uart_properties;
}

static const TypeInfo aeolia_uart_info = {
    .name          = TYPE_AEOLIA_UART,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AeoliaUartState),
    .instance_init = aeolia_uart_init,
    .class_init    = aeolia_uart_class_init,
};

static void aeolia_uart_register_types(void)
{
    type_register_static(&aeolia_uart_info);
}

type_init(aeolia_uart_register_types);
