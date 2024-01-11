#define pr_fmt(fmt)     KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/aer.h>
/* include early, to verify it depends only on the headers above */
#include "libxdma_api.h"
#include "libxdma.h"
#include "nupanet_main.h"

#define DRV_MODULE_NAME          "nupanet"

#define USE_NO_WAIT              0

MODULE_AUTHOR("Clussys, Inc.");
MODULE_LICENSE("Dual BSD/GPL");

static const struct pci_device_id pci_ids[] = {
	{ PCI_DEVICE(0x10ee, 0x9048), },
	{ PCI_DEVICE(0x10ee, 0x9148), },
	{0,}
};
MODULE_DEVICE_TABLE(pci, pci_ids);

static void adapter_free(struct nupanet_adapter *adapter)
{
	struct xdma_dev *xdev;
	struct napi_struct *napi;
	int host_id = adapter->host_id;

	xdev = adapter->xdev;
	pr_info("adapter 0x%p, destroy_interfaces, xdev 0x%p.\n", adapter, xdev);
	adapter->xdev = NULL;
	napi = &adapter->napi;
	pr_info("adapter 0x%p, xdev 0x%p xdma_device_close.\n", adapter, xdev);
	xdma_device_close(adapter->pdev, xdev);
	netif_napi_del(napi);
	//clear my own info
	memset(adapter->shm_info.vaddr + (host_id * INFO_SIZE), 0, INFO_SIZE);
	iounmap(adapter->shm_info.vaddr);
	unregister_netdev(adapter->netdev);
	free_netdev(adapter->netdev);
}

static struct nupanet_adapter *adapter_alloc(struct pci_dev *pdev)
{
	struct nupanet_adapter *adapter;
	struct net_device *netdev;

	netdev = alloc_etherdev(sizeof(struct nupanet_adapter));
	if(!netdev)
		return NULL;

	adapter = netdev_priv(netdev);
	if (!adapter)
		return NULL;
	memset(adapter, 0, sizeof(*adapter));

	adapter->magic = MAGIC_DEVICE;
	adapter->pdev = pdev;
	adapter->user_max = MAX_USER_IRQ;
	adapter->h2c_channel_max = XDMA_CHANNEL_NUM_MAX;
	adapter->c2h_channel_max = XDMA_CHANNEL_NUM_MAX;
	adapter->netdev = netdev;

	return adapter;
}

int nupanet_open(struct net_device *dev)
{
    NUPA_DEBUG("nupanet_open called\n");
    return 0;
}

int nupanet_release(struct net_device *dev)
{
    NUPA_DEBUG("nupanet_release called\n");
    return 0;
}

int nupanet_close(struct net_device *netdev) 
{
    NUPA_DEBUG("nupanet_close\r\n");
    return 0; 
}

static void nupanet_set_rx_mode(struct net_device *netdev) 
{
    NUPA_DEBUG("nupanet_set_rx_mode\r\n");
}

/**
* Currently only support 16 hosts connection
*/
static char device_id_to_host_id(int dev_id)
{
    // 7138 ==> 1, 7238 ==> 2, 7338 ==> 3
    return (dev_id >> 8) & 0xF;
}

static int mac_addr_to_host_id(char* mac_addr)
{
    return mac_addr[5];
}


/** |========================================================================================================================================
*   |          |            |            |           |               |          |            |            |           |
*   |   info   |    desc    |    desc    |    ...    |               |   info   |    desc    |    desc    |    ...    |
*   |          |            |            |           |               |          |            |            |           |
*   |========================================================================================================================================
*/  

static struct packet_desc* nupa_data_available(struct nupanet_adapter* adapter, int host_id)
{
    //TODO: determine if need to process data
	char* shm_base;
	char* host_base;
	char* desc_base;
	struct packets_info* info;
	struct packet_desc* desc;
	int head, tail;

	//NUPA_DEBUG("nupa_data_available, host_id = %d", host_id);
	BUG_ON(host_id >= MAX_AGENT_NUM);
	shm_base = adapter->shm_info.vaddr;
	host_base = shm_base + INFO_SIZE * host_id;
	desc_base = host_base + sizeof(struct packets_info);
	info = (struct packets_info*)host_base;
	head = info->head;
	tail = info->tail;

	if(head == tail) {
		//NUPA_DEBUG("nupa_data_available, host_id = %d, desc empty, head = %d, tail =%d", host_id, head, tail);
		return NULL;
	}
	//fetch tail desc
	desc = (struct packet_desc*)desc_base + tail;

