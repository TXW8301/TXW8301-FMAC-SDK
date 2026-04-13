/*****************************************************************************
* Module    : usb
* File      : usb_dev_wifi.c
* Author    :
* Function  : USB wifi  huge-ic defined, refer to ch9.h
*****************************************************************************/
#include "sys_config.h"
#include "typesdef.h"
#include "list.h"
#include "dev.h"
#include "devid.h"
#include "osal/string.h"
#include "osal/mutex.h"
#include "osal/semaphore.h"
#include "osal/task.h"
#include "hal/usb_device.h"
#include "lib/usb/usb_device_wifi.h"

/* TX & RX EP is for device */
#define USB_WIFI_TX_EP                  1
#define USB_WIFI_RX_EP                  1
#define USB_WIFI_EP_MAX_PKT_SIZE_HS        512
#define USB_WIFI_EP_MAX_PKT_SIZE_FS        64

#define USB_WIFI_CFG_DESC_LEN           (sizeof(tbl_usb_wifi_config_all_descriptor_wifi_hs))
#define USB_WIFI_IF_NUMS                1


#ifndef USB_VID
#define USB_VID                         0xA012
#endif
#ifndef USB_PID
#define USB_PID                         0x0000
#endif


//设备描述符
const uint8_t tbl_usb_wifi_device_descriptor[18] = {
    18,                 // Num bytes of the descriptor
    1,                  // Device Descriptor type
    0x00, 0x02,         // Revision of USB Spec. (in BCD)
    0xFF,               // Class : user-defined
    0xFF,               // Sub Class : user-defined
    0xFF,               // Class specific protocol : user-defined
    0x40,               // Max packet size of Endpoint 0
    (USB_VID >> 0) & 0xff,
    (USB_VID >> 8) & 0xff,         // Vendor ID
    (USB_PID >> 0) & 0xff,
    (USB_PID >> 8) & 0xff,         // Product ID
    0x00, 0x02,         // Device Revision (in BCD)
    1,                  // Index of Manufacture string descriptor
    2,                  // Index of Product string descriptor
    0,                  // Index of Serial No. string descriptor
    1                   // Num Configurations, Must = 1
};


//配置描述符 自定义WIFI
const uint8_t tbl_usb_wifi_config_all_descriptor_wifi_fs[23 + 9] = {
    9,                  // Num bytes of this descriptor
    2,                  // Configuration descriptor type
    (sizeof(tbl_usb_wifi_config_all_descriptor_wifi_fs) & 0xFF), 
    ((sizeof(tbl_usb_wifi_config_all_descriptor_wifi_fs)>>8) & 0xFF),             // Total size of configuration
    USB_WIFI_IF_NUMS,   // Num Interface, 会根据最后实际接口的数量来配置
    1,                  // Configuration number
    0,                  // Index of Configuration string descriptor
    0x80,               // Configuration characteristics: BusPowerd
    200, //0x32,           // Max current, unit is 2mA

//Bulk Interface 9 + 7 + 7= 23
    9,                  // Num bytes of this descriptor
    4,                  // Interface descriptor type
    0,                  // Interface Number，暂时填0，会根据最后实际配置来依次递加
    0,                  // Alternate interface number
    2,                  // Num endpoints of this interface
    0xff,               // Interface Class: unknown
    0xff,               // Interface Sub Class: unknown
    0xff,               // Class specific protocol: unknown
    0,                  // Index of Interface string descriptor

    7,                  // Num bytes of this descriptor
    5,                  // Endpoint descriptor type
    USB_WIFI_RX_EP,            //BULKOUT_EP, // Endpoint number, bit7=0 shows OUT
    2,                  // Bulk endpoint
    (USB_WIFI_EP_MAX_PKT_SIZE_FS >> 0) & 0xff, // Maximum packet size
    (USB_WIFI_EP_MAX_PKT_SIZE_FS >> 8) & 0xff,
    0,                  // no use for bulk endpoint

    7,                  // Num bytes of this descriptor
    5,                  // Endpoint descriptor type
    USB_WIFI_TX_EP | 0x80,   // Endpoint number, bit7=1 shows IN
    2,                  // Bulk endpoint
    (USB_WIFI_EP_MAX_PKT_SIZE_FS >> 0) & 0xff, // Maximum packet size
    (USB_WIFI_EP_MAX_PKT_SIZE_FS >> 8) & 0xff,
    0,                  // No use for bulk endpoint
};

