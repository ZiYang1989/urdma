/* interface.h */

/*
 * Userspace Software iWARP library for DPDK
 *
 * Authors: Patrick MacArthur <pam@zurich.ibm.com>
 *
 * Copyright (c) 2016, IBM Corporation
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *   - Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *   - Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *   - Neither the name of IBM nor the names of its contributors may be
 *     used to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef INTERFACE_H
#define INTERFACE_H

#include <stdbool.h>
#include <semaphore.h>

#include <infiniband/driver.h>

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_hash.h>
#include <rte_kni.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ring.h>
#include <rte_spinlock.h>

#include "list.h"
#include "verbs.h"

#define TX_BURST_SIZE 8
#define RX_BURST_SIZE 32
#define DPDKV_MAX_QP 64
#define MAX_ARP_ENTRIES 32
#define MAX_REMOTE_ENDPOINTS 32
#define MAX_RECV_WR 1023
#define MAX_SEND_WR 1023
#define DPDK_VERBS_IOV_LEN_MAX 32
#define DPDK_VERBS_RDMA_READ_IOV_LEN_MAX 1
#define MAX_MR_SIZE (UINT32_C(1) << 30)
#define USIW_IRD_MAX 128
#define USIW_ORD_MAX 128

#define STAG_TYPE_MASK      UINT32_C(0xFF000000)
#define STAG_MASK           UINT32_C(0x00FFFFFF)
#define STAG_TYPE_MR        (UINT32_C(0x00) << 24)
#define STAG_TYPE_RDMA_READ (UINT32_C(0x01) << 24)
#define STAG_RDMA_READ(x) (STAG_TYPE_RDMA_READ | ((x) & STAG_MASK))

struct usiw_context;
struct usiw_port;
struct usiw_qp;

struct arp_entry {
	struct ether_addr ether_addr;
	struct usiw_send_wqe *request;
};

struct usiw_recv_ooo_range {
	uint64_t offset_start;
	uint64_t offset_end;
};

struct usiw_wc {
	void *wr_context;
	enum ibv_wc_status status;
	enum ibv_wc_opcode opcode;
	uint32_t byte_len;
	uint32_t qp_num;
	struct usiw_ah ah;
};

struct usiw_recv_wqe {
	void *wr_context;
	struct ee_state *remote_ep;
	TAILQ_ENTRY(usiw_recv_wqe) active;
	uint32_t msn;
	uint32_t index;
	size_t total_request_size;
	size_t recv_size;
	size_t input_size;

	size_t iov_count;
	struct iovec iov[];
};

struct pending_datagram_info {
	uint64_t next_retransmit;
	struct usiw_send_wqe *wqe;
	uint16_t transmit_count;
	uint16_t ddp_length;
	uint32_t ddp_raw_cksum;
	uint32_t psn;
};

enum usiw_send_wqe_state {
	SEND_WQE_INIT = 0,
	SEND_WQE_TRANSFER,
	SEND_WQE_WAIT,
	SEND_WQE_COMPLETE,
};

enum usiw_send_opcode {
	usiw_wr_send = 0,
	usiw_wr_write = 1,
	usiw_wr_read = 2,
};

enum {
	usiw_send_signaled = 1,
	usiw_send_inline = 2,
};

struct usiw_send_wqe {
	enum usiw_send_opcode opcode;
	void *wr_context;
	struct ee_state *remote_ep;
	uint64_t remote_addr;
	uint32_t rkey;
	uint32_t flags;
	TAILQ_ENTRY(usiw_send_wqe) active;
	uint32_t index;
	enum usiw_send_wqe_state state;
	uint32_t msn;
	uint32_t local_stag; /* only used for READs */
	size_t total_length;
	size_t bytes_sent;
	size_t bytes_acked;

	size_t iov_count;
	struct iovec iov[];
};

struct usiw_mr {
	struct ibv_mr mr;
	struct usiw_mr *next;
	int access;
};

/* Lookup table for memory regions */
struct usiw_mr_table {
	struct ibv_pd pd;
	size_t capacity;
	size_t mr_count;
	struct usiw_mr *entries[0];
};

struct usiw_send_wqe_queue {
	struct rte_hash *active;
	struct rte_ring *ring;
	TAILQ_HEAD(usiw_send_wqe_active_head, usiw_send_wqe) active_head;
	uint64_t *bitmask;
	char *storage;
	int max_wr;
	int max_sge;
	unsigned int max_inline;
	rte_spinlock_t lock;
};