	while(desc->status == PACKET_FREEZING){
		NUPA_DEBUG(" desc at %d is FREEZING \r\n", tail);
	}
	//only cope with data settled
	if(desc->status == PACKET_SETTLED) {
		NUPA_DEBUG(" desc at %d is SETTLED \r\n", tail);
		info->tail = (tail + 1) % MAX_DESC_NUM;
		info->total_len -= DESC_MAX_DMA_SIZE;
	} else if(desc->status == PACKET_INIT || desc->status == PACKET_RELEASED){
		NUPA_DEBUG(" desc at %d is INIT or RELEASED \r\n", tail);
		desc = NULL;
	}
    return desc;
}

static int xdma_transfer_data(struct xdma_engine* engine, int offset, int length, char* buf, bool is_write)
{
	int res, i;
	struct xdma_dev *xdev;
	struct scatterlist *sg;
	unsigned int pages_nr;
	struct sg_table *sgt;

	NUPA_DEBUG("xdma_transfer_data , offset = %d , length = %d\r\n", offset, length);
	if(length > PAGE_SIZE) {
		NUPA_ERROR("skb linear length too big , length = %d \r\n", length);
		res = -EINVAL;
		return res;
	}
	sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	xdev = engine->xdev;
	pages_nr = (((unsigned long)buf + length + PAGE_SIZE - 1) - ((unsigned long)buf & PAGE_MASK)) >> PAGE_SHIFT;
	if (pages_nr == 0) {
		NUPA_ERROR("page_nr = %d \r\n", pages_nr);
		res = -EINVAL;
		goto out;
	}
	if (sg_alloc_table(sgt, pages_nr, GFP_KERNEL)) {
		NUPA_ERROR("sgl OOM.\n");
		res = -ENOMEM;
		goto out;
	}
	sg = &sgt->sgl[0];
	for (i = 0; i < pages_nr; i++) {
		unsigned int offset = offset_in_page(buf);
		unsigned int nbytes = min_t(unsigned int, PAGE_SIZE - offset, length);
		sg_set_buf(sg, buf, nbytes);
		buf += nbytes;
		length -= nbytes;
		sg = sg_next(sg);
	}
#if USE_NO_WAIT
	res = xdma_xfer_submit_nowait(NULL, xdev, engine->channel, is_write, offset, sgt, false, 0);
#else
	res = xdma_xfer_submit(xdev, engine->channel, is_write, offset, sgt, false, 0);
#endif
	if(is_write) {
		NUPA_DEBUG("write: transferred %d \r\n", res);
	} else {
		NUPA_DEBUG("read: transferred %d \r\n", res);
	}
out:
    return res;
}

static struct sk_buff * xdma_receive_data(struct nupanet_adapter* adapter, struct packet_desc* desc)
{
	int res;
	unsigned char* buf;
	struct xdma_engine* engine;
	struct sk_buff *skb;
	int dump_ctl;
	int hack_ctl;
	int offset, length;

	offset = desc->offset;
	length = desc->length;

	engine = &adapter->xdev->engine_c2h[0];
	skb = napi_alloc_skb(&adapter->napi, length);


	dump_ctl = adapter->debug.dump_ctl;
	hack_ctl = adapter->debug.hack_ctl;

	//do we really need to allocate data here?
	buf = kmalloc(length, GFP_KERNEL);
	if(!buf) {
		NUPA_ERROR("allocate buf failed \r\n");
		return NULL;
	}
	res = xdma_transfer_data(engine, offset, length, buf, false);
	NUPA_DEBUG("xdma_receive_data, res = %d, expect %d\r\n", res, length);
	if(hack_ctl && (length==42)) {
		buf[32] = 0;
		buf[33] = 0;
		buf[34] = 0;
		buf[35] = 0;
		buf[36] = 0;
		buf[37] = 0;
	
		buf[38] = 0xC0;
		buf[39] = 0xa8;
		buf[40] = 0x08;
		buf[41] = 0x64;
	}
	if(dump_ctl) {
		NUPA_DEBUG("xdma_receive_data, desc->pos =  %d, offset = %d, length = %d \r\n", desc->pos, desc->offset, desc->length);
		print_hex_dump(KERN_DEBUG, "rcv : ", DUMP_PREFIX_OFFSET, 16, 1, buf, length, false);
	}
	skb_put_data(skb, buf, length);
#if 0
	if(res == length) {
		skb_put_data(skb, buf, length);
	}
	else {
		skb = NULL;
		NUPA_ERROR("dma read res = %d, expect %d\r\n", res, length);
	}
#endif
	kfree(buf);
	return skb;
}

