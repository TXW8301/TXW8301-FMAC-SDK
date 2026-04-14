# TXW8301 FMAC AT Commands (Table + Notes)

Source SDK line: TX_AH_SDK_2.4  
Source firmware tree: TXW8301_FMAC-v2.4.1.5-40938  
Primary registration table: project/atcmd.c (static_atcmds)

## What Is Code-Verified vs Inferred

- Verified from code: command registration list, handlers, compile guards, selected behaviors from sdk/lib/common/atcmd.c (for example AT+SSID, AT+RSSI, AT+PING, AT+SYSDBG).
- Inferred from naming: many low-level test/calibration handlers (implemented in linked atcmd/lmac/wifi libs, not fully visible in this tree).

## Build/Runtime Remarks Found During Review

| Item | Value | Remarks |
|---|---|---|
| AT registration | project/atcmd.c | static_atcmds is the source of enabled commands for this firmware tree. |
| AT UART init | sys_atcmd_init | atcmd_uart_init(ATCMD_UARTDEV, 115200, 5, &setting). |
| Default AT UART (FMAC) | sys_config.h | USB builds use HG_UART0_DEVID; otherwise HG_UART1_DEVID. |
| Main init path | main.c | sys_atcmd_init() is called during boot before app loop. |
| Conditional commands | compile guards | Commands gated by SYS_NETWORK_SUPPORT, LWIP_RAW, WIFI_REPEATER_SUPPORT, CONFIG_SLEEP, and TX4001A guard around FT_ATT. |
| Shared command set | FMAC/WNB | FMAC and WNB project/atcmd.c are largely aligned in this SDK line. |

## 1) Core System and Maintenance

| Command | Handler | Description | Remarks |
|---|---|---|---|
| AT+RST | sys_atcmd_reset | Software reset/restart flow. | Core system command. |
| AT+SYSDBG | sys_atcmd_sysdbg | Set debug flags (heap/top/lmac/umac/irq). | Code shows two args expected: key,value. |
| AT+LOADDEF | sys_atcmd_loaddef | Load default configuration. | Separate test-mode LOADDEF handler is commented out. |
| AT+JTAG | sys_atcmd_jtag | JTAG related control. | Likely factory/debug oriented. |
| AT+SYSCFG | sys_syscfg_dump_hdl | Dump system configuration. | Calls syscfg_dump(). |
| AT+FWUPG | xmodem_fwupgrade_hdl | Firmware upgrade over xmodem path. | Maintenance/production use. |
| AT+VERSION | sys_wifi_atcmd_version | Report firmware version/build info. | Good first sanity check command. |

## 2) Wi-Fi Setup and Role Control

| Command | Handler | Description | Remarks |
|---|---|---|---|
| AT+SSID | sys_wifi_atcmd_set_ssid | Set/query SSID. | Code supports query with ? and set path; SSID max length check (32). |
| AT+KEY | sys_wifi_atcmd_set_key | Set query key material (legacy/key path). | Security related. |
| AT+PSK | sys_wifi_atcmd_set_psk | Set/query PSK. | Security related. |
| AT+ENCRYPT | sys_wifi_atcmd_set_encrypt | Set encryption mode. | Clears/updates keymgmt in code paths. |
| AT+WIFIMODE | sys_wifi_atcmd_set_wifimode | Switch STA/AP/WNB roles/modes. | Mode affects many other commands. |
| AT+CHANNEL | sys_wifi_atcmd_set_channel | Set working channel. | RF/channel planning command. |
| AT+APHIDE | sys_wifi_atcmd_aphide | AP hidden SSID behavior. | AP mode only in practice. |
| AT+SCAN | sys_wifi_atcmd_scan | Trigger scan/report scan data. | STA/site survey workflow. |
| AT+PAIR | sys_wifi_atcmd_pair | Pair/connect workflow start. | Product workflow command. |
| AT+UNPAIR | sys_wifi_atcmd_unpair | Remove pairing/disconnect workflow. | Product workflow command. |
| AT+HWMODE | sys_wifi_atcmd_hwmode | Set Wi-Fi HW mode/profile. | Naming suggests PHY profile selection. |
| AT+ROAM | sys_wifi_atcmd_roam | Control roaming behavior. | STA/repeater scenarios. |
| AT+BSS_BW | sys_wifi_atcmd_bss_bw | Configure BSS bandwidth policy. | AH-specific path in this table. |
| AT+CHAN_LIST | sys_wifi_atcmd_chan_list | Configure/inspect allowed channel list. | Useful for region/channel restrictions. |

## 3) Link Status and Station Information

