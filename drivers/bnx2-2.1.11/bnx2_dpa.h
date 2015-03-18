/* bnx2_dpa.h
 * bnx2驱动DPA适配层实现
 *
 * Direct Packet Access
 * Author:  <zhoubin@asiainfo.com>
 */

#ifndef _BNX2_DPA_H_
#define _BNX2_DPA_H_

#include "../../kernel/dpa_k.h"
#include "bnx2.h"
#include <linux/pci.h>

extern int dpa_flag_debug;
extern int dpa_flag_alter_promisc;
extern int dpa_flag_vlan_strip;
extern uint32_t dpa_num_rx_descs;

static int bnx2_dpa_reg(struct net_device *netdev, int reg)
{
    struct bnx2 *bp = netdev_priv(netdev);
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
    
    if (up && netif_running(bp->dev)) {
       
       /* Reset will erase chipset stats; save them */
        bnx2_save_stats(bp);

        bnx2_netif_stop(bp, true);
        bnx2_reset_chip(bp, BNX2_DRV_MSG_CODE_RESET);
        __bnx2_free_irq(bp);
        bnx2_free_skbs(bp);
        bnx2_free_mem(bp);   
        
    }

    if (reg) {
        DPA_IF_SET_ENABLE(netdev);
    } else {
        DPA_IF_SET_DISABLE(netdev);
    }
    
    if (up && netif_running(bp->dev)) {
        int rc;

        rc = bnx2_alloc_mem(bp);
        if (!rc) {
            rc = bnx2_request_irq(bp);
        }

        if (!rc) {
            rc = bnx2_init_nic(bp, 0);
        }

        if (rc) {
            bnx2_napi_enable(bp);
            dev_close(bp->dev); //??
            return rc;
        }
#ifdef BCM_CNIC
        mutex_lock(&bp->cnic_lock);
        /* Let cnic know about the new status block. */
        if (bp->cnic_eth_dev.drv_state & CNIC_DRV_STATE_REGD) {
            bnx2_setup_cnic_irq_info(bp);
        }
        mutex_unlock(&bp->cnic_lock);
#endif
        bnx2_netif_start(bp, true);
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
    return error;
}

static int bnx2_dpa_get_rx_stats(struct net_device *netdev, struct dpa_rx_stats *rx_stats)
{
    struct bnx2 *bp = netdev_priv(netdev);
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
    
    for (i = qbegin; i < qend; i++) {
        if (rx_stats->op_code & DPA_RX_STATS_OP_PACKETS) {
            rx_stats->packets += (&dadp->rx_rings[i])->total_packets;
        }
        if (rx_stats->op_code & DPA_RX_STATS_OP_BYTES) {
            rx_stats->bytes += (&dadp->rx_rings[i])->total_bytes;
        }
    }

    if (rx_stats->op_code & DPA_RX_STATS_OP_DROPS) {
        rx_stats->drops = GET_32BIT_NET_STATS(stat_IfInFTQDiscards) +
                            GET_32BIT_NET_STATS(stat_IfInMBUFDiscards) +
                            GET_32BIT_NET_STATS(stat_FwRxDrop);
    }
    if (rx_stats->op_code & DPA_RX_STATS_OP_ERRORS) {
        rx_stats->errors = GET_32BIT_NET_STATS(stat_Dot3StatsAlignmentErrors) +
                            GET_32BIT_NET_STATS(stat_Dot3StatsFCSErrors);
    }
    do_gettimeofday(&rx_stats->ts);
    return 0;

}

static int bnx2_dpa_rx_sync(struct net_device *netdev, uint32_t ring_nr, int do_lock)
{
    struct bnx2 *bp = netdev_priv(netdev);
    struct dpa_adapter *dadp = DPA_ADP(netdev);
    struct bnx2_rx_ring_info *rxr = &(bp->bnx2_napi[ring_nr].rx_ring);
    struct dpa_kring *kring = &dadp->rx_rings[ring_nr];
    struct dpa_ring *ring = kring->ring;
    uint32_t k, i = 0, j = 0, bytes = 0, num_slots = kring->nkr_num_slots - 1;
    int force_update = do_lock || kring->nr_kflags & DPA_KR_PEND_INTR;
    uint32_t u = ring->cur, resvd = ring->reserved;
    u32 max_ring_idx = bp->rx_max_ring_idx;
    u16 hw_cons, sw_cons, sw_prod, sw_ring_cons;
    struct sw_bd *rx_buf;
    dma_addr_t dma_addr;
    u32 status;
    int ibefore, iend;
    int pass_cycle = RX_RING(bp->rx_ring_size) + 1;
    
    /* 解决接口down时内核BUG ON问题 */
    if (!netif_carrier_ok(netdev)) {
        return 0;
    }
    
    if (u > num_slots) {
        return dpa_ring_reinit(kring, 1);
    }

    if (ring->hw_ofs != kring->nkr_hwofs) {
        dpa_info("Resynchronize hw offset of %s queue %d: %d -> %d.",
            kring->dadp->netdev->name, (uint32_t)(kring - kring->dadp->rx_rings),
            ring->hw_ofs, kring->nkr_hwofs);
        ring->hw_ofs = kring->nkr_hwofs;
    }

    if (do_lock) {
        dpa_sl_lock(&kring->q_lock);
    }
    rmb();

    /* 将新接收的包更新至ring
     * k是kring的索引，n是网卡ring索引 */
    hw_cons = bnx2_get_hw_rx_cons(&(bp->bnx2_napi[ring_nr]));
    sw_cons = rxr->rx_cons;
    sw_prod = rxr->rx_prod;
    
    k = dpa_idx_n2k(kring, RX_RING_IDX(sw_cons));
    ibefore = RX_RING(k);
    
    if (!dpa_pend_intr || force_update) {
        for (i = 0; i < bp->rx_ring_size; i++) {
            if ((i+kring->nr_hwavail) >= bp->rx_ring_size - 1) {
                break;
            }
            
            if (sw_cons == hw_cons) {
                hw_cons = bnx2_get_hw_rx_cons(&(bp->bnx2_napi[ring_nr]));
                rmb();
                if (sw_cons == hw_cons) {
                    break;
                }
            }

            sw_ring_cons = RX_RING_IDX(sw_cons);
            rx_buf = &rxr->rx_buf_ring[sw_ring_cons];
            
            dma_addr = pci_unmap_addr(rx_buf, mapping);

            dma_sync_single_for_cpu(&bp->pdev->dev, dma_addr,
                BNX2_RX_OFFSET + BNX2_RX_COPY_THRESH,
                PCI_DMA_FROMDEVICE);
            status = rx_buf->desc->l2_fhdr_status;
            ring->slot[k].len = rx_buf->desc->l2_fhdr_pkt_len-4;
            bytes += ring->slot[k].len;
            
            if ((status & L2_FHDR_STATUS_L2_VLAN_TAG) 
                && !(bp->rx_mode & BNX2_EMAC_RX_MODE_KEEP_VLAN_TAG)) {
                ring->slot[k].info = rx_buf->desc->l2_fhdr_vlan_tag & DPA_SLOT_INFO_VID;
            } else {
                ring->slot[k].info = rx_buf->desc->l2_fhdr_vlan_tag & 0;
            }

            sw_cons = NEXT_RX_BD(sw_cons);
            k = dpa_idx_n2k(kring, RX_RING_IDX(sw_cons));
             
        }
        
        if (i) { 
            /* 更新网卡的统计计数 */
            kring->total_packets += i;
            kring->total_bytes += bytes;
            kring->nr_hwavail += i;
            rxr->rx_cons = sw_cons;
            iend = RX_RING(k);
            kring->nr_hwavail += (iend < ibefore) ? 
                                pass_cycle + iend - ibefore : iend - ibefore;
            if (iend == ibefore && i > MAX_RX_DESC_CNT) {
                kring->nr_hwavail += pass_cycle;
            }
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
        for (j = 0; k != u; j++) {
            struct dpa_slot *slot = &ring->slot[k];
            uint64_t paddr;
            void *addr = BUF_VPA(slot, &paddr);

            if (addr == dpa_buffer_base) {
                if (do_lock) {
                    dpa_sl_unlock(&kring->q_lock);
                }
                return dpa_ring_reinit(kring, 0);
            }

            k = (k == max_ring_idx) ? 0 : (k + 1);
            
            if ((dpa_idx_k2n(kring, k) & MAX_RX_DESC_CNT) !=  MAX_RX_DESC_CNT) {
               sw_prod = NEXT_RX_BD(sw_prod);
            }
        }
        
        if (j) {
            kring->nr_hwavail -= j;
            kring->nr_hwcur = u;
            rxr->rx_prod = sw_prod;
            REG_WR16(bp, rxr->rx_bidx_addr, sw_prod);
        }       
        //wmb();
    }

    /* 通知用户进程包已就绪 */
    ring->avail = kring->nr_hwavail - resvd;

    if (do_lock) {
        dpa_sl_unlock(&kring->q_lock);
    }
    return 0;
}

static int bnx2_dpa_conf_rx_ring(struct bnx2 *bp, struct bnx2_rx_ring_info *rxr, uint32_t ring_id)
{
    struct rx_bd *rxbd;
    struct sw_bd *rx_buf;
    uint32_t i;
    u16 prod, ring_prod;
    uint64_t paddr;
    int si;
    struct net_device *netdev = bp->dev;
    struct dpa_adapter *dadp = DPA_ADP(netdev);
    struct dpa_slot *slot = dpa_reset(dadp, ring_id, 0);
    ring_prod = prod = rxr->rx_prod;
    
    if (!slot) {
        return 0;
    }

    for (i = 0; i < bp->rx_ring_size; i++) {
        rx_buf = &rxr->rx_buf_ring[ring_prod];
        rxbd = &rxr->rx_desc_ring[RX_RING(ring_prod)][RX_IDX(ring_prod)];
        si = dpa_idx_n2k(&dadp->rx_rings[ring_id], ring_prod);
        rx_buf->skb = NULL;
        rx_buf->desc = (struct l2_fhdr *)BUF_VPA(slot + si, &paddr);
        pci_unmap_addr_set(rx_buf, mapping, paddr);
        rxbd->rx_bd_haddr_hi = paddr >> 32;
        rxbd->rx_bd_haddr_lo = paddr & 0xffffffff;
        rxr->rx_prod_bseq += bp->rx_buf_use_size;
        prod = NEXT_RX_BD(prod);
        ring_prod = RX_RING_IDX(prod);
    }
    rxr->rx_prod = prod;
    return 1;
}

static void bnx2_dpa_attach(struct bnx2 *adapter)
{
    struct dpa_adapter dadp;

    memzero(&dadp, sizeof(dadp));

    dadp.netdev = adapter->dev;
    dadp.excl_locks = 1;
    //bnx2数据包在buffer的BNX2_RX_OFFSET偏移量开始
    dadp.dev_model = DPA_DEV_MODEL_BROADCOM_BNX2;
    dadp.num_rx_rings = adapter->num_rx_rings;
    //此处填写rx_max_ring_idx而不使用rx_ring_size为了使用户的ring的索引和网卡ring的索引同步
    dadp.num_rx_descs = adapter->rx_max_ring_idx + 1;
    dadp.reg = bnx2_dpa_reg;
    dadp.rx_sync = bnx2_dpa_rx_sync;
    dadp.get_rx_stats = bnx2_dpa_get_rx_stats;
    dpa_vb_info("bnx2 using %d RX queues.", adapter->rx_ring_size);
    dpa_attach(&dadp);
}
#endif
