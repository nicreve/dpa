/* dpa_main.c
 * DPA主模块
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#include "dpa_k.h"
#include "dpa_mem.h"

struct dpa_mem_allocator *allocator;

uint32_t dpa_total_buffers;
char *dpa_buffer_base;

/* 最大支持的接口数量 */
uint32_t dpa_num_ifs = DPA_DEF_IF_NUM;
module_param_named(num_interfaces, dpa_num_ifs, int, S_IRUGO);
MODULE_PARM_DESC(num_interfaces, "Maxmium number of interfaces that DPA supports, "
    "default " __stringify(DPA_DEF_IF_NUM) ", "
    "maxmium " __stringify(DPA_MAX_IF_NUM) ", "
    "minimum " __stringify(DPA_MIN_IF_NUM));

/* 一个接口最大支持的队列数量 */
uint32_t dpa_num_queues = DPA_DEF_QUEUE_NUM;
module_param_named(num_queues, dpa_num_queues, int, S_IRUGO);
MODULE_PARM_DESC(num_queues, "Maxmium number of queues per interface, "
    "default " __stringify(DPA_DEF_QUEUE_NUM) ", "
    "maxmium " __stringify(DPA_MAX_QUEUE_NUM) ", "
    "minimum " __stringify(DPA_MIN_QUEUE_NUM) ", "
    "the actual number of queues per interface is depends on NIC card's own specification");

/* 网卡一个rx_ring使用的buffer数量 */
uint32_t dpa_num_rx_descs = DPA_DEF_RX_DESC_NUM;
module_param_named(num_slots, dpa_num_rx_descs, uint, S_IRUGO);

MODULE_PARM_DESC(num_slots, "Maxmium number of slots per queue, "
    "default " __stringify(DPA_DEF_QUEUE_NUM) ", "
    "maxmium " __stringify(DPA_MAX_QUEUE_NUM) ", "
    "minimum " __stringify(DPA_MIN_QUEUE_NUM) ", "
    "the actual number of slots per queue is depends on NIC card's own specification");

uint32_t dpa_num_bufs;

/* Buffer大小 */
uint32_t dpa_buf_size = DPA_DEF_BUF_SIZE;
module_param_named(buffer_size, dpa_buf_size, uint, S_IRUGO);
MODULE_PARM_DESC(buffer_size, "Size of packet buffer, "
    "default " __stringify(DPA_DEF_BUF_SIZE) ", "
    "maxmium " __stringify(DPA_MAX_BUF_SIZE) ", "
    "minimum " __stringify(DPA_MIN_BUF_SIZE) ", "
    "will be aligned to integral multiple of " __stringify(DPA_BUF_SIZE_MULTIPLE));

/* 注册/反注册一个接口时是否设置/取消混杂模式 */
int dpa_flag_alter_promisc = 1;
module_param_named(alter_promisc, dpa_flag_alter_promisc, int, S_IRUGO);
MODULE_PARM_DESC(alter_promisc, "Alter a interface's promisc setting, "
    "set promisc when DPA registers to it, "
    "clear promisc when DPA unregisters to it, "
    "0=OFF, 1=ON, default 1");

/* 对于802.1q报文(带有Vlan Tag)是否剥除Vlan标签 */
/* Vlan ID会填入buffer的info字段0-11位 */
int dpa_flag_vlan_strip = 1;
module_param_named(vlan_strip, dpa_flag_vlan_strip, int, S_IRUGO);
MODULE_PARM_DESC(vlan_strip, "Enable/disable NIC card(s) to strip 802.1q "
    "VLAN tag from receiving packets, 0=OFF, 1=ON, default 1");
/* 是否允许延迟处理中断事件 */
int dpa_pend_intr = 0;

/* DEBUG打印开关 */
int dpa_flag_debug = 1;
module_param_named(debug, dpa_flag_debug, int , S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Enable/disable debugging output, "
    "0=OFF, 1=ON, default 1");