struct usiw_recv_wqe_queue {
	struct rte_hash *active;
	struct rte_ring *ring;
	TAILQ_HEAD(usiw_recv_wqe_active_head, usiw_recv_wqe) active_head;
	uint64_t *bitmask;
	char *storage;
	int max_wr;
	int max_sge;
	rte_spinlock_t lock;
};

struct psn_range {
	uint32_t min;
	uint32_t max;
};

enum {
	trp_recv_missing = 1,
	trp_ack_update = 2,
};

struct ee_state {
	struct usiw_ah ah;
	uint32_t expected_recv_msn;
	uint32_t expected_read_msn;
	uint32_t expected_ack_msn;
	uint32_t next_send_msn;
	uint32_t next_read_msn;
	uint32_t next_ack_msn;

	/* TX TRP state */
	uint32_t send_last_acked_psn;
	uint32_t send_next_psn;
	uint32_t send_max_psn;

	/* RX TRP state */
	uint32_t recv_ack_psn;

	uint32_t trp_flags;
	struct psn_range recv_sack_psn;

	struct rte_mbuf **tx_pending;
	struct rte_mbuf **tx_head;
	int tx_pending_size;

	/* This fields are only used if the NIC does not support
	 * filtering. */
	struct rte_ring *rx_queue;
};

struct read_response_state {
	char *vaddr;
	uint32_t msg_size;
	uint32_t sink_stag; /* network byte order */
	uint64_t sink_offset; /* host byte order */
	struct ee_state *sink_ep;
	TAILQ_ENTRY(read_response_state) qp_entry;
};

enum usiw_qp_state {
	usiw_qp_unbound = 0,
	usiw_qp_connected = 1,
	usiw_qp_shutdown = 2,
	usiw_qp_error = 3,
};

enum usiw_qp_type {
	usiw_qp_rd = 0,
	usiw_qp_rc = 1,
};

enum {
	usiw_qp_sig_all = 0x1,
};

DECLARE_TAILQ_HEAD(read_response_state);

/** This structure contains fields used by my initial reliable datagram-style
 * verbs interface.  This will be used for transition to the reliable connected
 * queue pairs and the libibverbs interface. */
struct usiw_qp {
	rte_atomic32_t refcnt;
	uint16_t qp_type;
	uint16_t udp_port;
	uint16_t rx_queue;
	uint16_t tx_queue;
	uint16_t qp_flags;
	rte_atomic16_t conn_state;
	enum ibv_qp_state qp_state;
	rte_spinlock_t conn_event_lock;
	sem_t conn_event_sem;

	LIST_ENTRY(usiw_qp) ctx_entry;
	struct usiw_context *ctx;
	struct usiw_port *port;
	struct usiw_cq *send_cq;

	/* txq_end points one entry beyond the last entry in the table
	 * the table is full when txq_end == txq + TX_BURST_SIZE
	 * the burst should be flushed at that point
	 */
	struct rte_mbuf **txq_end;
	struct rte_mbuf *txq[TX_BURST_SIZE];

	struct usiw_send_wqe_queue sq;

	struct rte_eth_fdir_filter fdir_filter;
        struct usiw_qp_stats stats;

	uint64_t timer_last;
	struct usiw_recv_wqe_queue rq0;

	struct read_response_state *readresp_store;
	struct read_response_state_tailq_head readresp_active;
	struct read_response_state_tailq_head readresp_empty;
	uint8_t ord_max;
	uint8_t ird_max;
	uint8_t ird_active;

	struct usiw_cq *recv_cq;
	struct usiw_mr_table *pd;

	struct ee_state *(*get_ee_context)(struct usiw_qp *qp,
						struct usiw_ah *ah);
	struct ee_state *ep_default;
	struct rte_hash *ee_state_table;
	struct ee_state ee_state_entries[MAX_REMOTE_ENDPOINTS];

	struct ibv_qp ib_qp;
};

struct usiw_cq {
	struct ibv_cq ib_cq;
	struct rte_ring *cqe_ring;
	struct rte_ring *free_ring;
	struct usiw_wc *storage;
	size_t capacity;
	size_t qp_count;
	uint32_t cq_id;
	rte_atomic32_t notify_count;
	rte_spinlock_t lock;
};

enum usiw_port_flags {
	port_checksum_offload = 1,
	port_fdir = 2,
};

