#include "sys_config.h"
#include "typesdef.h"
#include "list.h"
#include "dev.h"
#include "devid.h"
#include "osal/string.h"
#include "osal/mutex.h"
#include "osal/semaphore.h"
#include "osal/task.h"
#include "osal/timer.h"
#include "hal/sdio_slave.h"
#include "hal/gpio.h"
#include "lib/skb/skb.h"
#include "lib/bus/macbus/mac_bus.h"
#include "lib/lmac/hgic.h"

extern int32 _os_task_set_priority(struct os_task *task, uint8 priority);
#define _OS_TASK_INIT(name, task, func, data, prio, stksize) do { \
        os_task_init((const uint8 *)name, task, (os_task_func_t)func, (uint32)data); \
        os_task_set_stacksize(task, stksize); \
        _os_task_set_priority(task, prio); \
        os_task_run(task);\
    }while(0)

struct mac_bus_sdio {
    struct mac_bus     bus;
    struct sdio_slave *dev;
    uint8  cur_id;
    uint8  ready: 1, pending: 1, resv: 5;
    uint16 part_len;
    uint16 pkt_len;

    struct os_task      rxtask;
    struct os_semaphore rxsema;
    uint8 *rx_buf[2];
    uint16 rx_cnt[2];
};

static void sdio_bus_set_busy(struct mac_bus_sdio *sdio_bus, uint32 busy)
{
    //gpio_set_val(PA_8, busy);
    sdio_bus->pending = busy;
    sdio_slave_ioctl(sdio_bus->dev, SDIO_SLAVE_IO_CMD_SET_USER_REG1, busy);
}

static int32 sdio_bus_update_rxbuff(struct mac_bus_sdio *sdio_bus)
{
    int8 id = !sdio_bus->cur_id;
    if (sdio_bus->rx_cnt[id] == 0) {
        sdio_bus->cur_id = id;
        return 1;
    }
    return 0;
}

static int32 sdio_bus_proc_rxbuff(struct mac_bus_sdio *sdio_bus, uint8 id)
{
    int32 ret = 0;
    if (id != 0xff){
        ret = sdio_bus->bus.recv(&sdio_bus->bus, sdio_bus->rx_buf[id], sdio_bus->rx_cnt[id]);
        if (ret != -ENOMEM) {
            sdio_bus->rx_cnt[id] = 0;
            if (ret) sdio_bus->bus.rxerr++; 
        }
    }
    return ret;
}

static void sdio_bus_rx_task(struct mac_bus_sdio *sdio_bus)
{
    uint32 flags;
    int32  ret = 0;
    uint8  id1, id2;

    while (1) {
        os_sema_down(&sdio_bus->rxsema, osWaitForever);

        flags = disable_irq();
        id1   = !sdio_bus->cur_id;
        id2   = sdio_bus->cur_id;
        if (sdio_bus->rx_cnt[id1] == 0) id1 = 0xff;
        if (sdio_bus->rx_cnt[id2] == 0) id2 = 0xff;
        enable_irq(flags);

        ret = sdio_bus_proc_rxbuff(sdio_bus, id1);
        if (ret == -ENOMEM) {
            continue;
        }

        sdio_bus_proc_rxbuff(sdio_bus, id2);
        flags = disable_irq();
        if (sdio_bus->pending && sdio_bus_update_rxbuff(sdio_bus)) {
            sdio_slave_read(sdio_bus->dev, sdio_bus->rx_buf[sdio_bus->cur_id], sdio_bus->pkt_len, FALSE);
            sdio_bus_set_busy(sdio_bus, 0);
        }
        enable_irq(flags);
    }
}

static int32 sdio_bus_write(struct mac_bus *bus, uint8 *buff, int32 len)
{
    int32 ret = RET_OK;
    struct mac_bus_sdio *sdio_bus = container_of(bus, struct mac_bus_sdio, bus);

    if (!sdio_bus->ready) {
        sdio_bus->bus.txerr++;
        return RET_ERR;
    }
    ret = sdio_slave_write(sdio_bus->dev, buff, ALIGN(len, 4), TRUE);
    if (ret) {
        sdio_bus->bus.txerr++;
        sdio_bus->ready = 0;
    }
    return ret;
}

static int32 sdio_bus_write_scatter(struct mac_bus *bus, scatter_data *data, int count)
{
    int32 ret = RET_OK;
    struct mac_bus_sdio *sdio_bus = container_of(bus, struct mac_bus_sdio, bus);

    if (!sdio_bus->ready) {
        sdio_bus->bus.txerr++;
        return RET_ERR;
    }

    ret = sdio_slave_write_scatter(sdio_bus->dev, data, count, TRUE);
    if (ret) {
        sdio_bus->bus.txerr++;
        sdio_bus->ready = 0;
    }
    return ret;
}