//配置描述符 自定义WIFI
const uint8_t tbl_usb_wifi_config_all_descriptor_wifi_hs[23 + 9] = {
    9,                  // Num bytes of this descriptor
    2,                  // Configuration descriptor type
    (USB_WIFI_CFG_DESC_LEN & 0xFF), 
    ((USB_WIFI_CFG_DESC_LEN>>8) & 0xFF),             // Total size of configuration
    USB_WIFI_IF_NUMS,   // Num Interface, 会根据最后实际接口的数量来配置
    1,                  // Configuration number
    0,                  // Index of Configuration string descriptor
    0x80,               // Configuration characteristics: BusPowerd
    200, //0x32,           // Max current, unit is 2mA

//Bulk Interface 9 + 7 + 7= 23
    9,                  // Num bytes of this descriptor
    4,                  // Interface descriptor type
    0,                  // Interface Number，暂时填0，会根据最后实际配置来依次递加
    0,                  // Alternate interface number
    2,                  // Num endpoints of this interface
    0xff,               // Interface Class: unknown
    0xff,               // Interface Sub Class: unknown
    0xff,               // Class specific protocol: unknown
    0,                  // Index of Interface string descriptor

    7,                  // Num bytes of this descriptor
    5,                  // Endpoint descriptor type
    USB_WIFI_RX_EP,            //BULKOUT_EP, // Endpoint number, bit7=0 shows OUT
    2,                  // Bulk endpoint
    (USB_WIFI_EP_MAX_PKT_SIZE_HS >> 0) & 0xff, // Maximum packet size
    (USB_WIFI_EP_MAX_PKT_SIZE_HS >> 8) & 0xff,
    0,                  // no use for bulk endpoint

    7,                  // Num bytes of this descriptor
    5,                  // Endpoint descriptor type
    USB_WIFI_TX_EP | 0x80,   // Endpoint number, bit7=1 shows IN
    2,                  // Bulk endpoint
    (USB_WIFI_EP_MAX_PKT_SIZE_HS >> 0) & 0xff, // Maximum packet size
    (USB_WIFI_EP_MAX_PKT_SIZE_HS >> 8) & 0xff,
    0,                  // No use for bulk endpoint
};


//配置描述符 通用配置
const uint8_t tbl_usb_wifi_config_all_descriptor_gen[9] = {
    9,                  // Num bytes of this descriptor
    2,                  // Configuration descriptor type
    (USB_WIFI_CFG_DESC_LEN & 0xFF), 
    ((USB_WIFI_CFG_DESC_LEN>>8) & 0xFF),             // Total size of configuration
    USB_WIFI_IF_NUMS,   // Num Interface, 会根据最后实际接口的数量来配置
    1,                  // Configuration number
    0,                  // Index of Configuration string descriptor
    0x80,               // Configuration characteristics: BusPowerd
    200, //0x32,           // Max current, unit is 2mA
};


//语言
const uint8_t tbl_usb_wifi_language_id[4] = {
    4,              // Num bytes of this descriptor
    3,              // String descriptor
    0x09, 0x04,     // Language ID
};

//厂商信息
const uint8_t tbl_usb_wifi_str_manufacturer[16] = {
    16,             // Num bytes of this descriptor
    3,              // String descriptor
    'G',    0,
    'e',    0,
    'n',    0,
    'e',    0,
    'r',    0,
    'i',    0,
    'c',    0
};

//产品信息
const uint8_t tbl_usb_wifi_str_product[28] = {
    28,             // Num bytes of this descriptor
    3,              // String descriptor
    'U',    0,
    'S',    0,
    'B',    0,
    '2',    0,
    '.',    0,
    '0',    0,
    ' ',    0,
    'D',    0,
    'e',    0,
    'v',    0,
    'i',    0,
    'c',    0,
    'e',    0
};

//序列号
const uint8_t tbl_usb_wifi_str_serial_number[30] = {
    30,         // Num bytes of this descriptor
    3,          // String descriptor
    '2',    0,
    '0',    0,
    '1',    0,
    '7',    0,
    '0',    0,
    '8',    0,
    '2',    0,
    '9',    0,
    '0',    0,
    '0',    0,
    '0',    0,
    '0',    0,
    '0',    0,
    '1',    0
};



