#include "basic_include.h"
#include "lib/umac/ieee80211.h"
#include "lib/umac/wifi_mgr.h"
#include "lib/umac/wifi_cfg.h"
#include "syscfg.h"

void sys_wakeup_host(void)
{
    if (sys_cfgs.wkio_mode == 0) {
        gpio_set_val(PB_0, 1);
        os_sleep_ms(2);
        gpio_set_val(PB_0, 0);
    } else {
        gpio_set_val(PB_0, 1);
    }
    os_printf(KERN_NOTICE"wakeup host ...\r\n");
}

void sys_check_wkreason(uint8 type)
{
#ifdef CONFIG_SLEEP
    uint8 i;
    uint8 wk = 0;
    uint8 wkreason = 0;

    wkreason = ieee80211_conf_get_wkreason(sys_cfgs.wifi_mode);
    if (type == WKREASON_START_UP && wkreason == 0) {
        wkreason = sys_wakeup_reason();
    }

    os_printf(KERN_NOTICE"wakeup reason: %d\r\n", wkreason);
    if (wkreason == 0) {
        return;
    }

    for (i = 0; i < sizeof(sys_cfgs.wkhost_reasons); i++) {
        if (sys_cfgs.wkhost_reasons[i] == wkreason) {
            wk = 1;
            break;
        }
    }

    switch (wkreason) {
        case DSLEEP_WK_REASON_TIMER:
            break;
        case DSLEEP_WK_REASON_TIM:
            if(type == WKREASON_START_UP){
                os_printf("wakeup by TIM, delay check wk_reason after connected\r\n");
                wk = 0;
            }
            break;
        case DSLEEP_WK_REASON_BC_TIM:
            break;
        case DSLEEP_WK_REASON_IO:
            break;
        case DSLEEP_WK_REASON_BEACON_LOST:
            break;
        case DSLEEP_WK_REASON_AP_ERROR:
            break;
        case DSLEEP_WK_REASON_HB_TIMEOUT:
            break;
        case DSLEEP_WK_REASON_WK_DATA:
            break;
        case DSLEEP_WK_REASON_MCLR:
            break;
        case DSLEEP_WK_REASON_LVD:
            break;
        case DSLEEP_WK_REASON_PIR:
            break;
        case DSLEEP_WK_REASON_APWK:
            break;
        case DSLEEP_WK_REASON_PS_DISCONNECT:
            break;
        case DSLEEP_WK_REASON_MAX_IDLE_TMO:
            break;
        case DSLEEP_WK_REASON_STA_ERROR:
            break;
        case DSLEEP_WK_REASON_SLEEPED_STA_ERROR:
            break;
        case DSLEEP_WK_REASON_STA_DATA:
            break;
        case DSLEEP_WK_REASON_AP_PAIR:
            break;
        default:
            break;
    }

    if (wk) {
        sys_wakeup_host();
    }
#endif
}