static void sdio_bus_irq(uint32 irq, uint32 param1, uint32 param2, uint32 param3)
{
    struct mac_bus_sdio *sdio_bus = (struct mac_bus_sdio *)param1;
    //gpio_set_val(1, 1);
    switch (irq) {
        case SDIO_SLAVE_IRQ_RX:
            sdio_bus->ready = 1;
#ifdef SDIO_PART_TRANS
            if (sdio_bus->part_len == 0) {
                struct hgic_frm_hdr2 *hdr = (struct hgic_frm_hdr2 *)sdio_bus->rx_buf[sdio_bus->cur_id];
                if (hdr->hdr.magic == HGIC_HDR_TX_MAGIC && param3 < hdr->hdr.length) {
                    sdio_bus->part_len = param3;
                    break;
                }
            } else {
                param3 += sdio_bus->part_len;
            }
            sdio_bus->part_len = 0;
#endif
            if (sdio_bus->pending) {
                sdio_bus->bus.rxerr++;
                os_printf("*PENDING*:%d\r\n", param3);
            }

            sdio_bus->rx_cnt[sdio_bus->cur_id] = param3;
            if (sdio_bus_update_rxbuff(sdio_bus)) {
                sdio_bus_set_busy(sdio_bus, 0);
            } else {
                sdio_bus_set_busy(sdio_bus, 1);
            }
            sdio_slave_read(sdio_bus->dev, sdio_bus->rx_buf[sdio_bus->cur_id], sdio_bus->pkt_len, FALSE);
            os_sema_up(&sdio_bus->rxsema);
            break;
        case SDIO_SLAVE_IRQ_RESET:
            sdio_bus_set_busy(sdio_bus, sdio_bus->pending);
            sdio_slave_read(sdio_bus->dev, sdio_bus->rx_buf[sdio_bus->cur_id], sdio_bus->pkt_len, FALSE);
            //gpio_set_dir(PC_1, GPIO_DIR_OUTPUT);
            //gpio_set_dir(PC_2, GPIO_DIR_OUTPUT);
            //gpio_set_dir(PA_8, GPIO_DIR_OUTPUT);
            break;
        default:
            break;
    }
    //gpio_set_val(1, 0);
}

static int32 sdio_bus_ioctl(struct mac_bus *bus, uint32 cmd, uint32 param1, uint32 param2)
{
    int32 ret = -ENOTSUPP;
    struct mac_bus_sdio *sdio_bus = container_of(bus, struct mac_bus_sdio, bus);
    switch(cmd){
        case MACBUS_IOCTL_BUFFER_IDLE:
            if (sdio_bus->pending) {
                os_sema_up(&sdio_bus->rxsema);
            }
        default:
            break;
    }
    return ret;
}

__init struct mac_bus *mac_bus_sdio_attach(mac_bus_recv recv, void *priv, uint16 drv_aggsize)
{
    int32  ret = RET_OK;
    uint8 *buf = NULL;
    struct mac_bus_sdio *sdio_bus;
    struct sdio_slave *sdio = (struct sdio_slave *)dev_get(HG_SDIOSLAVE_DEVID);

    if (sdio == NULL) {
        return NULL;
    }

    sdio_bus = os_zalloc(sizeof(struct mac_bus_sdio));
    ASSERT(sdio_bus && recv);
    sdio_bus->bus.write = sdio_bus_write;
#ifdef DMA_SCATTER_COUNT
    sdio_bus->bus.write_scatter = sdio_bus_write_scatter;
#endif
    sdio_bus->bus.ioctl = sdio_bus_ioctl;
    sdio_bus->bus.type  = MAC_BUS_TYPE_SDIO;
    sdio_bus->bus.recv  = recv;
    sdio_bus->bus.priv  = priv;
    sdio_bus->dev       = sdio;
    sdio_bus->pkt_len   = drv_aggsize > 2048 ? drv_aggsize : 2048;
    buf = os_malloc(2 * sdio_bus->pkt_len);
    ASSERT(buf);
    sdio_bus->rx_buf[0] = buf;
    sdio_bus->rx_buf[1] = buf + sdio_bus->pkt_len;
    os_sema_init(&sdio_bus->rxsema, 0);
    ret = sdio_slave_open(sdio_bus->dev, SDIO_SLAVE_SPEED_48M, 0); //SDIO_SLAVE_SPEED_4M
    sdio_slave_read(sdio_bus->dev, sdio_bus->rx_buf[sdio_bus->cur_id], sdio_bus->pkt_len, FALSE);
    sdio_slave_request_irq(sdio_bus->dev, sdio_bus_irq, (uint32)sdio_bus);
    _OS_TASK_INIT("sdiobus", &sdio_bus->rxtask, sdio_bus_rx_task, sdio_bus, OS_TASK_PRIORITY_HIGH+7, 512);
    os_printf("SDIO bus init done, ret = %d\r\n", ret);
    //gpio_set_dir(PC_1, GPIO_DIR_OUTPUT);
    //gpio_set_dir(PC_2, GPIO_DIR_OUTPUT);
    //gpio_set_dir(PA_8, GPIO_DIR_OUTPUT);
    return &sdio_bus->bus;
}

void mac_bus_sdio_detach(struct mac_bus *bus)
{
    struct sdio_slave *sdio = (struct sdio_slave *)dev_get(HG_SDIOSLAVE_DEVID);
    if (sdio) {
        sdio_slave_close(sdio);
    }
}