static int xdma_send_data(struct nupanet_adapter *adapter, struct sk_buff *skb, struct packet_desc* desc)
{
	unsigned char* buf;
	struct xdma_engine* engine;
	int offset, length;
	int dump_ctl;

	dump_ctl = adapter->debug.dump_ctl;
	engine = &adapter->xdev->engine_h2c[0];
	offset = desc->offset;
	length = desc->length;
	// do we really need to allocate mem here?
	buf = kmalloc(length, GFP_KERNEL);
	memcpy(buf, skb->data, length);
	if(dump_ctl)
		print_hex_dump(KERN_DEBUG, "send_buf: ", DUMP_PREFIX_OFFSET, 16, 1, buf, length, false);
	NUPA_DEBUG("xdma_send_data, desc->pos = %d, desc->offset = %d, desc->length = %d \r\n",desc->pos, desc->offset, desc->length);
	kfree(buf);
	return xdma_transfer_data(engine, offset, length, buf, true);
}

static bool mac_addr_valid(char* mac_addr)
{
	return true;
}

struct packet_desc* fetch_packet_desc(struct nupanet_adapter *adapter, int dst_id, int length)
{
	struct packet_desc* desc;
	char* shm_base;
	char* host_base;
	char* desc_base;
	struct packets_info* info;
	int head, tail;
	bool ready;

	int total_len;

	BUG_ON(dst_id >= MAX_AGENT_NUM);
	shm_base = adapter->shm_info.vaddr;
	host_base = shm_base + INFO_SIZE * dst_id;
	desc_base = host_base + sizeof(struct packets_info);
	info = (struct packets_info*)host_base;
	head = info->head;
	tail = info->tail;
	ready = info->ready;

	if(!ready) {
		NUPA_DEBUG("fetch_packet_desc, dst_id = %d, not ready \r\n", dst_id);
		return NULL;
	}
	desc = (struct packet_desc*)desc_base + head;
	total_len += DESC_MAX_DMA_SIZE;
	if(total_len <= AGENT_TOTAL_DMA_SIZE) {
		if (((head + 1) & (MAX_DESC_NUM -1)) == tail) {
			NUPA_DEBUG("fetch_packet_desc, desc full");
			desc = NULL;
		} else {
			info->total_len += length;
			desc->status = PACKET_FREEZING;
			info->head = (head + 1) % MAX_DESC_NUM;
			desc->offset = (AGENT_TOTAL_DMA_SIZE * dst_id) + (desc->pos * DESC_MAX_DMA_SIZE);
			NUPA_DEBUG("fetch_packet_desc, dst_id = %d, desc->pos = %d, info->head = %d, desc->offset = %d, desc->length = %d \r\n", dst_id, desc->pos, head, desc->offset, desc->length);
		}
	} else {
		NUPA_ERROR("Agent data full \r\n");
		desc = NULL;
	}
	return desc;
}

void adapter_info_init(struct nupanet_adapter *adapter)
{
	char* shm_base;
	char* host_base;
	char* desc_base;
	int host_id;
	int pos = 0;
	struct packets_info* info;
	struct packet_desc* desc;

	host_id = adapter->host_id;
	NUPA_DEBUG("adapter_info_init, host_id = %d", host_id);
	NUPA_DEBUG("adapter_info_init, sizeof(struct packets_info) = %ld, sizeof(struct packet_desc) = %ld \r\n", sizeof(struct packets_info), sizeof(struct packet_desc));
	BUG_ON(host_id >= MAX_AGENT_NUM);
	shm_base = adapter->shm_info.vaddr;
	host_base = shm_base + INFO_SIZE * host_id;
	//clean my shm space first
	memset(host_base , 0, INFO_SIZE);
	info = (struct packets_info*)host_base;
	info->head = info->tail = 0;
	//set ready ?
	info->ready = true;
	info->host_id = host_id;
	desc_base = host_base + sizeof(struct packets_info);
	memset(desc_base, 0, MAX_DESC_NUM * sizeof(struct packet_desc));
	for(pos = 0,desc = (struct packet_desc*)desc_base; pos < MAX_DESC_NUM; pos++, desc++) {
		desc->pos = pos;
	}
}