| Command | Handler | Description | Remarks |
|---|---|---|---|
| AT+RSSI | sys_wifi_atcmd_get_rssi | Read current RSSI. | Code shows query style with ? argument. |
| AT+STA_INFO | atcmd_sta_info_hdl | Dump station/link context info. | Inferred from name; low-level handler. |
| AT+MAC_ADDR | atcmd_mac_addr_hdl | Get/set MAC address. | Inferred from name; low-level handler. |

## 4) Network Diagnostics and Throughput

| Command | Handler | Description | Remarks |
|---|---|---|---|
| AT+ICMPMNTR | sys_atcmd_icmp_mntr | Enable/disable ICMP monitor on interface. | Code expects two args: ifindex and enable. |
| AT+PING | sys_atcmd_ping | Ping destination. | Compiled when SYS_NETWORK_SUPPORT and LWIP_RAW; argv supports host,count,size. |
| AT+IPERF2 | sys_atcmd_iperf2 | iPerf2 throughput test command. | Compiled when SYS_NETWORK_SUPPORT. |

## 5) Repeater-Specific Commands

| Command | Handler | Description | Remarks |
|---|---|---|---|
| AT+R_SSID | sys_wifi_atcmd_set_rssid | Repeater upstream SSID config. | Compiled with WIFI_REPEATER_SUPPORT. |
| AT+R_KEY | sys_wifi_atcmd_set_rkey | Repeater upstream key config. | Compiled with WIFI_REPEATER_SUPPORT. |
| AT+R_PSK | sys_wifi_atcmd_set_rpsk | Repeater upstream PSK config. | Compiled with WIFI_REPEATER_SUPPORT. |

## 6) Sleep, Wake, and Power Management

| Command | Handler | Description | Remarks |
|---|---|---|---|
| AT+SLEEP_EN | atcmd_sleep_en_hdl | Enable/disable sleep behavior. | Low-level power control. |
| AT+AP_SLEEP_MODE | atcmd_ap_sleep_mode | AP-side sleep mode tuning. | Low-level power control. |
| AT+WAKE_EN | atcmd_wake_stas_hdl | Wake mechanism control for stations. | Low-level wake behavior. |
| AT+WAKEUP | sys_wifi_atcmd_wakeup | Trigger wakeup operation. | Product-facing wake command. |
| AT+AP_PSMODE | sys_wifi_atcmd_ap_psmode | AP power-save mode. | Compiled with CONFIG_SLEEP. |
| AT+DSLEEP | sys_wifi_atcmd_dsleep | Deep sleep entry/config. | Compiled with CONFIG_SLEEP. |
| AT+PS_CHECK | atcmd_ps_check_hdl | Power-save diagnostics/check. | Inferred from name. |
| AT+RADIO_ONOFF | atcmd_radio_onoff_hdl | Turn RF radio path on/off. | Useful in bring-up/debug. |

## 7) RF, PHY, MAC, Calibration, and Test

### 7.1 RF/PHY/MAC Control Knobs

