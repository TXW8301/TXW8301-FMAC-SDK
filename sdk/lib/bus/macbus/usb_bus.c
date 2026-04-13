
#include "typesdef.h"
#include "list.h"
#include "dev.h"
#include "devid.h"
#include "osal/string.h"
#include "osal/mutex.h"
#include "osal/task.h"
#include "osal/semaphore.h"
#include "osal/timer.h"
#include "osal/work.h"
#include "hal/usb_device.h"
#include "lib/common/sysevt.h"
#include "lib/skb/skb.h"
#include "lib/bus/macbus/mac_bus.h"
#include "lib/usb/usb_device_wifi.h"
#include "dev/usb/hgusb11_dev_api.h"
#include "dev/usb/hgusb11_dev_tbl.h"

#if ((defined (TXW4002ACK803)) || (defined (TXW4002A_M3)))
#define USB_BUS_RXBUFF_SIZE (4096)
#else
#define USB_BUS_RXBUFF_SIZE (2048)
#endif

struct mac_bus_usb {
    struct mac_bus     bus;
    struct usb_device *dev;
    /* address need 4byte aligned */
    uint8              rxbuff[USB_BUS_RXBUFF_SIZE];
    atomic8_t          pending;
    uint8              ready: 1,  resv: 7;
    uint16             rxcount;
    uint64             pd_tick;
    struct  os_work    pd_work;
};

static void usb_bus_proc_pending_data(struct mac_bus_usb *usb_bus)
{
    uint8 pd;
    uint8 drop;
    int32 ret_val;

    pd = atomic_dec2_return(&usb_bus->pending);
    if (pd) {
        drop = TIME_AFTER(os_jiffies(), usb_bus->pd_tick + 100);
        ret_val = usb_bus->bus.recv(&usb_bus->bus, usb_bus->rxbuff, usb_bus->rxcount);
        if (ret_val) {
            usb_bus->bus.rxerr++;
            if (!drop && ret_val == -ENOMEM) {
                atomic_set(&usb_bus->pending, 1);
                return;
            }
        }
        usb_bus->rxcount = 0;
        usb_device_wifi_read(usb_bus->dev, usb_bus->rxbuff, USB_BUS_RXBUFF_SIZE);
    }
}

static int32 usb_bus_pading(uint8 *buff, int32 len)
{
    uint8 pad = (uint32)buff & 0x03;
    if (pad) {
        os_memset(buff - pad, 0xff, pad);
    }
    return pad;
}

static int32 usb_bus_write(struct mac_bus *bus, uint8 *buff, int32 len)
{
    int32 ret = RET_OK;
    uint8 pad = 0;
    struct mac_bus_usb *usb_bus = container_of(bus, struct mac_bus_usb, bus);

    pad   = usb_bus_pading(buff, len);
    buff -= pad;
    len  += pad;

    if (!usb_bus->ready) {
        usb_bus->bus.txerr++;
        return RET_ERR;
    }
    ret = usb_device_wifi_write(usb_bus->dev, buff, len);
    if (ret) {
        usb_bus->bus.txerr++;
        usb_bus->ready = 0;
    }
    return ret;
}

static int32 usb_bus_write_scatter(struct mac_bus *bus, scatter_data *data, int count)
{
    int32 ret = RET_OK;
    struct mac_bus_usb *usb_bus = container_of(bus, struct mac_bus_usb, bus);

    if (!usb_bus->ready) {
        usb_bus->bus.txerr++;
        return RET_ERR;
    }
    ret = usb_device_wifi_write_scatter(usb_bus->dev, data, count);
    if (ret) {
        usb_bus->bus.txerr++;
        usb_bus->ready = 0;
    }
    return ret;
}