EXPORT_SYMBOL(dpa_attach);
EXPORT_SYMBOL(dpa_detach);
EXPORT_SYMBOL(dpa_ring_reinit);
EXPORT_SYMBOL(dpa_buffer_atbl);
EXPORT_SYMBOL(dpa_total_buffers);
EXPORT_SYMBOL(dpa_buffer_base);
EXPORT_SYMBOL(dpa_reset);
EXPORT_SYMBOL(dpa_buf_size);
EXPORT_SYMBOL(dpa_num_rx_descs);
EXPORT_SYMBOL(dpa_flag_alter_promisc);
EXPORT_SYMBOL(dpa_flag_vlan_strip);
EXPORT_SYMBOL(dpa_rx_irq);
EXPORT_SYMBOL(dpa_pend_intr);
EXPORT_SYMBOL(dpa_flag_debug);

struct dpa_priv {
    struct dpa_if       *dif;
    struct net_device   *netdev;
    int                 ring_id;
    uint32_t            rfirst;
    uint32_t            rlast;  /* 关注的RING的范围 */
};

/* 获取一个设备的驱动模块指针 */
static struct device_driver *dpa_get_dev_driver(struct device *dev)
{
    struct device_driver *driver;
    
	while ((driver = dev->driver) == NULL) {
		if ((dev = dev->parent) == NULL) {
            return NULL;
		}
	}
	return driver;
}

/* 
   得到接口的net_device引用
   同时会获取接口驱动模块的引用，以禁止使用时进行卸载模块操作 
*/
static int dpa_acquire_netdev(const char *name, struct net_device **netdevpp)
{
    struct device_driver *driver;
    
    *netdevpp = dev_get_by_name(&init_net, name);
    if (*netdevpp == NULL) {
        return -ENODEV;
    }
    if (!DPA_IF_CAPABLE(*netdevpp)) {
        goto error;
    }

    driver = dpa_get_dev_driver(&((*netdevpp)->dev));
    if (driver == NULL) {
        goto error;
    }
    if (!try_module_get(driver->owner)) {
		goto error;
    }
    
    dpa_debug("A reference of interface %s is acquired.", (*netdevpp)->name);
    dpa_debug("A reference of module %s is acquired.", driver->name);
    return 0;
error:
    dev_put(*netdevpp);
    return -EINVAL;
}


/* 
   释放接口的net_device引用
   同时会释放接口驱动模块的引用
*/
static void dpa_release_netdev(struct net_device *netdev)
{
    struct device_driver *driver;
	driver = dpa_get_dev_driver(&netdev->dev);
    dpa_debug("A reference of interface %s is released.", netdev->name);
	dev_put(netdev);
	if (driver != NULL) {
        dpa_debug("A reference of module %s is released.", driver->name);
        module_put(driver->owner);
	}
}


/*
 * file描述符的析构函数(需已对reg_lock加锁)
 * 恢复接口为正常模式
 * 释放对应的资源，不会释放net_device引用，需调用者释放并清空priv字段占用的空间
 */
static void dpa_destructor_locked(void *data)
{
    struct dpa_priv *priv = data;
    struct net_device *netdev = priv->netdev;
    struct dpa_adapter *dadp = DPA_ADP(netdev);
    struct dpa_if *dif = priv->dif;

    dadp->refs--;
    if (dadp->refs <= 0) {
        uint32_t i, j, num_slots;

        dpa_debug("Delete last instance for %s.", netdev->name);
         /* 此时接口的enable_flag仍置位但refs已为0，是一个特殊状态
            休眠10ms，此时DIOCREGIF流程会循环检查该状态(DPA_IF_DELETING) */
        dpa_ml_unlock(&dadp->reg_lock);
        msleep(10);
        dpa_ml_lock(&dadp->reg_lock);

        dadp->reg(netdev, 0);
        /* 唤醒所有等待队列, Poll会失败 */
        for (i = 0; i < dadp->num_rx_rings; i++) {
            wake_up(&dadp->rx_rings[i].wq);
        }
        wake_up(&dadp->rx_wq);

        /* 释放Buffer */
        DPA_ALLOC_LOCK();
        
        for (i = 0; i < dadp->num_rx_rings; i++) {
            struct dpa_ring *ring = dadp->rx_rings[i].ring;
            num_slots = dadp->rx_rings[i].nkr_num_slots;
            for (j = 0; j < num_slots; j++) {
                dpa_free_buf(dif, ring->slot[j].buf_idx);
            }
            dpa_sl_destroy(&dadp->rx_rings[i].q_lock);
        }
        DPA_ALLOC_UNLOCK();
        dpa_free_rings(dadp);
    }
    dpa_if_free(dif);
}

