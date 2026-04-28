#ifndef _PROJECT_SYSCFG_H_
#define _PROJECT_SYSCFG_H_

enum WIFI_WORK_MODE {
    WIFI_MODE_NONE = 0,
    WIFI_MODE_STA,
    WIFI_MODE_WNBSTA,
    WIFI_MODE_AP,
    WIFI_MODE_WNBAP,
    
    WIFI_MODE_AP_STA,
    WIFI_MODE_WNBAP_WNBSTA,
    WIFI_MODE_AP_WNBSTA,
    WIFI_MODE_WNBAP_STA,
    
    WIFI_MODE_AP_WNBAP,
};

#define MODE_IS_AP(mode)        (mode == WIFI_MODE_AP  || mode == WIFI_MODE_WNBAP)
#define MODE_IS_STA(mode)       (mode == WIFI_MODE_STA || mode == WIFI_MODE_WNBSTA)
#define MODE_IS_REPEATER(mode)  (mode >= WIFI_MODE_AP_STA && mode != WIFI_MODE_AP_WNBAP)
#define MODE_INCLUDE_AP(mode)   (MODE_IS_AP(mode) || mode >= WIFI_MODE_AP_STA)

#define AH_VALID_BW(bw)  ((bw)==1||(bw)==2||(bw)==4||(bw)==8)
#define AH_VALID_FREQ(f) ((f)>=7300&&(f)<=9300)

enum WKREASON_TYPE {
    WKREASON_START_UP = 0,
    WKREASON_CONNECTTED,
    WKREASON_DISCONNECT,
    WKREASON_RESUME,
};

struct sys_config {
    uint16 magic_num, crc;
    uint16 size, rev1, rev2, rev3;
    //////////////////////////////////////////////////
    uint8  wifi_mode, bss_bw, tx_mcs, chan_cnt;
    uint8  txpower, rev4, acs_enable, acs_tmo;
    uint16 chan_list[16];
    uint8  bssid[6], mac[6];
    uint8  ssid[SSID_MAX_LEN+1];
    uint8  psk[32];
    uint8  mcast_key[32];
    char   passwd[PASSWD_MAX_LEN+1];
    uint16 bss_max_idle, beacon_int;
    uint16 ack_tmo, dtim_period;
    uint16 aplost_time;
    int16  auto_sleep_time;

    uint16 heartbeat_int;
    uint8  psconnect_period, psconnect_roundup;

    uint8  wkup_io: 7, wkio_edge: 1;
    uint8  pri_chan, agg_cnt, channel;
    uint8  frm_max, rts_max;
    uint8  tx_bw, wifi_hwmode;

    uint32 ipaddr, netmask, gw_ip;
    uint32 dhcpd_startip, dhcpd_endip, dhcpd_lease_time;

    uint32 key_mgmt;
    uint32 auto_save: 1,
           wkio_mode: 1,
           auto_chsw: 1,
           auto_role: 1,
           dupfilter: 1,
           pa_pwrctrl_dis: 1,
           pair_autostop: 1,
           super_pwr_set: 1,
           dcdc13: 2,
           dhcpc_en: 1,
           dhcpc_update_hbdata: 1,
           wkhost_reasons_set: 1,
           dual_ant: 1,
           ant_auto_dis: 1,
           ant_sel: 1,
           mac_filter_en: 1,
           bssid_set: 1,
           mcast_filter: 1,
           reassoc_wkhost: 1,
           use4addr: 1,
           wkdata_save: 1,
           autosleep: 1,
           psmode: 3,
           pair_conn_only:1,
           dis_1v1_m2u: 1,
           dis_psconnect: 1,
           ap_hide: 1,
           cca_for_ce: 1,
           ap_psmode: 1;

    uint8  dhcpc_hostname[12];
    uint8  wkhost_reasons[32];

    uint8  wait_psmode: 2, roam_start: 1, cfg_init: 1, mkey_set: 1, super_pwr:3;
    uint8  sta_max, r1;
    uint8  standby_channel;

    uint32 standby_period_ms;

    uint8  r_ssid[SSID_MAX_LEN+1];
    uint8  r_psk[32];
    char   r_passwd[PASSWD_MAX_LEN+1];
    uint32 r_key_mgmt;

#if SYS_SAVE_PAIRSTA
    uint8  pair_stas[SYS_STA_MAX][6];
#endif
    uint32 uart_fixlen;

    uint32 wpa_proto;
    uint32 pairwise_cipher;
    uint32 group_cipher;
    
    int8   roam_rssi_th;
    uint8  roam_rssi_diff, roam_rssi_int;
};

struct system_status {
    uint32 dhcpc_done: 1,
           wifi_connected: 1,
           dbg_heap: 1,
           dbg_top: 2,
           dbg_lmac: 2,
           dbg_umac: 1,
           dbg_irq: 1,
           dbg_net: 1,
           reset_wdt: 1,
           pair_success:1,
           upgrading: 1;
    uint8  bssid[6];
    int8   rssi, evm;
    uint8  channel;
    struct {
        uint32 ipaddr, netmask, svrip, router, dns1, dns2;
    } dhcpc_result;
};

extern struct sys_config sys_cfgs;
extern struct system_status sys_status;

void syscfg_set_default_val(void);
void syscfg_set_default_chanlist();
int32 wificfg_flush(uint8 ifidx);
void syscfg_check(void);
void syscfg_dump(void);


#endif