static const struct usb_device_cfg usb_dev_wifi_cfg = {
    .vid        = USB_VID,
    .pid        = USB_PID,
    .speed      = USB_SPEED_FULL,
    .p_device_descriptor = (uint8 *)tbl_usb_wifi_device_descriptor,
    .p_config_descriptor_head = (uint8 *)tbl_usb_wifi_config_all_descriptor_gen,

    .p_config_desc_hs = (uint8 *)tbl_usb_wifi_config_all_descriptor_wifi_hs,
    .p_config_desc_fs = (uint8 *)tbl_usb_wifi_config_all_descriptor_wifi_fs,
    .config_desc_len = sizeof(tbl_usb_wifi_config_all_descriptor_wifi_hs),
    .interface_num  =   1,

    .p_language_id = (uint8 *)tbl_usb_wifi_language_id,
    .language_id_len = sizeof(tbl_usb_wifi_language_id),
    .p_str_manufacturer = (uint8 *)tbl_usb_wifi_str_manufacturer,
    .str_manufacturer_len = sizeof(tbl_usb_wifi_str_manufacturer),
    .p_str_product = (uint8 *)tbl_usb_wifi_str_product,
    .str_product_len = sizeof(tbl_usb_wifi_str_product),
    .p_str_serial_number = (uint8 *)tbl_usb_wifi_str_serial_number,
    .str_serial_number_len = sizeof(tbl_usb_wifi_str_serial_number),

    .ep_nums              =  2,
    .ep_cfg[0].ep_id      =  USB_WIFI_RX_EP,
    .ep_cfg[0].ep_type    =  USB_ENDPOINT_XFER_BULK,
    .ep_cfg[0].ep_dir_tx  =  0,
    .ep_cfg[0].max_packet_size_hs      =  USB_WIFI_EP_MAX_PKT_SIZE_HS,
    .ep_cfg[0].max_packet_size_fs      =  USB_WIFI_EP_MAX_PKT_SIZE_FS,
    .ep_cfg[1].ep_id      =  USB_WIFI_TX_EP,
    .ep_cfg[1].ep_type    =  USB_ENDPOINT_XFER_BULK,
    .ep_cfg[1].ep_dir_tx  =  1,
    .ep_cfg[1].max_packet_size_hs      =  USB_WIFI_EP_MAX_PKT_SIZE_HS,
    .ep_cfg[1].max_packet_size_fs      =  USB_WIFI_EP_MAX_PKT_SIZE_FS,
};

__init int32 usb_device_wifi_open(struct usb_device *p_usb_d)
{
    return usb_device_open(p_usb_d, (struct usb_device_cfg *)&usb_dev_wifi_cfg);
}

int32 usb_device_wifi_close(struct usb_device *p_usb_d)
{
    return usb_device_close(p_usb_d);
}

int32 usb_device_wifi_auto_tx_null_pkt_disable(struct usb_device *p_usb_d)
{
    return usb_device_ioctl(p_usb_d, USB_DEV_IO_CMD_AUTO_TX_NULL_PKT_DISABLE, USB_WIFI_TX_EP, 0);
}

int32 usb_device_wifi_auto_tx_null_pkt_enable(struct usb_device *p_usb_d)
{
    return usb_device_ioctl(p_usb_d, USB_DEV_IO_CMD_AUTO_TX_NULL_PKT_ENABLE, USB_WIFI_TX_EP, 0);
}

int32 usb_device_wifi_write(struct usb_device *p_usb_d, uint8 *buff, uint32 len)
{
    return usb_device_write(p_usb_d, USB_WIFI_TX_EP, buff, len, 0);
}

int32 usb_device_wifi_write_scatter(struct usb_device *p_usb_d, scatter_data *data, int count)
{
    return usb_device_write_scatter(p_usb_d, USB_WIFI_TX_EP, data, count, 0);
}

int32 usb_device_wifi_read(struct usb_device *p_usb_d, uint8 *buff, uint32 len)
{
    return usb_device_read(p_usb_d, USB_WIFI_RX_EP, buff, len, 0);
}