/* file描述符的析构函数 */
static void dpa_destructor(void *data)
{
    struct dpa_priv *priv = data;
    struct net_device *netdev = priv->netdev;
    struct dpa_adapter *dadp = DPA_ADP(netdev);
    dpa_ml_lock(&dadp->reg_lock);
    dpa_destructor_locked(data);
    dpa_ml_unlock(&dadp->reg_lock);
    dpa_release_netdev(netdev);
    memzero(priv, sizeof(*priv));
    kfree(priv);
    dpa_info("Process \"%s\" (PID: %i, TID: %i) is unregistered from interface %s.",
        current->comm, current->tgid, current->pid, netdev->name);
}

/*
 * 同步接收队列的错误处理函数
 * 将cur和avail同步为hwcur与hwavail，该函数只能被上半段调用
 */
int dpa_ring_reinit(struct dpa_kring *kring, int32_t force_sync)
{
    struct dpa_ring *ring = kring->ring;
    uint32_t i, num_slots = kring->nkr_num_slots - 1;
    uint32_t ring_id = kring - kring->dadp->rx_rings;
    uint32_t errors = 0;

    dpa_info("Reinitialize %s queue %d.", 
            kring->dadp->netdev->name, ring_id);

    for (i = 0; i <= num_slots; i++) {
        uint32_t idx = ring->slot[i].buf_idx;
        uint32_t len = ring->slot[i].len;
        if (idx < 2 || idx >= dpa_total_buffers) {
            if (!errors++) {
                dpa_debug("Bad buffer at slot %d idx %d len %d.", i, idx, len);
            }
            ring->slot[i].buf_idx = 0;
            ring->slot[i].len = 0;
        } 
        else if (len > dpa_buf_size) {
            ring->slot[i].len = 0;
            if (!errors++) {
                dpa_debug("Bad len %d at slot %d idx %d.", len, i, idx);
            }
        }
    }
    
    if (errors || force_sync) {
        dpa_info("Resynchronize %s queue %d: cur %d -> %d, avail %d -> %d, hw_ofs %d -> %d.",
            kring->dadp->netdev->name, ring_id,
            ring->cur, kring->nr_hwcur,
            ring->avail, kring->nr_hwavail,
            ring->hw_ofs, kring->nkr_hwofs);
        ring->cur = kring->nr_hwcur;
        ring->avail = kring->nr_hwavail;
        ring->hw_ofs = kring->nkr_hwofs;
    }
    return (errors ? 1 : 0);
}


/* 按照RING ID扫描指定的RING范围 */
static int dpa_set_ring_id(struct dpa_priv *priv, uint32_t ring_id_field)
{
    struct net_device *netdev = priv->netdev;
    struct dpa_adapter *dadp = DPA_ADP(netdev);
    uint32_t ring_id = ring_id_field & DPA_RING_ID_MASK;
    /* rfirst与rlast相等时为初始化状态，不需加锁 */
    int need_lock = (priv->rfirst != priv->rlast);
    int max = dadp->num_rx_rings;
    
    if (ring_id >= max) {
        dpa_error("Invalid ring ID %d.", ring_id);
        return -EINVAL;
    }
    if (need_lock) {
        dpa_sl_lock(&dadp->core_lock);
    }
    priv->ring_id = ring_id_field;

    if (ring_id_field & DPA_RING_FLAG_ALL) {
        priv->rfirst = 0;
        priv->rlast = DPA_RING_FLAG_ALL;
        dpa_debug("Ring ID set to all %d rings for %s.", max, netdev->name);

    } else {
        priv->rfirst = ring_id;
        priv->rlast = ring_id + 1;
        dpa_debug("Ring ID set to ring %d for %s.", priv->rfirst, netdev->name);
    }
    if (need_lock) {
        dpa_sl_unlock(&dadp->core_lock);
    }

    return 0;
}



