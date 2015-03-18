/* igb_dpa.h
 * igb驱动DPA适配层实现
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _IGB_DPA_H_
#define _IGB_DPA_H_

#include "../../kernel/dpa_k.h"
#define nic_adapter igb_adapter

extern int dpa_flag_debug;
extern int dpa_flag_alter_promisc;
extern int dpa_flag_vlan_strip;
extern uint32_t dpa_num_rx_descs;

/* 适配不同版本驱动 */
/* New */
#ifndef E1000_RX_DESC_ADV
#define E1000_RX_DESC_ADV(_r, _i)   IGB_RX_DESC(&(_r), _i)
/* Old */
#else
#define igb_rx_buffer           igb_buffer
#define rx_buffer_info          buffer_info
#endif

#ifdef DPA_PACKET_TYPE_UPDATE
#define IGB_RXDADV_PKTTYPE_IPV4       0x00000010 /* IPv4 hdr present */
#define IGB_RXDADV_PKTTYPE_IPV4_EX    0x00000020 /* IPv4 hdr + extensions */
#define IGB_RXDADV_PKTTYPE_IPV6       0x00000040 /* IPv6 hdr present */
#define IGB_RXDADV_PKTTYPE_IPV6_EX    0x00000080 /* IPv6 hdr + extensions */
#define IGB_RXDADV_PKTTYPE_TCP        0x00000100 /* TCP hdr present */
#define IGB_RXDADV_PKTTYPE_UDP        0x00000200 /* UDP hdr present */
#endif
static int igb_dpa_reg(struct net_device *netdev, int reg)
{
    struct nic_adapter *adapter = netdev_priv(netdev);
    struct dpa_adapter *dadp = DPA_ADP(netdev);
    int error = 0;
    int up = netdev->flags & IFF_UP;
    
    if (dadp == NULL) {
        return -EINVAL;
    }
    
    /* 接口down时不允许注册 */
    if (!up && reg) {
        dpa_error("Unable to register %s, interface is down.", netdev->name);
        return -EINVAL;
    }

    while (test_and_set_bit(__IGB_RESETTING, &adapter->state)) {
        msleep(1);
    }
    
    if (up) {
        igb_down(adapter);
    }

    if (reg) {
        DPA_IF_SET_ENABLE(netdev);
    } else {
        DPA_IF_SET_DISABLE(netdev);
    }
    
    if (up) {
        igb_up(adapter);
    }
    
    /* 设置/清除混杂模式 */
    if (dpa_flag_alter_promisc) {
        rtnl_lock();
        error = dev_set_promiscuity(netdev, reg ? 1 : -1);
        rtnl_unlock();

        if(error < 0) {
            dpa_error("%s promiscuity for %s failed.",
                reg ? "Set" : "Unset", netdev->name);
        }
    }
    clear_bit(__IGB_RESETTING, &adapter->state);
    return error;
}

