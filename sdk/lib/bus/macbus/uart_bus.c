// @file    uart_bus.c
// @author
// @brief

// Revision History
// V1.0.0  06/28/2019  First Release
// V1.0.1  08/06/2019  use uart1 when 40pin
// V1.0.2  10/28/2019  modify uart_bus_irq_hdl

#include "tx_platform.h"
#include "typesdef.h"
#include "list.h"
#include "dev.h"
#include "devid.h"
#include "osal/string.h"
#include "osal/mutex.h"
#include "osal/semaphore.h"
#include "osal/mutex.h"
#include "osal/task.h"
#include "osal/timer.h"
#include "osal/work.h"
#include "hal/uart.h"
#include "hal/gpio.h"
#include "hal/timer_device.h"
#include "hal/dma.h"
#include "lib/skb/skb.h"
#include "lib/lmac/hgic.h"
#include "lib/bus/macbus/mac_bus.h"
#include "lib/common/rbuffer.h"
#include "lib/misc/drivercmd.h"
#include "lib/common/ticker_api.h"
#include "lib/syscfg/syscfg.h"
#include "lib/lmac/hgic.h"
#include "lib/net/utils.h"
#include "lib/umac/wifi_mgr.h"

#include "syscfg.h"

#define UART_BUS_RX_BUF_SIZE (4096)

struct mac_bus_uart {
    struct mac_bus bus;
    struct uart_device *uart;
    struct os_task task;
    struct os_semaphore sema;
    struct os_mutex lock;
    struct dma_device *dma;
    struct timer_device *timer;
    uint8  rxbuff[UART_BUS_RX_BUF_SIZE];
    RBUFFER_DEF_R(rb, uint8);
    uint32 rxcount;
    uint32 stop_pos;
    uint16 magic;
    uint16 frm_len;
    uint16 fixlen;
    uint8  rx_chn;
    struct wifimgr_submod smod;
};

#ifdef MACBUS_UART
static void uart_bus_task(void *args)
{
    int32  count;
    int32  part;
    uint8 *buf;
    struct mac_bus_uart *uart_bus = (struct mac_bus_uart *)args;

    while (1) {
        os_sema_down(&uart_bus->sema, osWaitForever);
        count = (uart_bus->stop_pos >= uart_bus->rb.rpos) ?
                (uart_bus->stop_pos - uart_bus->rb.rpos) :
                (uart_bus->rb.qsize - uart_bus->rb.rpos + uart_bus->stop_pos);
        if (count > 0) {
            buf = os_malloc(count);
            if (buf) {
                if (uart_bus->stop_pos > uart_bus->rb.rpos) {
                    hw_memcpy(buf, uart_bus->rxbuff + uart_bus->rb.rpos, count);
                } else {
                    part = uart_bus->rb.qsize - uart_bus->rb.rpos;
                    hw_memcpy(buf, uart_bus->rxbuff + uart_bus->rb.rpos, part);
                    hw_memcpy(buf + part, uart_bus->rxbuff, uart_bus->stop_pos);
                }
                uart_bus->bus.recv(&uart_bus->bus, buf, count);
                os_free(buf);
            }
            uart_bus->rb.rpos = RB_NPOS(&uart_bus->rb, rpos, count);
        }
    }
}

static void uart_bus_timer_cb(uint32 args, uint32 flags)
{
    struct mac_bus_uart *uart_bus = (struct mac_bus_uart *)args;
    uart_bus->stop_pos = uart_bus->rb.wpos;
    uart_bus->rxcount = 0;
    uart_bus->frm_len = 0;
    uart_bus->magic   = 0;
    os_sema_up(&uart_bus->sema);
}

static int32 uart_bus_irq_hdl(uint32 irq_flag, uint32 irq_data, uint32 param1, uint32 param2)
{
    struct mac_bus_uart *uart_bus = (struct mac_bus_uart *)irq_data;

    switch (irq_flag) {
        case UART_IRQ_FLAG_RX_BYTE:
            if (uart_bus->fixlen > 0) {
                uart_bus->bus.recv(&uart_bus->bus, uart_bus->rxbuff, uart_bus->fixlen);
                uart_bus->rx_chn = uart_gets(uart_bus->uart, uart_bus->rxbuff, uart_bus->fixlen);
            } else {
                timer_device_stop(uart_bus->timer);
                RB_SET(&uart_bus->rb, (uint8)param1);
                uart_bus->rxcount++;
                if (uart_bus->rxcount <= 2) {
                    uart_bus->magic >>= 8;
                    uart_bus->magic |= (uint8)param1 << 8;
                } else if (uart_bus->rxcount == 5 || uart_bus->rxcount == 6) {
                    uart_bus->frm_len >>= 8;
                    uart_bus->frm_len |= (uint8)param1 << 8;
                }

                if ((uart_bus->magic == 0x1A2B && uart_bus->frm_len && uart_bus->rxcount >= uart_bus->frm_len) ||
                    (RB_COUNT(&uart_bus->rb) >= UART_BUS_RX_BUF_SIZE - 1)) {
                    uart_bus->stop_pos = uart_bus->rb.wpos;
                    uart_bus->rxcount = 0;
                    uart_bus->frm_len = 0;
                    uart_bus->magic   = 0;
                    os_sema_up(&uart_bus->sema);
                }
                timer_device_start(uart_bus->timer, 5000, uart_bus_timer_cb, (uint32)uart_bus);
            }
            break;
        default:
            break;
    }
    return 0;
}