| Command | Handler | Description | Remarks |
|---|---|---|---|
| AT+PHY_RESET | atcmd_phy_reset_hdl | Reset PHY subsystem. | Low-level test command. |
| AT+RF_RESET | atcmd_rf_reset_hdl | Reset RF subsystem. | Low-level test command. |
| AT+PRI_CHAN | atcmd_set_pri_chan_hdl | Set primary channel index. | Low-level channel control. |
| AT+SHORT_GI | atcmd_short_gi_hdl | Configure short GI behavior. | PHY timing tuning. |
| AT+SHORT_TH | atcmd_short_th_hdl | Configure short threshold/tuning value. | Inferred from name. |
| AT+SET_RTS | atcmd_set_rts_hdl | Set RTS threshold/policy. | MAC tuning. |
| AT+RTS_DUP | atcmd_rts_dup_hdl | RTS duplicate behavior control. | Inferred from name. |
| AT+CTS_DUP | atcmd_cts_dup_hdl | CTS duplicate behavior control. | Inferred from name. |
| AT+CCMP_SUPPORT | atcmd_ccmp_support_hdl | CCMP support toggle/inspection. | Security capability tuning. |
| AT+TXOP_EN | atcmd_txop_en_hdl | Enable/disable TXOP behavior. | EDCA/MAC tuning. |
| AT+TX_FC | atcmd_tx_fc_hdl | TX flow control tuning. | Inferred from name. |
| AT+TX_FLAGS | atcmd_tx_flags_hdl | Set internal TX flags. | Test/debug use. |
| AT+TX_ORDERED | atcmd_strictly_ordered_hdl | Ordered TX behavior control. | Queue/order debugging. |
| AT+TX_BW | atcmd_tx_bw_hdl | Set TX bandwidth mode. | PHY tuning. |
| AT+TX_BW_DYNAMIC | atcmd_tx_bw_dynamic_hdl | Dynamic TX bandwidth control. | PHY/MAC tuning. |
| AT+TX_TYPE | atcmd_tx_type_hdl | Select TX test packet type. | Test mode. |
| AT+TX_RATE_FIXED | atcmd_tx_rate_fixed_hdl | Fix TX rate adaptation behavior. | Rate-control debugging. |
| AT+TX_MCS | atcmd_tx_mcs_hdl | Set TX MCS. | Rate control tuning. |
| AT+TX_MCS_MAX | atcmd_tx_mcs_max_hdl | Set max TX MCS limit. | Rate control tuning. |
| AT+TX_MCS_MIN | atcmd_tx_mcs_min_hdl | Set min TX MCS limit. | Rate control tuning. |
| AT+TX_LEN | atcmd_tx_len_hdl | Set test TX frame length. | Test mode. |
| AT+TX_MAX_AGG | atcmd_tx_max_agg_hdl | Max aggregation setting. | Aggregation tuning. |
| AT+TX_MAX_SYMS | atcmd_tx_max_syms_hdl | Max symbols setting. | PHY/test tuning. |
| AT+TX_AGG_AUTO | atcmd_agg_auto_hdl | Auto aggregation control. | Aggregation tuning. |
| AT+TX_TRV_PILOT_EN | atcmd_tx_trv_pilot_en_hdl | Pilot-related TX test option. | Inferred from name. |

### 7.2 TX Trigger/Flow Test Controls

| Command | Handler | Description | Remarks |
|---|---|---|---|
| AT+TEST_START | atcmd_test_start_hdl | Enter/start RF test workflow. | Factory/lab usage. |
| AT+TX_START | atcmd_tx_start_hdl | Start TX test traffic. | Factory/lab usage. |
| AT+TX_STEP | atcmd_tx_step_hdl | Step TX test sequence. | Factory/lab usage. |
| AT+TX_TRIG | atcmd_tx_trig_hdl | Trigger TX event manually. | Factory/lab usage. |
| AT+TX_CONT | atcmd_tx_cont_hdl | Continuous TX mode. | RF validation. |
| AT+TX_DELAY | atcmd_tx_delay_hdl | TX delay injection/config. | Timing debug. |
| AT+TX_CNT_MAX | atcmd_tx_cnt_max_hdl | Maximum TX count for tests. | Test bounds. |
| AT+TX_DST_ADDR | atcmd_tx_dst_addr_hdl | Set destination address for TX test. | Traffic targeting. |
| AT+TX_CW | atcmd_tx_cw_hdl | CW transmit mode control. | RF lab calibration. |

### 7.3 TX Power and Analog Front-End

| Command | Handler | Description | Remarks |
|---|---|---|---|
| AT+TXPOWER | atcmd_txpower_hdl | Set/read TX power. | Product + lab useful. |
| AT+TX_ATTN | atcmd_tx_attn_hdl | Set TX attenuation. | RF calibration. |
| AT+TX_PWR_AUTO | atcmd_tx_pwr_auto_hdl | Auto TX power control. | Power algorithm tuning. |
| AT+TX_PWR_MAX | atcmd_tx_pwr_max_hdl | Max TX power limit. | Regulatory/tuning use. |
| AT+TX_PWR_SUPER | atcmd_tx_pwr_super_hdl | Super power mode control. | Platform-specific tuning. |
| AT+TX_PWR_SUPER_TH | atcmd_tx_pwr_super_th_hdl | Threshold for super power logic. | Platform-specific tuning. |
| AT+TX_PHA_AMP | atcmd_tx_pha_amp_hdl | Phase/amplitude TX tuning. | RF calibration. |
| AT+SET_VDD13 | atcmd_set_vdd13_hdl | Internal rail/voltage related tuning. | Hardware-specific. |
| AT+XO_CS | atcmd_xo_cs_hdl | Crystal oscillator capacitance/setting. | Clock calibration. |
| AT+XO_CS_AUTO | atcmd_xo_cs_auto_hdl | Auto XO calibration/selection. | Clock calibration. |
| AT+LO_FREQ | atcmd_lo_freq_hdl | Local oscillator frequency tuning. | RF calibration. |
| AT+LO_TABLE | atcmd_lo_table_read_hdl | Read LO table data. | RF debug/readback. |
| AT+FT_ATT | atcmd_ft_att_hdl | Front-end attenuation tuning. | Omitted on TX4001A by guard. |

