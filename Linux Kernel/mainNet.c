#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/inet.h>

static struct net_device *vnet_dev;
static u32 vnet_ip = 0;

static ssize_t vnet_proc_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos) {
char buf[16];
if (count > sizeof(buf) - 1) return -EFAULT;
if (copy_from_user(buf, ubuf, count)) return -EFAULT;
buf[count] = '\0';

//ip
vnet_ip = in_aton(buf);

pr_info("vnet: IP %pI4\n", &vnet_ip);
return count;
}

static const struct proc_ops vnet_proc_ops = {
  .proc_write = vnet_proc_write,
};

static int vnet_open(struct net_device *dev) {
netif_start_queue(dev);
pr_info("vnet: open\n");
return 0;
}
static int vnet_stop(struct net_device *dev) {
netif_stop_queue(dev);
pr_info("vnet: stop\n");
return 0;
}
static netdev_tx_t vnet_xmit(struct sk_buff *skb, struct net_device *dev) {
struct iphdr *iph = ip_hdr(skb);

//check ip
 if (iph && iph->protocol == IPPROTO_ICMP) {
  struct icmphdr *icmph = icmp_hdr(skb);
  //check ping
  if(icmph->type == ICMP_ECHO) {
    pr_info("vnet: icmp for ip %pI4\n", &iph->daddr);
  }
 }

dev_kfree_skb(skb);
return NETDEV_TX_OK;
}
static const struct net_device_ops vnet_ops = {
.ndo_open = vnet_open,
.ndo_stop = vnet_stop,
.ndo_start_xmit = vnet_xmit,
};
static void vnet_setup(struct net_device *dev) {
ether_setup(dev);
dev->netdev_ops = &vnet_ops;
dev->flags |= IFF_NOARP;
dev->features |= NETIF_F_HW_CSUM;
strcpy(dev->name, "vnet0");
}
static int __init vnet_init(void) {
vnet_dev = alloc_netdev(0, "vnet%d", NET_NAME_UNKNOWN, vnet_setup);
  if (!vnet_dev)
    return -ENOMEM;
  
  if (register_netdev(vnet_dev)) {
    free_netdev(vnet_dev);
    return -ENODEV;
  }
  
proc_create("vnet_ip", 0666, NULL, &vnet_proc_ops);
  
pr_info("vnet: loaded\n");
return 0;
}
static void __exit vnet_exit(void) {
  if (vnet_dev) {
  unregister_netdev(vnet_dev);
  free_netdev(vnet_dev);
  } 
pr_info("vnet: unload\n");
}

module_init(vnet_init);
module_exit(vnet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("You");
MODULE_DESCRIPTION("Virtual interface");