static netdev_tx_t nupanet_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
    char *dest_mac_addr_p;
    char *src_mac_addr_p;
	char* type;
    int dst_id, this_id;
    struct nupanet_adapter *adapter;
	int offset, length;
	struct packet_desc* desc;
	int i;
	int dump_ctl;

	NUPA_DEBUG("nupanet_xmit_frame\r\n");

    adapter = netdev_priv(netdev);
	dump_ctl = adapter->debug.dump_ctl;

    dest_mac_addr_p = (char *)skb->data;
    src_mac_addr_p = (char *)skb->data + ETH_ALEN;

	type = src_mac_addr_p + ETH_ALEN;

    NUPA_DEBUG("DEST: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\r\n",dest_mac_addr_p[0] & 0xFF,dest_mac_addr_p[1] & 0xFF,dest_mac_addr_p[2] & 0xFF, dest_mac_addr_p[3] & 0xFF,dest_mac_addr_p[4] & 0xFF,dest_mac_addr_p[5] & 0xFF);
    NUPA_DEBUG("SRC: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\r\n",src_mac_addr_p[0] & 0xFF,src_mac_addr_p[1] & 0xFF,src_mac_addr_p[2] & 0xFF, src_mac_addr_p[3] & 0xFF,src_mac_addr_p[4] & 0xFF,src_mac_addr_p[5] & 0xFF);

	NUPA_DEBUG("Type: 0x%.2x%.2x \r\n", type[0] & 0xFF, type[1] & 0xFF);
    if (is_broadcast_ether_addr(dest_mac_addr_p) || is_multicast_ether_addr(dest_mac_addr_p)) {
        NUPA_ERROR("broadcast and multicast currently not supported ,will support later \r\n");
        for(i = 0; i< MAX_AGENT_NUM; i++) {
			if(i != adapter->host_id) {
				dst_id = i;
				break;
			}
		}
		goto broad;
    }
    // should we check dest mac is online or not?
    // later, we should read reg to get current host ID

    dst_id = mac_addr_to_host_id(dest_mac_addr_p);
broad:
    this_id = mac_addr_to_host_id(src_mac_addr_p);

	if((!mac_addr_valid(src_mac_addr_p)) || (!mac_addr_valid(dest_mac_addr_p))) {
		NUPA_DEBUG("Address Not Valid \r\n");
		return NETDEV_TX_OK;
	}

    NUPA_DEBUG("[XMIT] skb->len = %d , skb_headlen(skb) = %d, dst_id =%d \r\n ", skb->len, skb_headlen(skb), dst_id);
	if(skb_headlen(skb) > DESC_MAX_DMA_SIZE) {
		NUPA_ERROR("skb linear length too big  \r\n");
		goto out;
	}
	if(dump_ctl)
		print_hex_dump(KERN_DEBUG, "skb->data: ", DUMP_PREFIX_OFFSET, 16, 1, (char *)skb->data, skb_headlen(skb), false);
	//currently only cope with linear data
	length = skb_headlen(skb);
	desc = fetch_packet_desc(adapter, dst_id, length);
	if (!desc) {
		NUPA_ERROR("fetch_packet_desc fail \r\n");
		goto out;
	}
	//offset should be fetched from dst INFO area
	offset = desc->offset;
	desc->length = length;

    xdma_send_data(adapter, skb, desc);

	//schedule_work(&adapter->xmit_task);

	//notify buddy, change to interrupt mechanism later 
	//nupa_notify_data_available(adapter, dst_id);
	desc->status = PACKET_SETTLED;

out:
    //2. free skb
    dev_kfree_skb(skb);
    NUPA_DEBUG("[XMIT]free skb done\r\n");
    return NETDEV_TX_OK;
}

static int nupanet_set_mac(struct net_device *netdev, void *p)
{
	int host_id, dev_id;
	struct pci_dev *pdev;
	static char mac_addr[ETH_ALEN] = NUPANET_DEFAULT_BASE_MAC_ADDR;
    // Get Device ID, generate MAC according to Device ID
    // Change to reg version later 
    pdev = to_pci_dev(netdev->dev.parent);
    dev_id = pdev->device;
	host_id = (int)device_id_to_host_id(dev_id);
	NUPA_DEBUG("nupanet_set_mac, dev_id = %#x , host_id = %d\r\n", dev_id, host_id);
    mac_addr[5] = device_id_to_host_id(dev_id);
    // Set MAC address
    netdev->dev_addr = mac_addr;
	NUPA_DEBUG("nupanet_set_mac done\r\n");
	return 0;
}