static int uart_bus_write(struct mac_bus *bus, uint8 *buff, int len)
{
    struct mac_bus_uart *uart_bus = container_of(bus, struct mac_bus_uart, bus);
    ASSERT(uart_bus->uart);
    os_mutex_lock(&uart_bus->lock, osWaitForever);
    uart_puts(uart_bus->uart, buff, len);
    delay_us(10*24*1000000/UARTBUS_DEV_BAUDRATE); // 24Bytes
    os_mutex_unlock(&uart_bus->lock);
    return RET_OK;
}

static int32 uart_device_init(struct mac_bus_uart *uart_bus)
{
    uart_close(uart_bus->uart);
    delay_us(100); // very important!
    uart_open(uart_bus->uart, UARTBUS_DEV_BAUDRATE);
    dma_stop(uart_bus->dma, uart_bus->rx_chn);
    if (uart_bus->fixlen > 0) {
        uart_bus->rx_chn = uart_gets(uart_bus->uart, uart_bus->rxbuff, uart_bus->fixlen);
    }
    uart_request_irq(uart_bus->uart, uart_bus_irq_hdl, UART_IRQ_FLAG_RX_BYTE, (uint32)uart_bus); // request irq at last
    return RET_OK;
}

static int32 uart_bus_set_fixlen(struct mac_bus_uart *uart_bus, uint16 fixlen)
{
    if (uart_bus->fixlen != fixlen) {
        sys_cfgs.uart_fixlen = fixlen;
        syscfg_save();
        uart_bus->fixlen = fixlen; // change fixlen after write flash
        uart_device_init(uart_bus);
    }
    os_printf("uart bus fixlen set %d\r\n", uart_bus->fixlen);
    return RET_OK;
}

static int32 uart_bus_proc_cmd(struct wifimgr_submod *smod, struct sk_buff **skb)
{
    int32 ret = -ENOTSUPP;
    uint8 *data = NULL;
    uint32 cmd_id = 0;
    struct sk_buff *nskb;
    struct hgic_ctrl_hdr *ctrl = (struct hgic_ctrl_hdr *)((*skb)->data);
    struct mac_bus_uart *uart_bus = container_of(smod, struct mac_bus_uart, smod);

    cmd_id  = HDR_CMDID(ctrl);
    data    = (uint8 *)(ctrl + 1);
    switch (cmd_id) {
        case HGIC_CMD_SET_UART_FIXLEN:
            ret = uart_bus_set_fixlen(uart_bus, get_unaligned_le16(data));
            break;
        case HGIC_CMD_GET_UART_FIXLEN:
            nskb = wifi_mgr_alloc_resp(*skb, (int8 *)&uart_bus->fixlen, 2);
            if (nskb) {
                kfree_skb(*skb);
                *skb = nskb;
                ret = 2;
            } else {
                ret = -ENOMEM;
            }
            break;
        default:
            break;
    }
    return ret;
}
static int uart_bus_ioctl(struct mac_bus *bus, uint32 cmd, uint32 param1, uint32 param2)
{
    return -ENOTSUPP;
}


/**
 * @brief UART mac bus initialization
 *
 * @param recv  Mac bus recv function
 * @param priv  UART mac bus
 * @return __init
 *
 */
__init struct mac_bus *mac_bus_uart_attach(mac_bus_recv recv, void *priv, uint16 drv_aggsize)
{
    struct mac_bus_uart *uart_bus = NULL;
    int32 ret;

    uart_bus = os_zalloc(sizeof(struct mac_bus_uart));
    ASSERT(uart_bus && recv);
    uart_bus->bus.write = uart_bus_write;
    uart_bus->bus.ioctl = uart_bus_ioctl;
    uart_bus->bus.type  = MAC_BUS_TYPE_UART;
    uart_bus->bus.recv  = recv;
    uart_bus->bus.priv  = priv;
#if MACBUS_UART_FIXLEN
    uart_bus->fixlen    = MACBUS_UART_FIXLEN;
#else
    uart_bus->fixlen    = (sys_cfgs.uart_fixlen > UART_BUS_RX_BUF_SIZE) ? UART_BUS_RX_BUF_SIZE : sys_cfgs.uart_fixlen;
#endif
    RB_INIT_R(&uart_bus->rb, UART_BUS_RX_BUF_SIZE, uart_bus->rxbuff);
    os_printf("uart bus fixlen=%d\r\n", uart_bus->fixlen);
    uart_bus->timer = (struct timer_device *)dev_get(UART_BUS_TIMER);
    ASSERT(uart_bus->timer);
    timer_device_open(uart_bus->timer, TIMER_TYPE_ONCE, 0);
    uart_bus->uart = (struct uart_device *)dev_get(UARTBUS_DEV);
    ASSERT(uart_bus->uart);
    uart_bus->dma = (struct dma_device *)dev_get(HG_DMAC_DEVID);
    ASSERT(uart_bus->dma);
    uart_ioctl(uart_bus->uart, UART_IOCTL_CMD_USE_DMA, (uint32)uart_bus->dma, 0);
    os_sema_init(&uart_bus->sema, 0);
    os_mutex_init(&uart_bus->lock);
    OS_TASK_INIT("uart_bus", &uart_bus->task, uart_bus_task, uart_bus, OS_TASK_PRIORITY_NORMAL, 512);
    ret = uart_device_init(uart_bus);
    ASSERT(!ret);
    uart_bus->smod.name     = "uart";
    uart_bus->smod.type     = WIFI_MGR_STYPE_NONE;
    uart_bus->smod.cmd_proc = uart_bus_proc_cmd;
    wifi_mgr_add_submod(&uart_bus->smod);
    return &uart_bus->bus;
}
#endif