/* 同步接收队列 */
static int igb_dpa_rx_sync(struct net_device *netdev, uint32_t ring_nr, int do_lock)
{
    struct nic_adapter *adapter = netdev_priv(netdev);
    struct dpa_adapter *dadp = DPA_ADP(netdev);
    struct igb_ring *nring = adapter->rx_ring[ring_nr];
    struct dpa_kring *kring = &dadp->rx_rings[ring_nr];
    struct dpa_ring *ring = kring->ring;
    uint32_t k, n, i, bytes = 0, num_slots = kring->nkr_num_slots - 1;
    int force_update = do_lock || kring->nr_kflags & DPA_KR_PEND_INTR;
    uint32_t u = ring->cur, resvd = ring->reserved;
#ifdef DPA_PACKET_TYPE_UPDATE
    int16_t pkt_info;
#endif

    if (!netif_carrier_ok(netdev)) {
        return 0;
    }
    
    if (u > num_slots) {
        return dpa_ring_reinit(kring, 1);
    }

    if (do_lock) {
        dpa_sl_lock(&kring->q_lock);
    }
    rmb();
    /* 将新接收的包更新至ring
     * k是kring的索引，n是网卡ring索引 */
    n = nring->next_to_clean;
    k = dpa_idx_n2k(kring, n);
    if (!dpa_pend_intr || force_update) {
        for (i = 0; ; i++) {
            union e1000_adv_rx_desc *curr =
                    E1000_RX_DESC_ADV(*nring, n);
            uint32_t staterr = le32toh(curr->wb.upper.status_error);
            if ((staterr & E1000_RXD_STAT_DD) == 0) {
                break;
            }
            ring->slot[k].len = le16toh(curr->wb.upper.length);

            /* Vlan ID */
            ring->slot[k].info = (staterr & E1000_RXD_STAT_VP) ?
                    le16toh(curr->wb.upper.vlan) & DPA_SLOT_INFO_VID : 0;
            
#ifdef DPA_PACKET_TYPE_UPDATE
            pkt_info = le16toh(curr->wb.lower.lo_dword.pkt_info);
            if (likely((pkt_info & IGB_RXDADV_PKTTYPE_IPV4)
                || (pkt_info & IGB_RXDADV_PKTTYPE_IPV4_EX))) {
                ring->slot[k].info |= DPA_SLOT_INFO_IPV4;
            }
            else if ((pkt_info & IGB_RXDADV_PKTTYPE_IPV6)
                || (pkt_info & IGB_RXDADV_PKTTYPE_IPV6_EX)) {
                ring->slot[k].info |= DPA_SLOT_INFO_IPV6;
            }
            
            if (pkt_info & IGB_RXDADV_PKTTYPE_TCP) {
                ring->slot[k].info |= DPA_SLOT_INFO_TCP;
            }
            else if (pkt_info & IGB_RXDADV_PKTTYPE_UDP) {
                ring->slot[k].info |= DPA_SLOT_INFO_UDP;
            }
#endif

            bytes += ring->slot[k].len;
            k = (k == num_slots) ? 0 : k + 1;
            n = (n == num_slots) ? 0 : n + 1;
        }
        if (i) { 
            /* 更新网卡的统计计数 */
            kring->total_packets += i;
            kring->total_bytes += bytes;

#ifndef DPA_NIC_STATS_DISABLE
            nring->total_packets += i;
            nring->rx_stats.packets += i;
            nring->total_bytes += bytes;
            nring->rx_stats.bytes += bytes;
#endif
            /* 更新状态 */
            nring->next_to_clean = n;
            kring->nr_hwavail += i;
        }
        kring->nr_kflags &= ~DPA_KR_PEND_INTR;
    }

    /* 跳过用户释放的buffer */
    
    k = kring->nr_hwcur;
    if (resvd > 0) {
        if (resvd + ring->avail >= num_slots + 1) {
            dpa_debug("Invalid reserve/avail %d %d.", resvd, ring->avail);
            ring->reserved = resvd = 0;
        }
        u = (u >= resvd) ? u - resvd : u + num_slots + 1 - resvd;
    }
    if (k != u) { 
        /* 将用户空间释放的buffer更新至网卡ring */
        n = dpa_idx_k2n(kring, k);
        for (i = 0; k != u; i++) {
            struct dpa_slot *slot = &ring->slot[k];
            union e1000_adv_rx_desc *curr = E1000_RX_DESC_ADV(*nring, n);
            uint64_t paddr;
            void *addr = BUF_VPA(slot, &paddr);

            if (addr == dpa_buffer_base) {
                if (do_lock) {
                    dpa_sl_unlock(&kring->q_lock);
                }
                return dpa_ring_reinit(kring, 0);
            }

            curr->wb.upper.status_error = 0;
            curr->read.pkt_addr = htole64(paddr);
            k = (k == num_slots) ? 0 : k + 1;
            n = (n == num_slots) ? 0 : n + 1;
        }
        kring->nr_hwavail -= i;
        kring->nr_hwcur = u;
        nring->next_to_use = n;
        wmb();
        /* 保留一个buffer，将n回退1个buffer */
        n = (n == 0) ? num_slots : n - 1;
        writel(n, nring->tail);
    }
    /* 通知用户进程包已就绪 */
    ring->avail = kring->nr_hwavail - resvd;

    if (do_lock) {
        dpa_sl_unlock(&kring->q_lock);
    }
    return 0;
}