### 7.4 RX/Quality/Counters Readback

| Command | Handler | Description | Remarks |
|---|---|---|---|
| AT+RX_RSSI | atcmd_rx_rssi_rd_hdl | Read RX RSSI from low-level path. | Different from AT+RSSI system path. |
| AT+RX_EVM | atcmd_rx_evm_rd_hdl | Read RX EVM metric. | PHY quality metric. |
| AT+RX_AGC | atcmd_rx_agc_rd_hdl | Read RX AGC state/value. | RF tuning/debug. |
| AT+RX_ERR | atcmd_rx_err_rd_hdl | Read RX error counters. | Reliability diagnostics. |
| AT+RX_PKTS | atcmd_rx_pkts_rd_hdl | Read RX packet counters. | Throughput diagnostics. |
| AT+RX_REORDER | atcmd_rx_ordered_hdl | RX reorder behavior/counters. | Queue/reorder diagnostics. |
| AT+TX_PKTS | atcmd_tx_pkts_rd_hdl | Read TX packet counters. | Throughput diagnostics. |
| AT+TX_FAIL | atcmd_tx_fail_rd_hdl | Read TX failure counters. | Link quality diagnostics. |
| AT+T_SENSOR | atcmd_t_sensor_hdl | Read temperature sensor. | Thermal diagnostics. |

### 7.5 OBSS/CCA/EDCA/PCF Runtime Tuning

| Command | Handler | Description | Remarks |
|---|---|---|---|
| AT+CCA_OBSV | atcmd_cca_obsv_hdl | CCA observation controls/readback. | Coexistence/channel occupancy tuning. |
| AT+OBSS_CCA_DIFF | atcmd_obss_cca_diff_hdl | OBSS CCA differential threshold. | OBSS tuning. |
| AT+OBSS_NAV_DIFF | atcmd_obss_nav_diff_hdl | OBSS NAV differential threshold. | OBSS tuning. |
| AT+OBSS_SWITCH | atcmd_obss_switch_hdl | OBSS tuning switch/enable. | OBSS tuning. |
| AT+OBSS_TH | atcmd_obss_th_hdl | OBSS threshold setting. | OBSS tuning. |
| AT+OBSS_EDCA | atcmd_obss_edca_hdl | OBSS-aware EDCA control. | QoS/coexistence tuning. |
| AT+EDCA_AIFS | atcmd_edca_aifs_hdl | EDCA AIFS settings. | QoS tuning. |
| AT+EDCA_CW | atcmd_edca_cw_hdl | EDCA contention window settings. | QoS tuning. |
| AT+EDCA_TXOP | atcmd_edca_txop_hdl | EDCA TXOP limits. | QoS tuning. |
| AT+AP_BACKOFF | atcmd_edca_ap_backoff_hdl | AP backoff tuning. | QoS tuning. |
| AT+PCF_EN | atcmd_pcf_en_hdl | Enable/disable PCF behavior. | Legacy MAC mode tuning. |
| AT+PCF_PERCENT | atcmd_pcf_percent_hdl | PCF percentage setting. | MAC scheduling tuning. |
| AT+PCF_PERIOD | atcmd_pcf_period_hdl | PCF period setting. | MAC scheduling tuning. |

### 7.6 Misc Test/Factory/Register Access