#if KERNEL_VERSION(5, 0, 1) <= LINUX_VERSION_CODE
static void nupanet_tx_timeout(struct net_device *dev, unsigned int txqueue)
{
    NUPA_DEBUG("nupanet_tx_timeout\r\n");
}
#else
static void nupanet_tx_timeout(struct net_device *dev)
{
    NUPA_DEBUG("nupanet_tx_timeout\r\n");
}
#endif

static int nupanet_change_mtu(struct net_device *dev, int new_mtu)
{
	NUPA_DEBUG("nupanet_change_mtu , change to %d,\r\n", new_mtu);
	return 0;
}

static int nupanet_validate_addr(struct net_device *dev)
{
    NUPA_DEBUG("nupanet_validate_addr\r\n");
	return 0;
}

#if KERNEL_VERSION(5, 0, 1) <= LINUX_VERSION_CODE
static int nupanet_eth_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    NUPA_DEBUG("nupanet_eth_ioctl, cmd = %d\r\n", cmd);
	return 0;
}
#endif

static int nupanet_do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    NUPA_DEBUG("nupanet_do_ioctl, cmd = %d\r\n", cmd);
	return 0;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
/* Polling 'interrupt' - used by things like netconsole to send skbs
 * without having to re-enable interrupts. It's not called while
 * the interrupt routine is executing.
 */
static void nupanet_netpoll(struct net_device *netdev)
{
}
#endif

static const struct net_device_ops nupanet_netdev_ops = {
    .ndo_open = nupanet_open,
    .ndo_stop = nupanet_close,
    .ndo_start_xmit = nupanet_xmit_frame,
    .ndo_set_rx_mode = nupanet_set_rx_mode,
    .ndo_set_mac_address = nupanet_set_mac,
    .ndo_tx_timeout		= nupanet_tx_timeout,
    .ndo_change_mtu		= nupanet_change_mtu,
	.ndo_do_ioctl		= nupanet_do_ioctl,
#if KERNEL_VERSION(5, 0, 1) <= LINUX_VERSION_CODE
    .ndo_eth_ioctl		= nupanet_eth_ioctl,
#endif
    .ndo_validate_addr	= nupanet_validate_addr,
#ifdef CONFIG_NET_POLL_CONTROLLER
    .ndo_poll_controller	= nupanet_netpoll,
#endif
};

static int nupanet_poll(struct napi_struct *napi, int budget)
{
    struct sk_buff *skb;
    int work_done = 0;
    gro_result_t gro_ret;
    struct nupanet_adapter* adapter;
	struct packet_desc* desc;
    int host_id;
    //NUPA_DEBUG("nupanet_poll\r\n");
	adapter = container_of(napi, struct nupanet_adapter, napi);
    host_id = adapter->host_id;
    while(1) {
        //TODO:check if need to process packet
        if((desc = nupa_data_available(adapter, host_id))) {
            skb = xdma_receive_data(adapter, desc);
            if(!skb) {
                break;
            }
			desc->status = PACKET_RELEASED;
        } else {
            break;
        }
        skb->protocol = eth_type_trans(skb, napi->dev);
        gro_ret = napi_gro_receive(napi, skb);

        if(++work_done > budget) {
            break;
        }
    }
	if (work_done == budget)
		return budget;
	napi_complete_done(napi, work_done);
    return work_done;
}

static int nupanet_poll_thread(void *data)
{
	struct nupanet_adapter *adapter = data;
	while (1) {
		if (kthread_should_stop())
			break;
		napi_schedule(&adapter->napi);
		cond_resched();
	}
	NUPA_DEBUG("nupanet_poll_thread stopped\n");
	return 0;
}

static void nupanet_poll_start(struct nupanet_adapter *adapter)
{
	struct napi_struct *napi = &adapter->napi;
	napi_enable(napi);
	napi->thread = kthread_run(nupanet_poll_thread, adapter, "nupanet_poll_thread");
}


