#ifndef _QCA_HCI_H
#define _QCA_HCI_H

#include <IOKit/IOTypes.h>

// ========================
// HCI Packet Types
// ========================
#define HCI_COMMAND_PKT   0x01
#define HCI_ACLDATA_PKT   0x02
#define HCI_SCODATA_PKT   0x03
#define HCI_EVENT_PKT     0x04

// ========================
// Standard Opcodes
// ========================
#define HCI_OPCODE(ogf, ocf)  ((uint16_t)(((ocf) & 0x03FF) | ((ogf) << 10)))

// OGF (Opcode Group Field)
#define OGF_LINK_CTL     0x01
#define OGF_LINK_POLICY  0x02
#define OGF_CTRL_BB      0x03
#define OGF_INFO_PARAM   0x04
#define OGF_STATUS_PARAM 0x05
#define OGF_TESTING      0x06
#define OGF_VENDOR       0x3F

// OCF (Opcode Command Field)
#define OCF_RESET        0x0003

// Common opcodes
#define HCI_RESET        HCI_OPCODE(OGF_CTRL_BB, OCF_RESET)

// ========================
// QCA Vendor-Specific Opcodes
// (参考 Linux hci_qca.c)
// ========================
#define QCA_HCI_VS_GET_VERSION   HCI_OPCODE(OGF_VENDOR, 0x0001)  // 获取 ROM/patch 版本
#define QCA_HCI_VS_DOWNLOAD      HCI_OPCODE(OGF_VENDOR, 0x0002)  // 下载 patch 分片
#define QCA_HCI_VS_SET_BAUDRATE  HCI_OPCODE(OGF_VENDOR, 0x0003)  // 改串口速率 (UART 用)
#define QCA_HCI_VS_RAMPATCH      HCI_OPCODE(OGF_VENDOR, 0x0004)  // 启动 patch

// ========================
// HCI Event Codes
// ========================
#define EVT_CMD_COMPLETE   0x0E
#define EVT_CMD_STATUS     0x0F
#define EVT_VENDOR_SPEC    0xFF

// ========================
// Packet Structures
// ========================

/* HCI Command Header (3 bytes) */
typedef struct __attribute__((packed)) {
    uint16_t opcode;   // OGF+OCF
    uint8_t plen;      // 参数长度
} hci_command_hdr;

/* HCI Event Header (2 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t evt;       // 事件码
    uint8_t plen;      // 参数长度
} hci_event_hdr;

/* HCI ACL Data Header (4 bytes) */
typedef struct __attribute__((packed)) {
    uint16_t handle;   // 连接 handle + PB/BC 标志
    uint16_t dlen;     // 数据长度
} hci_acl_hdr;

/* HCI SCO Data Header (3 bytes) */
typedef struct __attribute__((packed)) {
    uint16_t handle;
    uint8_t dlen;
} hci_sco_hdr;

// ========================
// Helper Macros
// ========================
#define HCI_PKT_IND(cmd)   ((uint8_t)(cmd))  // 第一个字节指示 packet 类型

#endif // _QCA_HCI_H