| Command | Handler | Description | Remarks |
|---|---|---|---|
| AT+REG_RD | atcmd_reg_rd_hdl | Read chip register. | Low-level debug access. |
| AT+REG_WT | atcmd_reg_wt_hdl | Write chip register. | Low-level debug access. |
| AT+NOR_RD | atcmd_nor_rd_hdl | Read NOR flash/register area. | Storage/debug path. |
| AT+BUS_WT | atcmd_bus_wt_hdl | Bus write/test operation. | Bring-up/debug. |
| AT+SMT_DAT | atcmd_smt_dat_hdl | SMT/factory data access. | Manufacturing use. |
| AT+CHAN_SCAN | atcmd_chan_scan_hdl | Channel scan/test command. | Low-level scan path. |
| AT+FREQ_LIST | atcmd_freq_list_hdl | Frequency list operation. | Region/test support. |
| AT+LMAC_DBGSEL | atcmd_lmac_dbgsel_hdl | LMAC debug selector. | LMAC debug routing. |
| AT+PRINT_PERIOD | atcmd_lmac_print_period_hdl | Periodic debug print interval. | Runtime diagnostics. |
| AT+QA_ATT | atcmd_qa_att_hdl | QA attenuation parameter. | QA/factory workflow. |
| AT+QA_CFG | atcmd_qa_cfg_hdl | QA configuration. | QA/factory workflow. |
| AT+QA_RESULTS | atcmd_qa_results_hdl | Read QA results. | QA/factory workflow. |
| AT+QA_RXTHD | atcmd_qa_rxthd_hdl | QA RX threshold. | QA/factory workflow. |
| AT+QA_TXTHD | atcmd_qa_txthd_hdl | QA TX threshold. | QA/factory workflow. |
| AT+QA_START | atcmd_qa_start_hdl | Start QA test run. | QA/factory workflow. |
| AT+RC_NEW | atcmd_rc_new_hdl | Rate-control/new RC tuning hook. | Inferred from name. |
| AT+ACS_START | atcmd_acs_start_hdl | Start ACS procedure. | Channel selection tuning. |
| AT+ACK_TO | atcmd_ack_to_extra_hdl | ACK timeout tuning. | Timing tuning. |
| AT+ADC_DUMP | atcmd_adc_dump_hdl | Dump ADC data. | Analog debug. |
| AT+ANT_DUAL | atcmd_ant_dual_hdl | Dual antenna mode. | Antenna config. |
| AT+ANT_CTRL | atcmd_ant_ctrl_hdl | Antenna control policy. | Antenna config. |
| AT+ANT_AUTO | atcmd_ant_auto_hdl | Auto antenna selection. | Antenna config. |
| AT+ANT_DEF | atcmd_ant_def_hdl | Default antenna selection. | Antenna config. |
| AT+BGRSSI_MARGIN | atcmd_bgrssi_margin_hdl | Background RSSI margin tuning. | Noise/interference tuning. |
| AT+BGRSSI_SPUR | atcmd_bgrssi_spur_hdl | Spur/background RSSI handling. | Noise/interference tuning. |
| AT+CS_CNT | atcmd_cs_cnt_hdl | Carrier sense counter/tuning. | CS diagnostics. |
| AT+CS_EN | atcmd_cs_enable_hdl | Enable carrier sense logic. | CS diagnostics. |
| AT+CS_NUM | atcmd_cs_num_hdl | Carrier sense parameter count. | CS diagnostics. |
| AT+CS_PERIOD | atcmd_cs_period_hdl | Carrier sense period. | CS diagnostics. |
| AT+CS_TH | atcmd_cs_th_hdl | Carrier sense threshold. | CS diagnostics. |
| AT+EVM_MARGIN | atcmd_evm_margin_hdl | EVM margin threshold tuning. | PHY quality thresholding. |
| AT+MCAST_DUP | atcmd_mcast_dup_hdl | Multicast duplicate control. | Multicast behavior tuning. |
| AT+MCAST_REORDER | atcmd_mcast_reorder_hdl | Multicast reorder behavior. | Multicast behavior tuning. |
| AT+MCAST_BW | atcmd_mcast_bw_hdl | Multicast bandwidth behavior. | Multicast behavior tuning. |
| AT+MCAST_MCS | atcmd_mcast_mcs_hdl | Multicast MCS setting. | Multicast behavior tuning. |
| AT+MCAST_RTS | atcmd_mcast_rts_hdl | Multicast RTS behavior. | Multicast behavior tuning. |

## 8) Commands Present but Disabled in This Table Snapshot

| Command | Status | Remarks |
|---|---|---|
| AT+BSS_BW (test handler variant) | Commented out | Distinct from enabled AH-specific sys_wifi_atcmd_bss_bw entry. |
| AT+LOADDEF (test handler variant) | Commented out | Core AT+LOADDEF remains enabled via sys_atcmd_loaddef. |
| AT+REBOOT | Commented out | No active mapping in static_atcmds. |
| AT+RX_ADDR_FILTER | Commented out | Low-level test/diagnostic path disabled. |
| AT+RX_PHY_CHECK | Commented out | Low-level test/diagnostic path disabled. |

## 9) Practical Usage Notes

| Topic | Notes |
|---|---|
| Query syntax | Several commands use ? query mode (verified examples: AT+SSID, AT+RSSI). |
| Error model | Many handlers return atcmd_ok/atcmd_error semantics depending on argument validation. |
| Command stability | Product-facing commands are mostly sys_wifi_atcmd_* and sys_atcmd_*; atcmd_* handlers are often factory/tuning oriented. |
| Safety | Register/PHY/RF commands can destabilize runtime behavior; use cautiously on production units. |