static void nupanet_xmit_task(struct work_struct *work)
{
#if 0
	struct nupanet_adapter *adapter = container_of(work, struct nupanet_adapter, xmit_task);
	NUPA_DEBUG("XMIT WORK \r\n");

	//xdma_send_data(&adapter->xdev->engine_h2c[0], skb, offset, length);
#endif
}

static void nupanet_rcv_task(struct work_struct *work)
{
#if 0
	struct nupanet_adapter *adapter = container_of(work, struct nupanet_adapter, rcv_task);
#endif
}


static int probe_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int rv = 0;
	struct nupanet_adapter *adapter;
	struct xdma_dev *xdev;
	void *hndl;
    struct net_device *netdev;
    int err;

	NUPA_DEBUG("probe_one, vendor_id = %#x, device_id = %#x \r\n", pdev->vendor, pdev->device);

	adapter = adapter_alloc(pdev);
	if (!adapter) {
		err = -ENOMEM;
		goto err_alloc_etherdev;
	}

    netdev = adapter->netdev;
	SET_NETDEV_DEV(netdev, &pdev->dev);


	hndl = xdma_device_open(DRV_MODULE_NAME, pdev, &adapter->user_max, &adapter->h2c_channel_max, &adapter->c2h_channel_max);
	if (!hndl) {
		err = -EINVAL;
		goto err_out;
	}

	if (adapter->user_max > MAX_USER_IRQ) {
		pr_err("Maximum users limit reached\n");
		err = -EINVAL;
		goto err_out;
	}

	if (adapter->h2c_channel_max > XDMA_CHANNEL_NUM_MAX) {
		pr_err("Maximun H2C channel limit reached\n");
		err = -EINVAL;
		goto err_out;
	}

	if (adapter->c2h_channel_max > XDMA_CHANNEL_NUM_MAX) {
		pr_err("Maximun C2H channel limit reached\n");
		err = -EINVAL;
		goto err_out;
	}

	if (!adapter->h2c_channel_max && !adapter->c2h_channel_max)
		pr_warn("NO engine found!\n");

	if (adapter->user_max) {
		u32 mask = (1 << (adapter->user_max + 1)) - 1;

		err = xdma_user_isr_enable(hndl, mask);
		if (err)
			goto err_out;
	}

	/* make sure no duplicate */
	xdev = xdev_find_by_pdev(pdev);
	if (!xdev) {
		pr_warn("NO xdev found!\n");
		err =  -EINVAL;
		goto err_out;
	}

	if (hndl != xdev) {
		pr_err("xdev handle mismatch\n");
		err =  -EINVAL;
		goto err_out;
	}

	pr_info("%s xdma%d, pdev 0x%p, xdev 0x%p, 0x%p, usr %d, ch %d,%d.\n", dev_name(&pdev->dev), xdev->idx, pdev, adapter, xdev, adapter->user_max, adapter->h2c_channel_max, adapter->c2h_channel_max);

	adapter->xdev = hndl;

	NUPA_DEBUG("user %d, config %d, bypass %d.\n", xdev->user_bar_idx, xdev->config_bar_idx, xdev->bypass_bar_idx);
	adapter->shm_info.vaddr = pci_ioremap_wc_bar(pdev, xdev->user_bar_idx);
    adapter->shm_info.length = pci_resource_len(pdev, xdev->user_bar_idx);

#if HAS_DEBUG_CHAR_DEV
	create_debug_cdev(&adapter->debug, adapter->shm_info.vaddr, adapter->shm_info.length);
#endif

	adapter->host_id = (int)device_id_to_host_id(pdev->device);

	adapter_info_init(adapter);

	NUPA_DEBUG("XDMA Init Done \r\n");
    netdev->netdev_ops = &nupanet_netdev_ops;

	NUPA_DEBUG("set mac and name \r\n");
	nupanet_set_mac(netdev, NULL);
	strcpy(netdev->name, "nupanet%d");
	NUPA_DEBUG("netif_napi_add \r\n");
    netif_napi_add(netdev, &adapter->napi, nupanet_poll, 64);

	INIT_WORK(&adapter->xmit_task, nupanet_xmit_task);
	INIT_WORK(&adapter->rcv_task, nupanet_rcv_task);

    err = register_netdev(netdev);
    NUPA_DEBUG("register_netdev Done , err = %d\r\n", err);
	if (err)
		goto err_out;

	dev_set_drvdata(&pdev->dev, adapter);
	nupanet_poll_start(adapter);

	return 0;

