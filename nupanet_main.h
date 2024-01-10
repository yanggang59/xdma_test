/*
 * This file is part of the Xilinx DMA IP Core driver for Linux
 *
 * Copyright (c) 2016-present,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#ifndef __XDMA_MODULE_H__
#define __XDMA_MODULE_H__

#include <linux/types.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/poll.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/aio.h>
#include <linux/splice.h>
#include <linux/version.h>
#include <linux/uio.h>
#include <linux/spinlock_types.h>
#include <linux/stddef.h>
#include <asm/byteorder.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/pagemap.h>
#include <linux/bitops.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/capability.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <net/pkt_sched.h>
#include <linux/list.h>
#include <linux/reboot.h>
#include <net/checksum.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>

#include "libxdma.h"
#include "xdma_thread.h"
#include "debug.h"

#define MAGIC_ENGINE                      0xEEEEEEEEUL
#define MAGIC_DEVICE                      0xDDDDDDDDUL
#define MAGIC_CHAR                        0xCCCCCCCCUL
#define MAGIC_BITSTREAM                   0xBBBBBBBBUL

#define HAS_DEBUG_CHAR_DEV                1
#define NUPANET_DEFAULT_BASE_MAC_ADDR     {0xFC, 0xAF, 0xAC, 0x00, 0x00, 0x00}

#undef DEBUG_THIS_MODULE
#define DEBUG_THIS_MODULE                 1
#if DEBUG_THIS_MODULE
#define NUPA_DEBUG(fmt,...)               printk("[NUPA DEBUG] "fmt, ##__VA_ARGS__)
#define NUPA_ERROR(fmt,...)               printk("[NUPA ERROR] "fmt, ##__VA_ARGS__)
#else
#define NUPA_DEBUG(fmt,...)
#define NUPA_ERROR(fmt,...)
#endif

//MAX DMA Size id 32K
#define MAX_DMA_SIZE                     (8 << 12)

//Truly usable mem size is 32K
#define USR_BAR_SHARING_SIZE              (1 << 12) //4K
#define MAX_AGENT_NUM                     2
#define INFO_SIZE                         (USR_BAR_SHARING_SIZE / MAX_AGENT_NUM)
#define MAX_DESC_NUM                      8

#define AGENT_MAX_DATA_SIZE               (MAX_DMA_SIZE / MAX_AGENT_NUM)
#define DESC_MAX_DMA_SIZE                 (AGENT_MAX_DATA_SIZE / MAX_DESC_NUM)

enum packet_desc_status{
	PACKET_INIT,
	PACKET_FREEZING,
	PACKET_SETTLED,
	PACKET_RELEASED,
};

struct packet_desc {
	int pos;
	int offset;
	int length;
	int next;
	int prev;
	enum packet_desc_status status;
};

//maybe we should judge here when desc is too big

// #if (sizeof(struct packet_desc) * MAX_DESC_NUM) > INFO_SIZE
// #error "too many packet_descs" 
// #endif

struct packets_info {
	int host_id;
	int head;
	int tail;
	int total_len;
	bool ready;
};

struct pci_shm_info {
	char __iomem* vaddr;
	int length;
};

struct nupanet_adapter {
	unsigned long magic;		/* structure ID for sanity checks */
	struct pci_dev *pdev;	/* pci device struct from probe() */
	struct xdma_dev *xdev;
	int major;		/* major number */
	int instance;		/* instance number */
	int user_max;
	int c2h_channel_max;
	int h2c_channel_max;
	unsigned int flags;

	struct pci_shm_info shm_info;

	struct net_device *netdev;
	struct napi_struct napi;
	struct work_struct xmit_task;
	struct work_struct rcv_task;
	int host_id;
#if HAS_DEBUG_CHAR_DEV
	struct debug_cdev debug;
#endif
};
#endif /* ifndef __XDMA_MODULE_H__ */