int dpa_attach(struct dpa_adapter *dadp)
{
    int size;
    void *mem;
    struct net_device *netdev = dadp->netdev;

    if (netdev == NULL) {
        dpa_error("Can not get netdev, attach failed.");
        return -ENODEV;
    }
    if (DPA_ADP_HOOK(netdev) != NULL) {
        dpa_error("Device %s is already attached.", netdev->name);
        return -EINVAL;   
    }

    dadp->refs = 0;
    
    size = sizeof(*dadp) + dadp->num_rx_rings * sizeof(struct dpa_kring);

    mem = kmalloc(size, GFP_ATOMIC);
    if (mem) {
        memzero(mem, size);

        DPA_ADP_HOOK(netdev) = mem;
        dadp->rx_rings = (void *)((char *)mem + sizeof(*dadp));
        memcpy(mem, dadp, sizeof(*dadp));
        DPA_IF_SET_CAPABLE(netdev);

        dadp = mem;
        /* 核心锁初始化，其他的锁在dpa_if_new后初始化 */
        dpa_sl_init(&dadp->core_lock);
        dpa_ml_init(&dadp->reg_lock);
    }
    
    dpa_info("%s attach %s.", netdev->name, mem ? "succeed" : "failed");

    return mem ? 0 : -ENOMEM;
}


void dpa_detach(struct net_device *netdev)
{
    struct dpa_adapter *dadp = DPA_ADP(netdev);

    if (!dadp) {
        return;
    }

    dpa_sl_destroy(&dadp->core_lock);
    dpa_ml_destroy(&dadp->reg_lock);
    memzero(dadp, sizeof(*dadp));
    DPA_ADP_HOOK(netdev) = NULL;
    kfree(dadp);
    dpa_info("%s detached.", netdev->name);
}

/* 由驱动重新初始化RING时调用 */
struct dpa_slot * dpa_reset(struct dpa_adapter *dadp, int ring_id, uint32_t new_cur)
{
    struct dpa_kring *kring;
    int new_hwofs, num_slots;

    if (dadp == NULL) {
        return NULL;
    }
    if (!DPA_IF_ENABLE(dadp->netdev)) {
        return NULL;
    }

    kring = dadp->rx_rings + ring_id;
    new_hwofs = kring->nr_hwcur + kring->nr_hwavail - new_cur;
    
    num_slots = kring->nkr_num_slots - 1;
    if (new_hwofs > num_slots) {
        new_hwofs -= num_slots + 1;
    }

    /* 重置网卡ring偏移量 */
    kring->nkr_hwofs = new_hwofs;
    dpa_debug("New hwofs %d on %s [%d].", kring->nkr_hwofs, dadp->netdev->name, ring_id);
    /* 唤醒主等待队列和接收等待队列，注意此时ring还没有重新配置 */
    wake_up(&kring->wq);
    wake_up(&dadp->rx_wq);
    return kring->ring->slot;
}



