/* dpa_main.c
 * DPA��ģ��
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#include "dpa_k.h"
#include "dpa_mem.h"

struct dpa_mem_allocator *allocator;

uint32_t dpa_total_buffers;
char *dpa_buffer_base;

/* ���֧�ֵĽӿ����� */
uint32_t dpa_num_ifs = DPA_DEF_IF_NUM;
module_param_named(num_interfaces, dpa_num_ifs, int, S_IRUGO);
MODULE_PARM_DESC(num_interfaces, "Maxmium number of interfaces that DPA supports, "
    "default " __stringify(DPA_DEF_IF_NUM) ", "
    "maxmium " __stringify(DPA_MAX_IF_NUM) ", "
    "minimum " __stringify(DPA_MIN_IF_NUM));

/* һ���ӿ����֧�ֵĶ������� */
uint32_t dpa_num_queues = DPA_DEF_QUEUE_NUM;
module_param_named(num_queues, dpa_num_queues, int, S_IRUGO);
MODULE_PARM_DESC(num_queues, "Maxmium number of queues per interface, "
    "default " __stringify(DPA_DEF_QUEUE_NUM) ", "
    "maxmium " __stringify(DPA_MAX_QUEUE_NUM) ", "
    "minimum " __stringify(DPA_MIN_QUEUE_NUM) ", "
    "the actual number of queues per interface is depends on NIC card's own specification");

/* ����һ��rx_ringʹ�õ�buffer���� */
uint32_t dpa_num_rx_descs = DPA_DEF_RX_DESC_NUM;
module_param_named(num_slots, dpa_num_rx_descs, uint, S_IRUGO);

MODULE_PARM_DESC(num_slots, "Maxmium number of slots per queue, "
    "default " __stringify(DPA_DEF_QUEUE_NUM) ", "
    "maxmium " __stringify(DPA_MAX_QUEUE_NUM) ", "
    "minimum " __stringify(DPA_MIN_QUEUE_NUM) ", "
    "the actual number of slots per queue is depends on NIC card's own specification");

uint32_t dpa_num_bufs;

/* Buffer��С */
uint32_t dpa_buf_size = DPA_DEF_BUF_SIZE;
module_param_named(buffer_size, dpa_buf_size, uint, S_IRUGO);
MODULE_PARM_DESC(buffer_size, "Size of packet buffer, "
    "default " __stringify(DPA_DEF_BUF_SIZE) ", "
    "maxmium " __stringify(DPA_MAX_BUF_SIZE) ", "
    "minimum " __stringify(DPA_MIN_BUF_SIZE) ", "
    "will be aligned to integral multiple of " __stringify(DPA_BUF_SIZE_MULTIPLE));

/* ע��/��ע��һ���ӿ�ʱ�Ƿ�����/ȡ������ģʽ */
int dpa_flag_alter_promisc = 1;
module_param_named(alter_promisc, dpa_flag_alter_promisc, int, S_IRUGO);
MODULE_PARM_DESC(alter_promisc, "Alter a interface's promisc setting, "
    "set promisc when DPA registers to it, "
    "clear promisc when DPA unregisters to it, "
    "0=OFF, 1=ON, default 1");

/* ����802.1q����(����Vlan Tag)�Ƿ����Vlan��ǩ */
/* Vlan ID������buffer��info�ֶ�0-11λ */
int dpa_flag_vlan_strip = 1;
module_param_named(vlan_strip, dpa_flag_vlan_strip, int, S_IRUGO);
MODULE_PARM_DESC(vlan_strip, "Enable/disable NIC card(s) to strip 802.1q "
    "VLAN tag from receiving packets, 0=OFF, 1=ON, default 1");
/* �Ƿ������ӳٴ����ж��¼� */
int dpa_pend_intr = 0;

/* DEBUG��ӡ���� */
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
    uint32_t            rlast;  /* ��ע��RING�ķ�Χ */
};