err_out:
	pr_err("pdev 0x%p, err %d.\n", pdev, rv);
	adapter_free(adapter);
err_alloc_etherdev:
	return err;
}

static void remove_one(struct pci_dev *pdev)
{
	struct nupanet_adapter *adapter;

	if (!pdev)
		return;

	adapter = dev_get_drvdata(&pdev->dev);
	if (!adapter)
		return;

	pr_info("pdev 0x%p, xdev 0x%p, 0x%p.\n",pdev, adapter, adapter->xdev);
#if HAS_DEBUG_CHAR_DEV
	delete_debug_cdev(&adapter->debug);
#endif
	dev_set_drvdata(&pdev->dev, NULL);
	adapter_free(adapter);
}

static pci_ers_result_t xdma_error_detected(struct pci_dev *pdev,
					pci_channel_state_t state)
{
	struct nupanet_adapter *adapter = dev_get_drvdata(&pdev->dev);

	switch (state) {
	case pci_channel_io_normal:
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		pr_warn("dev 0x%p,0x%p, frozen state error, reset controller\n",
			pdev, adapter);
		xdma_device_offline(pdev, adapter->xdev);
		pci_disable_device(pdev);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		pr_warn("dev 0x%p,0x%p, failure state error, req. disconnect\n",
			pdev, adapter);
		return PCI_ERS_RESULT_DISCONNECT;
	}
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t xdma_slot_reset(struct pci_dev *pdev)
{
	struct nupanet_adapter *adapter = dev_get_drvdata(&pdev->dev);

	pr_info("0x%p restart after slot reset\n", adapter);
	if (pci_enable_device_mem(pdev)) {
		pr_info("0x%p failed to renable after slot reset\n", adapter);
		return PCI_ERS_RESULT_DISCONNECT;
	}

	pci_set_master(pdev);
	pci_restore_state(pdev);
	pci_save_state(pdev);
	xdma_device_online(pdev, adapter->xdev);

	return PCI_ERS_RESULT_RECOVERED;
}

static void xdma_error_resume(struct pci_dev *pdev)
{
	struct nupanet_adapter *adapter = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p.\n", pdev, adapter);
#if PCI_AER_NAMECHANGE
	pci_aer_clear_nonfatal_status(pdev);
#else
	pci_cleanup_aer_uncorrect_error_status(pdev);
#endif

}

#if KERNEL_VERSION(4, 13, 0) <= LINUX_VERSION_CODE
static void xdma_reset_prepare(struct pci_dev *pdev)
{
	struct nupanet_adapter *adapter = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p.\n", pdev, adapter);
	xdma_device_offline(pdev, adapter->xdev);
}

static void xdma_reset_done(struct pci_dev *pdev)
{
	struct nupanet_adapter *adapter = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p.\n", pdev, adapter);
	xdma_device_online(pdev, adapter->xdev);
}

#elif KERNEL_VERSION(3, 16, 0) <= LINUX_VERSION_CODE
static void xdma_reset_notify(struct pci_dev *pdev, bool prepare)
{
	struct nupanet_adapter *adapter = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p, prepare %d.\n", pdev, adapter, prepare);

	if (prepare)
		xdma_device_offline(pdev, adapter->xdev);
	else
		xdma_device_online(pdev, adapter->xdev);
}
#endif

static const struct pci_error_handlers xdma_err_handler = {
	.error_detected	= xdma_error_detected,
	.slot_reset	= xdma_slot_reset,
	.resume		= xdma_error_resume,
#if KERNEL_VERSION(4, 13, 0) <= LINUX_VERSION_CODE
	.reset_prepare	= xdma_reset_prepare,
	.reset_done	= xdma_reset_done,
#elif KERNEL_VERSION(3, 16, 0) <= LINUX_VERSION_CODE
	.reset_notify	= xdma_reset_notify,
#endif
};

static struct pci_driver pci_driver = {
	.name = DRV_MODULE_NAME,
	.id_table = pci_ids,
	.probe = probe_one,
	.remove = remove_one,
	.err_handler = &xdma_err_handler,
};

static int nupanet_init(void)
{
	return pci_register_driver(&pci_driver);
}

static void nupanet_exit(void)
{
	/* unregister this driver from the PCI bus driver */
	dbg_init("pci_unregister_driver.\n");
	pci_unregister_driver(&pci_driver);
}

module_init(nupanet_init);
module_exit(nupanet_exit);