/* RX收包队列处理函数(可能在中断中) */
int dpa_rx_irq(struct net_device *netdev, int ring_id, int *work_done)
{
    struct dpa_adapter *dadp;
    struct dpa_kring *r;
    wait_queue_head_t *main_wq;

    if (!DPA_IF_ENABLE(netdev)) {
        return 0;
    }
    dadp = DPA_ADP(netdev);

    /* 偏移至对应的RING */
    r = dadp->rx_rings + ring_id;
    r->nr_kflags |= DPA_KR_PEND_INTR;
    main_wq = (dadp->num_rx_rings > 0) ? &dadp->rx_wq : NULL;
    
    if (dadp->excl_locks) {
        dpa_sl_lock(&r->q_lock);
        wake_up(&r->wq);
        dpa_sl_unlock(&r->q_lock);
        if (main_wq) {
            dpa_sl_lock(&dadp->core_lock);
            wake_up(main_wq);
            dpa_sl_unlock(&dadp->core_lock);
        }
    } else {
        dpa_sl_lock(&dadp->core_lock);
        wake_up(&r->wq);
        if (main_wq) {
            wake_up(main_wq);
        }
        dpa_sl_unlock(&dadp->core_lock);
    }
    *work_done = 1;
    return 1;
}

/* 注册至网络接口,由dpa_ioctl调用 */
static int dpa_reg_if(struct dpa_priv **ppriv, const char *if_name, uint32_t queue_id)
{
    struct dpa_priv *priv = *ppriv;
    struct net_device *netdev;
    struct dpa_adapter *dadp;
    uint32_t i;
    int error = 0;
    
    /* 该文件描述符已注册过 */
    if (priv != NULL) {
        return dpa_set_ring_id(priv, queue_id);
    }

    /* 获取net_device引用 */
    error = dpa_acquire_netdev(if_name, &netdev);
    if (error) {
        return error;
    }
    dadp = DPA_ADP(netdev);
    
    /* 申请private字段 */
    priv = kmalloc(sizeof(struct dpa_priv), GFP_ATOMIC);
    if (priv == NULL) {
        error = -ENOMEM;
        dpa_release_netdev(netdev);
        return error;
    }
    memzero(priv, sizeof(struct dpa_priv));
    
    for (i = 10; i > 0; i--) {
        dpa_ml_lock(&dadp->reg_lock);
        if (!DPA_IF_DELETING(dadp)) {
            break;
        }
        dpa_ml_unlock(&dadp->reg_lock);
        msleep(10);
    }
    if (i == 0) {
        dpa_error("Too many register request to interface %s, cancel.", if_name);
        error = -EBUSY;
        kfree(priv);
        dpa_release_netdev(netdev);
        return error;
    }

    /* 保存设备引用 */
    priv->netdev = netdev;
    error = dpa_set_ring_id(priv, queue_id);
    if (error) {
        goto release;
    }
    
    if (dadp->refs == dadp->num_rx_rings) {
        dpa_info("Device %s is fully in use, accept no more request.", if_name);
        error = -EBUSY;
        goto release;
    }
    
    priv->dif = dpa_new_if(if_name, dadp);
    if (priv->dif == NULL) {
        error = -ENOMEM;
    } 
    else if (DPA_IF_ENABLE(netdev)) {
        /*  已经注册 */
        dpa_debug("Interface %s is already registed.", if_name);
    } 
    else 
    {
        for (i = 0 ; i < dadp->num_rx_rings; i++) {
            dpa_sl_init(&dadp->rx_rings[i].q_lock);
        }
        error = dadp->reg(netdev, 1);
        if (error) {
            dpa_destructor_locked(priv);
        }
    }

    if (error) {
release:
        dpa_ml_unlock(&dadp->reg_lock);
        dpa_release_netdev(netdev);
        memzero(priv, sizeof(*priv));
        kfree(priv);
        return error;
    }
    
    dpa_ml_unlock(&dadp->reg_lock);
    
    *ppriv = priv;
    
    return 0;
}