struct usiw_port {
	int portid;
	uint64_t timer_freq;

	struct rte_mempool *rx_mempool;
	struct rte_mempool *tx_ddp_mempool;
	struct rte_mempool *tx_hdr_mempool;
	struct usiw_context *ctx;

	uint16_t rx_desc_count;
	uint16_t tx_desc_count;

	uint64_t flags;
	uint64_t qp_bitmask;
	uint16_t max_qp;

	struct rte_kni *kni;
	struct ether_addr ether_addr;
	uint32_t ipv4_addr;
	int ipv4_prefix_len;

	char kni_name[RTE_KNI_NAMESIZE];
	struct rte_eth_dev_info dev_info;
};

struct usiw_context {
	struct verbs_context vcontext;
	struct usiw_port *port;
	int event_fd;
	LIST_HEAD(usiw_qp_head, usiw_qp) qp_active;
	rte_atomic32_t qp_init_count;
		/**< The number of queue pairs in the INIT state. */
	struct rte_hash *qp;
		/**< Hash table of all non-destroyed queue pairs in any
		 * state.  Guarded by qp_lock. */
	rte_spinlock_t qp_lock;
};


static inline struct usiw_context *
usiw_get_context(struct ibv_context *ctx)
{
	struct verbs_context *vctx = verbs_get_ctx(ctx);
	return container_of(vctx, struct usiw_context, vcontext);
} /* usiw_get_context */

struct usiw_device {
	struct verbs_device vdev;
	struct usiw_port *port;
};

struct usiw_driver {
	struct nl_sock *nl_sock;
	struct nl_cache *nl_link_cache;

	int port_count;
	uint16_t progress_lcore;
	struct usiw_port ports[];
};

struct usiw_mr **
usiw_mr_lookup(struct usiw_mr_table *tbl, uint32_t rkey);

/* Internal-only helper used by usiw_dereg_mr */
void
usiw_dereg_mr_real(struct usiw_mr_table *tbl, struct usiw_mr **mr);

/* Places a pointer to the next send WQE in *wqe and returns 0 if one is
 * available.  If one is not available, returns -ENOSPC.
 *
 * The caller is responsible for enqueuing the WQE after it is filled in;
 * otherwise the behavior is undefined. */
int
qp_get_next_send_wqe(struct usiw_qp *qp, struct usiw_send_wqe **wqe);

/* Returns the send WQE to the free set.  The caller must set still_in_hash if
 * the WQE is in the hashtable, in which case this function will also remove
 * the WQE from the hash. */
void
qp_free_send_wqe(struct usiw_qp *qp, struct usiw_send_wqe *wqe,
		bool still_in_hash);

/* Places a pointer to the next receive WQE in *wqe and returns 0 if one is
 * available.  If one is not available, returns -ENOSPC.
 *
 * The caller is responsible for enqueuing the WQE after it is filled in;
 * otherwise the behavior is undefined. */
int
qp_get_next_recv_wqe(struct usiw_qp *qp, struct usiw_recv_wqe **wqe);

int
usiw_send_wqe_queue_init(uint32_t qpn, struct usiw_send_wqe_queue *q,
		uint32_t max_send_wr, uint32_t max_sge);

void
usiw_send_wqe_queue_destroy(struct usiw_send_wqe_queue *q);

int
usiw_recv_wqe_queue_init(uint32_t qpn, struct usiw_recv_wqe_queue *q,
		uint32_t max_recv_wr, uint32_t max_sge);

void
usiw_recv_wqe_queue_destroy(struct usiw_recv_wqe_queue *q);

struct ee_state *
usiw_get_ee_context_rc(struct usiw_qp *qp, struct usiw_ah *ah);

struct ee_state *
usiw_get_ee_context_rd(struct usiw_qp *qp, struct usiw_ah *ah);

void
usiw_do_destroy_qp(struct usiw_qp *qp);

int
kni_loop(void *arg);

#ifdef NDEBUG
#define cq_check_sanity(x) do { } while (0)
#else
void
cq_check_sanity(struct usiw_cq *cq);
#endif

/* These two functions are actually defined in verbs.h but are *not* intended
 * to be public API, so they are declared here in this internal-only header
 * instead. */
int
usiw_init_context(struct verbs_device *device, struct ibv_context *context,
		int cmd_fd);

void
usiw_uninit_context(struct verbs_device *device, struct ibv_context *ctx);

#endif