static int igb_dpa_conf_rx_ring(struct igb_ring *rxr)
{
    struct net_device *netdev = rxr->netdev;
    struct dpa_adapter *dadp = DPA_ADP(netdev);
    int reg_idx = rxr->reg_idx;
    struct dpa_slot *slot = dpa_reset(dadp, reg_idx, 0);
    uint32_t i;

    if (!slot) {
        return 0;
    }

    for (i = 0; i < rxr->count; i++) {
        union e1000_adv_rx_desc *rx_desc;
        uint64_t paddr;
        int si = dpa_idx_n2k(&dadp->rx_rings[reg_idx], i);

        BUF_VPA(slot + si, &paddr);
        rx_desc = E1000_RX_DESC_ADV(*rxr, i);
        rx_desc->read.hdr_addr = 0;
        rx_desc->read.pkt_addr = htole64(paddr);
    }
    rxr->next_to_use = 0;
    i = rxr->count - 1 - dadp->rx_rings[reg_idx].nr_hwavail;

    wmb();
    dpa_debug("%s rxr%d.tail %d.", netdev->name, reg_idx, i);
    writel(i, rxr->tail);
    return 1;
}

static int igb_dpa_get_rx_stats(struct net_device *netdev, struct dpa_rx_stats *rx_stats)
{
    struct nic_adapter *adapter = netdev_priv(netdev);
    struct dpa_adapter *dadp = DPA_ADP(netdev);
    uint32_t queue_id, qbegin, qend, i;
    
    if (rx_stats->queue_id & DPA_QUEUE_FLAG_ALL) {
        qbegin = 0;
        qend = dadp->num_rx_rings;
    } else {
        if ((rx_stats->op_code & DPA_RX_STATS_OP_DROPS)
            || (rx_stats->op_code & DPA_RX_STATS_OP_ERRORS)) {
            /* 不支持队列粒度获取丢包与错包计数 */
            return 1;
        }
        queue_id = rx_stats->queue_id & DPA_RING_ID_MASK;
        if (queue_id >= dadp->num_rx_rings) {
            return 1;
        }
        qbegin = queue_id;
        qend = queue_id + 1;
    }
        
    if (!netif_carrier_ok(netdev)) {
        return 1;
    }
    
    igb_update_stats(adapter);
    
    for (i = qbegin; i < qend; i++) {
        if (rx_stats->op_code & DPA_RX_STATS_OP_PACKETS) {
            rx_stats->packets += (&dadp->rx_rings[i])->total_packets;
        }
        if (rx_stats->op_code & DPA_RX_STATS_OP_BYTES) {
            rx_stats->bytes += (&dadp->rx_rings[i])->total_bytes;
        }
        #if 0
        if (rx_stats->op_code & DPA_RX_STATS_OP_DROPS) {
            rx_stats->drops += (adapter->rx_ring[i])->rx_stats.drops;
        }
        if (rx_stats->op_code & DPA_RX_STATS_OP_ERRORS) {
            /* 驱动只记录了csum错误 */
            rx_stats->errors += (adapter->rx_ring[i])->rx_stats.csum_err;
        }
        #endif
    }
    if (rx_stats->op_code & DPA_RX_STATS_OP_DROPS) {
        rx_stats->drops = netdev->stats.rx_missed_errors;
    }
    if (rx_stats->op_code & DPA_RX_STATS_OP_ERRORS) {
        rx_stats->errors = netdev->stats.rx_errors;
    }
    do_gettimeofday(&rx_stats->ts);
    return 0;

}

static void igb_dpa_attach(struct nic_adapter *adapter)
{
    struct dpa_adapter dadp;

    memzero(&dadp, sizeof(dadp));

    dadp.netdev = adapter->netdev;
    dadp.excl_locks = 1;
    dadp.dev_model = DPA_DEV_MODEL_INTEL_IGB;
    dadp.num_rx_rings = adapter->num_rx_queues;
    dadp.num_rx_descs = adapter->rx_ring_count;
    dadp.reg = igb_dpa_reg;
    dadp.rx_sync = igb_dpa_rx_sync;
    dadp.get_rx_stats = igb_dpa_get_rx_stats;
    dpa_vb_info("Intel igb using %d RX queues.", adapter->num_rx_queues);
    dpa_attach(&dadp);
}

#endif
