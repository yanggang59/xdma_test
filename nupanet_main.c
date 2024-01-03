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
#undef DEBUG_THIS_MODULE
#define DEBUG_THIS_MODULE          1
#if DEBUG_THIS_MODULE
#define NUPA_DEBUG(fmt,...)             printk("[NUPA DEBUG] "fmt, ##__VA_ARGS__)
#else
#define NUPA_DEBUG(fmt,...)
#endif

MODULE_AUTHOR("Clussys, Inc.");
MODULE_LICENSE("Dual BSD/GPL");

#define SHARING_BAR                0

static const struct pci_device_id pci_ids[] = {
	{ PCI_DEVICE(0x10ee, 0x9148), },
	{0,}
};
MODULE_DEVICE_TABLE(pci, pci_ids);

static void adapter_free(struct nupanet_adapter *adapter)
{
	struct xdma_dev *xdev = adapter->xdev;

	pr_info("adapter 0x%p, destroy_interfaces, xdev 0x%p.\n", adapter, xdev);
	adapter->xdev = NULL;
	pr_info("adapter 0x%p, xdev 0x%p xdma_device_close.\n", adapter, xdev);
	xdma_device_close(adapter->pdev, xdev);
	kfree(adapter->netdev->dev_addr);
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
    printk("nupanet_open called\n");
    //netif_start_queue(dev);
    return 0;
}

int nupanet_release(struct net_device *dev)
{
    printk("nupanet_release called\n");
    //netif_stop_queue(dev);
    return 0;
}

int nupanet_close(struct net_device *netdev) 
{
    printk("[DEBUG] nupanet_close\r\n");
    return 0; 
}

static void nupanet_set_rx_mode(struct net_device *netdev) 
{
    printk("[DEBUG] nupanet_set_rx_mode\r\n");
}

/**
* Currently only support 16 hosts connection
*/
static char dev_id_to_host_id(int dev_id)
{
    // 7138 ==> 1, 7238 ==> 2, 7338 ==> 3
    return (dev_id > 8) & 0xF;
}

static int mac_addr_to_host_id(char* mac_addr)
{
    return mac_addr[5];
}

static unsigned long nupa_data_available(int host_id)
{
    //TODO: determine if need to process data
    return 0;
}

static struct sk_buff * xdma_receive_data(struct xdma_engine* engine, unsigned long length)
{
    return NULL;
}

static void sync_with_peer(struct xdma_engine* engine, int dst_id)
{
    //1.Get current head

    //2.

}

void start_dma(void)
{

}

static void xdma_send_data(struct xdma_engine* engine, struct sk_buff *skb)
{
    sync_with_peer(NULL, 0);
    return;
}

static netdev_tx_t nupanet_xmit_frame(struct sk_buff *skb,
                                      struct net_device *netdev)
{
    char *dest_mac_addr_p;
    char *src_mac_addr_p;
    int dst_id, this_id;
    struct nupanet_adapter *adapter;

	NUPA_DEBUG("nupanet_xmit_frame\r\n");

    adapter = netdev_priv(netdev);

    dest_mac_addr_p = (char *)skb->data;
    src_mac_addr_p = (char *)skb->data + ETH_ALEN;

    NUPA_DEBUG("DEST: %x:%x:%x:%x:%x:%x\r\n",dest_mac_addr_p[0] & 0xFF,dest_mac_addr_p[1] & 0xFF,dest_mac_addr_p[2] & 0xFF, dest_mac_addr_p[3] & 0xFF,dest_mac_addr_p[4] & 0xFF,dest_mac_addr_p[5] & 0xFF);
    NUPA_DEBUG("SRC: %x:%x:%x:%x:%x:%x\r\n",src_mac_addr_p[0] & 0xFF,src_mac_addr_p[1] & 0xFF,src_mac_addr_p[2] & 0xFF, src_mac_addr_p[3] & 0xFF,src_mac_addr_p[4] & 0xFF,src_mac_addr_p[5] & 0xFF);

    // if (is_broadcast_ether_addr(dest_mac_addr_p) || is_multicast_ether_addr(dest_mac_addr_p)) {
    //     printk("[Error] broadcast and multicast currently not supported ,will support later\r\n");
    //     return NET_XMIT_DROP;
    // }
    // should we check dest mac is online or not?
    // later, we should read reg to get current host ID
    dst_id = mac_addr_to_host_id(dest_mac_addr_p);
    this_id = mac_addr_to_host_id(src_mac_addr_p);