static uint32 usb_bus_irq(uint32 irq, uint32 param1, uint32 param2, uint32 param3)
{
    struct mac_bus_usb *usb_bus = (struct mac_bus_usb *)param1;
    int32 ret_val = 1;
    switch (irq) {
        case USB_DEV_RESET_IRQ:
            usb_bus->pending.counter = 0;
            usb_device_wifi_read(usb_bus->dev, usb_bus->rxbuff, USB_BUS_RXBUFF_SIZE);
            break;
        case USB_DEV_SUSPEND_IRQ:
            break;
        case USB_DEV_RESUME_IRQ:
            break;
        case USB_DEV_SOF_IRQ:
            break;
        case USB_DEV_CTL_IRQ:
            ret_val = 0;
            break;
        case USB_EP_RX_IRQ:
            usb_bus->pending.counter = 0;
            usb_bus->ready   = 1;
            usb_bus->rxcount = param3;
            if (param3 > USB_BUS_RXBUFF_SIZE) {
                os_printf("usb rxbuff over, %d\r\n", param3);
                usb_bus->bus.rxerr++;
            } else {
                ret_val = usb_bus->bus.recv(&usb_bus->bus, usb_bus->rxbuff, param3);
                if (ret_val) {
                    usb_bus->bus.rxerr++;
                    if (ret_val == -ENOMEM) {
                        usb_bus->pending.counter = 1;
                        usb_bus->pd_tick = os_jiffies();
                        break;
                    }
                }
            }
            usb_device_wifi_read(usb_bus->dev, usb_bus->rxbuff, USB_BUS_RXBUFF_SIZE);
            usb_bus->rxcount = 0;
            break;
        case USB_EP_TX_IRQ:
            break;
        default:
            break;
    }
    return ret_val;
}

static int32 usb_bus_ioctl(struct mac_bus *bus, uint32 cmd, uint32 param1, uint32 param2)
{
    int32 ret = -ENOTSUPP;
    struct mac_bus_usb *usb_bus = container_of(bus, struct mac_bus_usb, bus);
    switch (cmd) {
        case MACBUS_IOCTL_BUFFER_IDLE:
            usb_bus_proc_pending_data(usb_bus);
        default:
            break;
    }
    return ret;
}

static int32 usb_bus_pdwork(struct os_work *work)
{
    struct mac_bus_usb *usb_bus = container_of(work,  struct mac_bus_usb, pd_work);
    usb_bus_proc_pending_data(usb_bus);
    os_run_work_delay(&usb_bus->pd_work, 50);
	return 0;
}

__init struct mac_bus *mac_bus_usb_attach(mac_bus_recv recv, void *priv, uint16 drv_aggsize)
{
    struct mac_bus_usb *usb_bus;
    struct usb_device *usb = (struct usb_device *)dev_get(HG_USBDEV_DEVID);

    if (usb == NULL) {
        return NULL;
    }
    usb_bus = os_zalloc(sizeof(struct mac_bus_usb));
    ASSERT(usb_bus && recv);
    usb_bus->bus.write = usb_bus_write;
#ifdef DMA_SCATTER_COUNT
    usb_bus->bus.write_scatter = usb_bus_write_scatter;
#endif
    usb_bus->bus.ioctl = usb_bus_ioctl;
    usb_bus->bus.type  = MAC_BUS_TYPE_USB;
    usb_bus->bus.recv  = recv;
    usb_bus->bus.priv  = priv;
    usb_bus->dev = usb;
    usb_device_wifi_open(usb_bus->dev);
    usb_device_wifi_auto_tx_null_pkt_enable(usb_bus->dev);
    usb_device_request_irq(usb_bus->dev, usb_bus_irq, (uint32)usb_bus);
    OS_WORK_INIT(&usb_bus->pd_work, usb_bus_pdwork, 15);
    os_run_work_delay(&usb_bus->pd_work, 50);
    os_printf("USB bus init done\r\n");
    return &usb_bus->bus;
}

void mac_bus_usb_detach(struct mac_bus *bus)
{
    struct mac_bus_usb *usb_bus;
    if (bus) {
        usb_bus = container_of(bus, struct mac_bus_usb, bus);
        usb_device_wifi_auto_tx_null_pkt_disable(usb_bus->dev);
        usb_device_wifi_close(usb_bus->dev);
        os_free(usb_bus);
    }
}