/* POLL实现 */
static unsigned int dpa_poll(struct file * filp, struct poll_table_struct *pwait)
{
    /* 只支持Poll in,可以不获取events */
    /* 不过为了实现规范还是取下 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,4,0)
    int events = pwait ? pwait->_key : POLLIN;

#elif LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)/* 2.6.32 */
    int events = pwait ? pwait->key : POLLIN;
#else
    int events = POLLIN;
#endif
    
    struct dpa_priv *priv;
    struct dpa_adapter *dadp;
    struct net_device *netdev;
    struct dpa_kring *kring;
    uint32_t core_lock, i, check_all, do_rx, revents = 0;
    uint32_t max_rx;
    enum { NO_CL, NEED_CL, LOCKED_CL };

    priv = filp->private_data;
    if (priv == NULL) {
        return POLLERR;
    }

    netdev = priv->netdev;

    if (!DPA_IF_ENABLE(netdev)) {
        return POLLERR;
    }
    
    //dbg("Device %s receive poll events 0x%x.", netdev->name, events);
    do_rx = events & (POLLIN | POLLRDNORM);

    dadp = DPA_ADP(netdev);

    max_rx = dadp->num_rx_rings;

    /*
     * 如果网卡支持多队列且用户进程全部poll，则使用全局的wq，否则使用独立
     * 的wq。
     * 驱动代码的中断处理流程会唤醒独立的wq，如果存在多个挂载的用户ring，则
     * 全局wq也会唤醒。
     * 如果网卡支持独立加锁，则优先使用独立的锁。但是如果设置了check_all，则
     * 直接使用全局锁。
     * 只在必要时加锁，比如如果poll时仍有可用buffer，则不加锁返回结果
     * rxsync同步函数只在可用buffer耗尽时调用。
     */
    check_all = (priv->rlast == DPA_RING_FLAG_ALL) && (max_rx > 1);
    core_lock = (check_all || !dadp->excl_locks) ? NEED_CL : NO_CL;
    if (priv->rlast != DPA_RING_FLAG_ALL) {
        max_rx = priv->rlast;
    }

    for (i = priv->rfirst; do_rx && i < max_rx; i++) {
        kring = &dadp->rx_rings[i];
        if (kring->ring->avail > 0) {
            revents |= do_rx;
            do_rx = 0;
        }
    }
    
    if (do_rx) {
        for (i = priv->rfirst; i < max_rx; i++) {
            kring = &dadp->rx_rings[i];
            
            if (core_lock == NEED_CL) {
                dpa_sl_lock(&dadp->core_lock);
                core_lock = LOCKED_CL;
            }
            
            if (dadp->excl_locks) {
                dpa_sl_lock(&dadp->rx_rings[i].q_lock);
            }

            if (dadp->rx_sync(netdev, i, 0)) {
                revents |= POLLERR;
            }

            //do_gettimeofday(&kring->ring->ts);

            if (kring->ring->avail > 0) {
                revents |= do_rx;
            } 
            else if (!check_all) {
                poll_wait(filp, &kring->wq, pwait);
            }
            
            if (dadp->excl_locks) {
                dpa_sl_unlock(&dadp->rx_rings[i].q_lock);
            }
        }
    }
    if (check_all && revents == 0) {
        if (do_rx) {
            poll_wait(filp, &dadp->rx_wq, pwait);
        }
    }
    if (core_lock == LOCKED_CL) {
        dpa_sl_unlock(&dadp->core_lock);
    }

    return revents;
}

/* MMAP实现 */
static int dpa_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int atbl_skip, i, j;
    int user_skip = 0;
    struct addr_tbl_item *atbl_item;
    const struct dpa_obj_pool *p[] = {
        allocator->if_pool,
        allocator->ring_pool,
        allocator->buf_pool };

    UNUSED(filp);

    for (i = 0; i < 3; i++) {
        for (atbl_skip = 0, j = 0; j < p[i]->_num_clusters; j++) {
            atbl_item = &p[i]->addr_tbl[atbl_skip];
            if (remap_pfn_range(vma, vma->vm_start + user_skip,
                    atbl_item->paddr >> PAGE_SHIFT, p[i]->_clst_size,
                    vma->vm_page_prot)) {
                return -EAGAIN;
            }
            atbl_skip += p[i]->clst_n_entries;
            user_skip += p[i]->_clst_size;
        }
    }

    return 0;
}