    NUPA_DEBUG("[XMIT] skb->len = %d , skb_headlen(skb) = %d", skb->len,skb_headlen(skb));
    xdma_send_data(&adapter->xdev->engine_h2c[0], skb);

    //2. free skb
    dev_kfree_skb(skb);
    NUPA_DEBUG("[XMIT]free skb done\r\n");
    return NETDEV_TX_OK;
}

static int nupanet_set_mac(struct net_device *netdev, void *p)
{
	int host_id;
    char* mac_addr = kmalloc(ETH_ALEN, GFP_KERNEL);
	char default_mac[ETH_ALEN] = NUPANET_DEFAULT_BASE_MAC_ADDR;
    // Get Device ID, generate MAC according to Device ID
    // Change to reg version later 
    struct pci_dev *pdev = to_pci_dev(netdev->dev.parent);
    int dev_id = pdev->device;
	memcpy(mac_addr, default_mac, ETH_ALEN);
	host_id = (int)dev_id_to_host_id(dev_id);
	NUPA_DEBUG("nupanet_set_mac, dev_id = %#x , host_id = %d\r\n", dev_id, host_id);
    mac_addr[5] = dev_id_to_host_id(dev_id);
    // Set MAC address
    netdev->dev_addr = mac_addr;
	NUPA_DEBUG("nupanet_set_mac done\r\n");
	return 0;
}

static void nupanet_tx_timeout(struct net_device *dev, unsigned int txqueue)
{
    NUPA_DEBUG("nupanet_tx_timeout\r\n");
}

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

static int nupanet_eth_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    NUPA_DEBUG("nupanet_eth_ioctl, cmd = %d\r\n", cmd);
	return 0;
}

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
    .ndo_set_mac_address	= nupanet_set_mac,
    .ndo_tx_timeout		= nupanet_tx_timeout,
    .ndo_change_mtu		= nupanet_change_mtu,
	.ndo_do_ioctl		= nupanet_do_ioctl,
    .ndo_eth_ioctl		= nupanet_eth_ioctl,
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
    unsigned long length = 0;
    int host_id;
    NUPA_DEBUG("nupanet_poll\r\n");
	adapter = container_of(napi, struct nupanet_adapter, napi);
    host_id = adapter->host_id;
    while(1) {
        //TODO:check if need to process packet
        if((length = nupa_data_available(host_id))) {
            skb = xdma_receive_data(&adapter->xdev->engine_c2h[0], length);
            if(!skb) {
                break;
            }
        } else {
            break;
        }
        skb->protocol = eth_type_trans(skb, napi->dev);
        gro_ret = napi_gro_receive(napi, skb);

        if(++work_done > budget) {
            break;
        }
    }
    return 0;
}

static int probe_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int rv = 0;
	struct nupanet_adapter *adapter = NULL;
	struct xdma_dev *xdev;
	void *hndl;
    struct net_device *netdev;
    int err;

	int dev_id = pdev->device;
	NUPA_DEBUG("probe_one, dev_id = %#x \r\n", dev_id);

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

	pr_info("%s xdma%d, pdev 0x%p, xdev 0x%p, 0x%p, usr %d, ch %d,%d.\n",
		dev_name(&pdev->dev), xdev->idx, pdev, adapter, xdev,
		adapter->user_max, adapter->h2c_channel_max,
		adapter->c2h_channel_max);

	adapter->xdev = hndl;
#if HAS_DEBUG_CHAR_DEV
	create_debug_cdev(&adapter->debug);
#endif

	adapter->shm_info.vaddr = pci_ioremap_wc_bar(pdev, SHARING_BAR);
    adapter->shm_info.length = pci_resource_len(pdev, SHARING_BAR);

	NUPA_DEBUG("XDMA Init Done \r\n");
    netdev->netdev_ops = &nupanet_netdev_ops;

	NUPA_DEBUG("set mac and name \r\n");
	nupanet_set_mac(netdev, NULL);
	strcpy(netdev->name, "nupa_net0");
	NUPA_DEBUG("netif_napi_add \r\n");
    netif_napi_add(netdev, &adapter->napi, nupanet_poll, 64);

    err = register_netdev(netdev);
    NUPA_DEBUG("register_netdev Done , err = %d\r\n", err);
	if (err)
		goto err_out;

	dev_set_drvdata(&pdev->dev, adapter);

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