/* ��ȡһ���豸������ģ��ָ�� */
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
   �õ��ӿڵ�net_device����
   ͬʱ���ȡ�ӿ�����ģ������ã��Խ�ֹʹ��ʱ����ж��ģ����� 
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
   �ͷŽӿڵ�net_device����
   ͬʱ���ͷŽӿ�����ģ�������
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
 * file����������������(���Ѷ�reg_lock����)
 * �ָ��ӿ�Ϊ����ģʽ
 * �ͷŶ�Ӧ����Դ�������ͷ�net_device���ã���������ͷŲ����priv�ֶ�ռ�õĿռ�
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
         /* ��ʱ�ӿڵ�enable_flag����λ��refs��Ϊ0����һ������״̬
            ����10ms����ʱDIOCREGIF���̻�ѭ������״̬(DPA_IF_DELETING) */
        dpa_ml_unlock(&dadp->reg_lock);
        msleep(10);
        dpa_ml_lock(&dadp->reg_lock);

        dadp->reg(netdev, 0);
        /* �������еȴ�����, Poll��ʧ�� */
        for (i = 0; i < dadp->num_rx_rings; i++) {
            wake_up(&dadp->rx_rings[i].wq);
        }
        wake_up(&dadp->rx_wq);

        /* �ͷ�Buffer */
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

/* file���������������� */
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
 * ͬ�����ն��еĴ�������
 * ��cur��availͬ��Ϊhwcur��hwavail���ú���ֻ�ܱ��ϰ�ε���
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


/* ����RING IDɨ��ָ����RING��Χ */
static int dpa_set_ring_id(struct dpa_priv *priv, uint32_t ring_id_field)
{
    struct net_device *netdev = priv->netdev;
    struct dpa_adapter *dadp = DPA_ADP(netdev);
    uint32_t ring_id = ring_id_field & DPA_RING_ID_MASK;
    /* rfirst��rlast���ʱΪ��ʼ��״̬��������� */
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
        /* ��������ʼ��������������dpa_if_new���ʼ�� */
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

/* ���������³�ʼ��RINGʱ���� */
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

    /* ��������ringƫ���� */
    kring->nkr_hwofs = new_hwofs;
    dpa_debug("New hwofs %d on %s [%d].", kring->nkr_hwofs, dadp->netdev->name, ring_id);
    /* �������ȴ����кͽ��յȴ����У�ע���ʱring��û���������� */
    wake_up(&kring->wq);
    wake_up(&dadp->rx_wq);
    return kring->ring->slot;
}



/* RX�հ����д�����(�������ж���) */
int dpa_rx_irq(struct net_device *netdev, int ring_id, int *work_done)
{
    struct dpa_adapter *dadp;
    struct dpa_kring *r;
    wait_queue_head_t *main_wq;

    if (!DPA_IF_ENABLE(netdev)) {
        return 0;
    }
    dadp = DPA_ADP(netdev);

    /* ƫ������Ӧ��RING */
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

/* ע��������ӿ�,��dpa_ioctl���� */
static int dpa_reg_if(struct dpa_priv **ppriv, const char *if_name, uint32_t queue_id)
{
    struct dpa_priv *priv = *ppriv;
    struct net_device *netdev;
    struct dpa_adapter *dadp;
    uint32_t i;
    int error = 0;
    
    /* ���ļ���������ע��� */
    if (priv != NULL) {
        return dpa_set_ring_id(priv, queue_id);
    }

    /* ��ȡnet_device���� */
    error = dpa_acquire_netdev(if_name, &netdev);
    if (error) {
        return error;
    }
    dadp = DPA_ADP(netdev);
    
    /* ����private�ֶ� */
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

    /* �����豸���� */
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
        /*  �Ѿ�ע�� */
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

/* POLLʵ�� */
static unsigned int dpa_poll(struct file * filp, struct poll_table_struct *pwait)
{
    /* ֻ֧��Poll in,���Բ���ȡevents */
    /* ����Ϊ��ʵ�ֹ淶����ȡ�� */
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
     * �������֧�ֶ�������û�����ȫ��poll����ʹ��ȫ�ֵ�wq������ʹ�ö���
     * ��wq��
     * ����������жϴ������̻ỽ�Ѷ�����wq��������ڶ�����ص��û�ring����
     * ȫ��wqҲ�ỽ�ѡ�
     * �������֧�ֶ���������������ʹ�ö����������������������check_all����
     * ֱ��ʹ��ȫ������
     * ֻ�ڱ�Ҫʱ�������������pollʱ���п���buffer���򲻼������ؽ��
     * rxsyncͬ������ֻ�ڿ���buffer�ľ�ʱ���á�
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

/* MMAPʵ�� */
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

/* IOCTL��� */
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
                /* �������Ϣ����ioc_para�ṹ */
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
 * ��ʼ��ģ��
 * ��������ڴ沢����/dev/dpa �豸�ڵ�
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