/* IOCTL入口 */
#define IOCTL_NAME  .unlocked_ioctl
long dpa_ioctl(struct file *filp, uint32_t cmd, unsigned long arg)
{
    struct ioc_para para;
    struct dpa_priv *priv;
    struct net_device *netdev;
    struct dpa_adapter *dadp;
    int error = 0;
    uint32_t i, last;
    
    if (_IOC_TYPE(cmd) != DPA_IOC_MAGIC) {
        return -ENOTTY;
    }
    
    if (_IOC_DIR(cmd) & _IOC_READ) {
        error = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    }
    else if (_IOC_DIR(cmd) & _IOC_WRITE) {
        error = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    }
    
    if (error) {
        return -EFAULT;
    }
    
    memzero(&para, sizeof(para));

    if (arg && copy_from_user(&para, (void *)arg, sizeof(para)) != 0) {
        return -EFAULT;
    }

    priv = filp->private_data;

    para.if_name[sizeof(para.if_name) - 1] = '\0';
    
    switch (cmd) {
        case DIOCGETINFO:
            para.mem_size = allocator->total_size;
            para.num_rx_queues = 0;
            para.num_rx_slots = 0;
            para.num_bufs = dpa_num_bufs;
            para.buf_size = dpa_buf_size;
            
            if (para.if_name[0] == '\0') {
                break;
            }
            error = dpa_acquire_netdev(para.if_name, &netdev);
            if (error) {
                break;
            }
            
            dadp = DPA_ADP(netdev);
            para.num_rx_queues = dadp->num_rx_rings;
            para.num_rx_slots = dadp->num_rx_descs;
            para.dev_model = dadp->dev_model;
            
            dpa_release_netdev(netdev);
            break;


        case DIOCREGIF:
            error = dpa_reg_if(&priv, para.if_name, para.queue_id);
            if ((!error) && priv) {
                filp->private_data = priv;
                dadp = DPA_ADP(priv->netdev);
                /* 将相关信息填入ioc_para结构 */
                para.num_rx_queues = dadp->num_rx_rings;
                para.num_rx_slots = dadp->num_rx_descs;
                para.mem_size = allocator->total_size;
                para.dev_model = dadp->dev_model;
                para.offset = dpa_if_offset(priv->dif);
                
                dpa_info("Process \"%s\" (PID: %i, TID: %i) is registered to interface %s.",
                    current->comm, current->tgid, current->pid, para.if_name);
                
            } else {
                dpa_error("Process \"%s\" (PID: %i, TID: %i) failed to register to interface %s due to error: %d.",
                    current->comm, current->tgid, current->pid, para.if_name, -error);
            }
            break;


        case DIOCUNREGIF:
            if (priv == NULL) {
                error = -ENXIO;
                dpa_error("Process \"%s\" (PID: %i, TID: %i) failed to unregister to interface %s due to error: %d.",
                    current->comm, current->tgid, current->pid, para.if_name, -error);
                break;
            }
            dpa_destructor(priv); 
            filp->private_data = NULL;
            break;


        case DIOCRXSYNC:
            if (priv == NULL)  {
                error = -ENXIO;
                break;
            }
            netdev = priv->netdev;
            dadp = DPA_ADP(netdev);
            last = priv->rlast;
            if (last == DPA_RING_FLAG_ALL) {
                last = dadp->num_rx_rings;
            }

            for (i = priv->rfirst; i < last; i++) {
                dadp->rx_sync(netdev, i, 1);
                do_gettimeofday(&dadp->rx_rings[i].ring->ts);
            }

            break;


        case DIOCGETRXSTATS:
            para.mem_size = allocator->total_size;
            para.num_rx_queues = 0;
            para.num_rx_slots = 0;
            para.rx_stats.packets = 0;
            para.rx_stats.bytes = 0;
            para.rx_stats.drops = 0;
            para.rx_stats.ts = (struct timeval){0};
            
            if (para.if_name[0] == '\0') {
                break;
            }
            error = dpa_acquire_netdev(para.if_name, &netdev);
            if (error) {
                break;
            }

            dadp = DPA_ADP(netdev);
            para.num_rx_queues = dadp->num_rx_rings;
            para.num_rx_slots = dadp->num_rx_descs;
            para.dev_model = dadp->dev_model;
            
            para.rx_stats.enabled = DPA_IF_ENABLE(netdev) ? 1 : 0;
            
            error = dadp->get_rx_stats(netdev, &(para.rx_stats));
            if (error) {
                error = -EINVAL;
            }
            dpa_release_netdev(netdev);
            break;
        default:
            error = -EOPNOTSUPP;
    }
    
    if (arg && copy_to_user((void*)arg, &para, sizeof(para)) != 0) {
        return -EFAULT;
    }
    return error;
}


