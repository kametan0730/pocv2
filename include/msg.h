#ifndef MSG_H
#define MSG_H

#include "types.h"
#include "net.h"
#include "ethernet.h"
#include "compiler.h"

enum msgtype {
  MSG_NONE            = 0x0,
  MSG_INIT            = 0x1,
  MSG_INIT_ACK        = 0x2,
  MSG_CLUSTER_INFO    = 0x3,
  MSG_SETUP_DONE      = 0x4,
  MSG_CPU_WAKEUP      = 0x5,
  MSG_CPU_WAKEUP_ACK  = 0x6,
  MSG_SHUTDOWN        = 0x7,
  MSG_FETCH           = 0x8,
  MSG_FETCH_REPLY     = 0x9,
  MSG_INVALIDATE      = 0xa,
  MSG_INVALIDATE_ACK  = 0xb,
  MSG_INTERRUPT       = 0xc,
  MSG_MMIO_REQUEST    = 0xd,
  MSG_MMIO_REPLY      = 0xe,
  MSG_GIC_CONFIG      = 0xf,
  MSG_SGI             = 0x10,
  NUM_MSG,
};

/*
 *  pocv2-msg protocol via Ethernet (64 - 4160 byte)
 *  +-------------+-------------------------+------------------+
 *  | etherheader | src | type |    argv    |      (body)      |
 *  +-------------+-------------------------+------------------+
 *     (14 byte)           (50 byte)          (up to 4096 byte)
 */

struct pocv2_msg_header {
  u32 src_nodeid;     /* me */
  u32 type;           /* enum msgtype */
} __aligned(8);

#define POCV2_MSG_HDR_STRUCT      struct pocv2_msg_header hdr

#define ETH_POCV2_MSG_HDR_SIZE    64

struct pocv2_msg {
  u8 *mac;      /* dst or src */
  struct pocv2_msg_header *hdr;
  void *body;
  u32 body_len;
};

#define pocv2_msg_src_mac(msg)    ((msg)->mac)
#define pocv2_msg_dst_mac(msg)    ((msg)->mac)

#define pocv2_msg_src_nodeid(msg) ((msg)->hdr->src_nodeid)
#define pocv2_msg_type(msg)       ((msg)->hdr->type)

#define POCV2_MSG_ETH_PROTO       0x0019

struct pocv2_msg_data {
  enum msgtype type;
  u32 msg_hdr_size;
  void (*recv_handler)(struct pocv2_msg *);
};

#define DEFINE_POCV2_MSG(ty, hdr_struct, handler)  \
  struct pocv2_msg_data _mdata_##ty __section(".rodata.pocv2_msg") = {   \
    .type = (ty),   \
    .msg_hdr_size = sizeof(hdr_struct),    \
    .recv_handler = handler,    \
  };

void send_msg(struct pocv2_msg *msg);

int msg_recv_intr(u8 *src_mac, struct receive_buf *buf);

#define pocv2_broadcast_msg_init(msg, type, hdr, body, body_len)   \
  _pocv2_broadcast_msg_init(msg, type, (struct pocv2_msg_header *)hdr, body, body_len)

#define pocv2_msg_init2(msg, dst_nodeid, type, hdr, body, body_len)   \
  _pocv2_msg_init2(msg, dst_nodeid, type, (struct pocv2_msg_header *)hdr, body, body_len)

#define pocv2_msg_init(msg, dst_mac, type, hdr, body, body_len)   \
  _pocv2_msg_init(msg, dst_mac, type, (struct pocv2_msg_header *)hdr, body, body_len)

void _pocv2_broadcast_msg_init(struct pocv2_msg *msg, enum msgtype type,
                               struct pocv2_msg_header *hdr, void *body, int body_len);
void _pocv2_msg_init2(struct pocv2_msg *msg, int dst_nodeid, enum msgtype type,
                       struct pocv2_msg_header *hdr, void *body, int body_len);
void _pocv2_msg_init(struct pocv2_msg *msg, u8 *dst_mac, enum msgtype type,
                      struct pocv2_msg_header *hdr, void *body, int body_len);

void msg_sysinit(void);

void msgenqueue(struct pocv2_msg *msg);

int pocv2_recv_reply(struct pocv2_msg *msg, struct pocv2_msg_header *buf);

#endif