static int dpa_release(struct inode *inodep, struct file *file)
{
    UNUSED(inodep);
    if (file->private_data) {
        dpa_destructor(file->private_data);
    }
    return 0;
}

static struct file_operations dpa_fops = {
    .owner = THIS_MODULE,
    .mmap = dpa_mmap,
    IOCTL_NAME = dpa_ioctl,
    .poll = dpa_poll,
    .release = dpa_release,
};

static struct miscdevice dpa_miscdev = {
    .minor  = MISC_DYNAMIC_MINOR,
    .name   = "dpa",
    .fops   = &dpa_fops,
};

static void __init dpa_param_init(void)
{
    dpa_num_ifs = min_t(u32, dpa_num_ifs, DPA_MAX_IF_NUM);
    dpa_num_ifs = max_t(u32, dpa_num_ifs, DPA_MIN_IF_NUM);
    
    dpa_num_queues = min_t(u32, dpa_num_queues, DPA_MAX_QUEUE_NUM);
    dpa_num_queues = max_t(u32, dpa_num_queues, DPA_MIN_QUEUE_NUM);

    dpa_num_rx_descs = min_t(u32, dpa_num_rx_descs, DPA_MAX_RX_DESC_NUM);
    dpa_num_rx_descs = max_t(u32, dpa_num_rx_descs, DPA_MIN_RX_DESC_NUM);
  
    dpa_num_bufs = dpa_num_ifs * dpa_num_queues * dpa_num_rx_descs;

    dpa_buf_size = min_t(u32, dpa_buf_size, DPA_MAX_BUF_SIZE);
    dpa_buf_size = max_t(u32, dpa_buf_size, DPA_MIN_BUF_SIZE);
    dpa_buf_size = ALIGN(dpa_buf_size, DPA_BUF_SIZE_MULTIPLE);
    
    dpa_debug("Param initialized, number of interfaces is %u, number of queues (per interface) is %u, "
        "number of slots (per queue) is %u, buffer size is %u.",
        dpa_num_ifs, dpa_num_queues, dpa_num_rx_descs, dpa_buf_size);
}

/*
 * 初始化模块
 * 分配变量内存并创建/dev/dpa 设备节点
 */
static int __init dpa_init(void)
{
    int ret;
    dpa_param_init();
    ret = dpa_mem_init();
    if (ret) {
        dpa_error("Failed to initialize the memory allocator.");
        return ret;
    }
    
    ret = misc_register(&dpa_miscdev); 
    if (ret) {
        dpa_error("Failed to create dpa device.");
        dpa_mem_free();
        return ret;
    }
    dpa_print("DPA initialized, version: " DPA_VERSION ".");
    dpa_info("Allocator set up with %d MiB.", (int)(allocator->total_size >> 20));

    return 0;
}

static void __exit dpa_exit(void)
{
    misc_deregister(&dpa_miscdev);
    dpa_mem_free();
    dpa_print("DPA exit.");
}


module_init(dpa_init);
module_exit(dpa_exit);

MODULE_AUTHOR("Nicholas Lee, <nicrevelee@gmail.com>");
MODULE_DESCRIPTION("Direct Packet Access");
MODULE_LICENSE("GPL");
MODULE_VERSION(DPA_VERSION);
