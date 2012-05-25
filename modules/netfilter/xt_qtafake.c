/*
 * Light xt_qtaguid for 2.6.32.9 compat.
 *
 * originaly made to prevent BattStats/NetworkStats FC
 * related to network usage in ICS
 *
 * (C) 2011-2012 Tanguy Pruvot, Stub Module then Backport
 *
 * v0.5: Basic module stub, which only create proc/self/net entries
 * v0.6: Implement the rb trees, and basic tcp packet identification
 *
 * TODO: Original module could be compiled with the work made here
 *       else... we need to list the network intf, and link stuff
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/types.h>
#include "atomic.h"

#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/cred.h>
#include <linux/skbuff.h>
#include <linux/file.h>
#include <linux/proc_fs.h>
#include <linux/workqueue.h>

#include <net/sock.h>
#include <net/tcp.h>
#include <net/udp.h>

#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_owner.h>
#include <linux/netfilter/xt_socket.h>

/* TODO: We dont have all the ip6_tables in our kernel (ipv6_find_hdr) */
#include <linux/netfilter_ipv6/ip6_tables.h>

#include "xt_qtaguid_internal.h"
#include "xt_qtaguid_print.h"

/* to prevent iptables error, we need also OUTPUT filter hook,
   but real filter is set here */
#define XT_SOCKET_SUPPORTED_HOOKS \
	((1 << NF_INET_PRE_ROUTING) | (1 << NF_INET_LOCAL_IN))

#define QTA_PROC_DIR "xt_qtaguid"
#define QTU_DEV_NAME "xt_qtaguid"

#define TAG "xt_qtaguid"
#define DEBUG_LOGS 1

/* this one is exported */
module_param_named(debug_mask, qtaguid_debug_mask, uint, S_IRUGO | S_IWUSR);

static unsigned int proc_ctrl_perms = S_IRUGO | S_IWUSR;
module_param_named(ctrl_perms, proc_ctrl_perms, uint, S_IRUGO | S_IWUSR);

static gid_t proc_stats_readall_gid;
static gid_t proc_ctrl_write_gid;
module_param_named(stats_readall_gid, proc_stats_readall_gid, uint, S_IRUGO | S_IWUSR);
module_param_named(ctrl_write_gid, proc_ctrl_write_gid, uint, S_IRUGO | S_IWUSR);

/*
 * Limit the number of active tags (via socket tags) for a given UID.
 * Multiple processes could share the UID.
 */
static int max_sock_tags = DEFAULT_MAX_SOCK_TAGS;
module_param(max_sock_tags, int, S_IRUGO | S_IWUSR);

/*
 * After the kernel has initiallized this module, it is still possible
 * to make it passive
 */
static bool module_passive = false;
module_param_named(passive, module_passive, bool, S_IRUGO | S_IWUSR);

static struct proc_dir_entry *proc_root = NULL;

/*
 * Data lists
 *
 */
static LIST_HEAD(iface_stat_list);
static DEFINE_SPINLOCK(iface_stat_list_lock);

static struct rb_root sock_tag_tree = RB_ROOT;
static DEFINE_SPINLOCK(sock_tag_list_lock);

static struct rb_root tag_counter_set_tree = RB_ROOT;
static DEFINE_SPINLOCK(tag_counter_set_list_lock);

static struct rb_root uid_tag_data_tree = RB_ROOT;
static DEFINE_SPINLOCK(uid_tag_data_tree_lock);

static struct rb_root proc_qtu_data_tree = RB_ROOT;
/* No proc_qtu_data_tree_lock; use uid_tag_data_tree_lock */

static struct qtaguid_event_counts qtu_events;

/*****************************************************************************/
static bool can_manipulate_uids(void)
{
	return unlikely(!current_fsuid()) || unlikely(!proc_ctrl_write_gid)
		|| in_egroup_p(proc_ctrl_write_gid);
}

static bool can_impersonate_uid(uid_t uid)
{
	return uid == current_fsuid() || can_manipulate_uids();
}

/*****************************************************************************/
static struct tag_node *tag_node_tree_search(struct rb_root *root, tag_t tag)
{
	struct rb_node *node = root->rb_node;
	while (node) {
		struct tag_node *data = rb_entry(node, struct tag_node, node);
		int result;
		result = tag_compare(tag, data->tag);
		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else
			return data;
	}
	return NULL;
}

static void tag_node_tree_insert(struct tag_node *data, struct rb_root *root)
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;
	/* Figure out where to put new node */
	while (*new) {
		struct tag_node *this =
			 rb_entry(*new, struct tag_node, node);
		int result = tag_compare(data->tag, this->tag);
		parent = *new;
		if (result < 0)
			new = &((*new)->rb_left);
		else if (result > 0)
			new = &((*new)->rb_right);
		else
			BUG();
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, root);
}
/*****************************************************************************/
static void tag_stat_tree_insert(struct tag_stat *data, struct rb_root *root)
{
	tag_node_tree_insert(&data->tn, root);
}

static struct tag_stat *tag_stat_tree_search(struct rb_root *root, tag_t tag)
{
	struct tag_node *node = tag_node_tree_search(root, tag);
	if (!node)
		return NULL;
	return rb_entry(&node->node, struct tag_stat, tn.node);
}
/*****************************************************************************/
static void tag_counter_set_tree_insert(struct tag_counter_set *data, struct rb_root *root)
{
	tag_node_tree_insert(&data->tn, root);
}

static struct tag_counter_set *tag_counter_set_tree_search(struct rb_root *root,
		tag_t tag)
{
	struct tag_node *node = tag_node_tree_search(root, tag);
	if (!node)
		return NULL;
	return rb_entry(&node->node, struct tag_counter_set, tn.node);
}

static int get_active_counter_set(tag_t tag)
{
	int active_set = 0;
	struct tag_counter_set *tcs;

	MT_DEBUG(TAG": get_active_counter_set(tag=0x%llx)"
		 " (uid=%u)\n",
		 tag, get_uid_from_tag(tag));

	/* For now we only handle UID tags for active sets */
	tag = get_utag_from_tag(tag);
	spin_lock_bh(&tag_counter_set_list_lock);
	tcs = tag_counter_set_tree_search(&tag_counter_set_tree, tag);
	if (tcs)
		active_set = tcs->active_set;
	spin_unlock_bh(&tag_counter_set_list_lock);
	return active_set;
}

/*****************************************************************************/
static void tag_ref_tree_insert(struct tag_ref *data, struct rb_root *root)
{
	tag_node_tree_insert(&data->tn, root);
}

static struct tag_ref *tag_ref_tree_search(struct rb_root *root, tag_t tag)
{
	struct tag_node *node = tag_node_tree_search(root, tag);
	if (!node)
		return NULL;
	return rb_entry(&node->node, struct tag_ref, tn.node);
}

static struct tag_ref *new_tag_ref(tag_t new_tag, struct uid_tag_data *utd_entry)
{
        struct tag_ref *tr_entry = NULL;
	int res = 0;

	if (utd_entry->num_active_tags + 1 > max_sock_tags) {
		res = -EMFILE;
		goto err_res;
	}

	tr_entry = kzalloc(sizeof(*tr_entry), GFP_ATOMIC);
	if (!tr_entry) {
		res = -ENOMEM;
		goto err_res;
	}

	tr_entry->tn.tag = new_tag;
	utd_entry->num_active_tags++;
	tag_ref_tree_insert(tr_entry, &utd_entry->tag_ref_tree);

	pr_info(TAG": new_tag_ref(0x%llx): "
		 " allocated new tag ref %p size=%d\n",
		new_tag, tr_entry, sizeof(*tr_entry));

	return tr_entry;
err_res:
	return ERR_PTR(res);
}

static struct tag_ref *lookup_tag_ref(tag_t full_tag, struct uid_tag_data **utd_res)
{
	struct uid_tag_data *utd_entry;
	struct tag_ref *tr_entry;
	bool found_utd;
	uid_t uid = get_uid_from_tag(full_tag);

	utd_entry = get_uid_data(uid, &found_utd);
	if (IS_ERR_OR_NULL(utd_entry)) {
		if (utd_res)
			*utd_res = utd_entry;
		return NULL;
	}

	tr_entry = tag_ref_tree_search(&utd_entry->tag_ref_tree, full_tag);
	if (utd_res)
		*utd_res = utd_entry;

	DR_DEBUG(TAG": lookup_tag_ref(0x%llx) utd_entry=%p tr_entry=%p\n",
		full_tag, utd_entry, tr_entry);

	return tr_entry;
}

/* Never returns NULL. Either PTR_ERR or a valid ptr. */
static struct tag_ref *get_tag_ref(tag_t full_tag, struct uid_tag_data **utd_res)
{
	struct uid_tag_data *utd_entry;
	struct tag_ref *tr_entry;

	spin_lock_bh(&uid_tag_data_tree_lock);
	tr_entry = lookup_tag_ref(full_tag, &utd_entry);
	BUG_ON(IS_ERR_OR_NULL(utd_entry));
	if (!tr_entry)
		tr_entry = new_tag_ref(full_tag, utd_entry);
	spin_unlock_bh(&uid_tag_data_tree_lock);

	if (utd_res)
		*utd_res = utd_entry;

	DR_DEBUG(TAG": get_tag_ref(0x%llx) utd=%p tr=%p\n",
		full_tag, utd_entry, tr_entry);

	return tr_entry;
}

/* Checks and maybe frees the UID Tag Data entry */
static void put_utd_entry(struct uid_tag_data *utd_entry)
{
	if (RB_EMPTY_ROOT(&utd_entry->tag_ref_tree) && !utd_entry->num_pqd)
	{
		pr_info(TAG": %s(): "
			"erase utd_entry=%p uid=%u "
			"by pid=%u tgid=%u uid=%u\n", __func__,
			utd_entry, utd_entry->uid,
			current->pid, current->tgid, current_fsuid());

		WARN_ON(utd_entry->num_active_tags);
		rb_erase(&utd_entry->node, &uid_tag_data_tree);
		kfree(utd_entry);
	} else {
		DR_DEBUG(TAG": %s(): "
			"utd_entry=%p still has %d tags %d proc_qtu_data\n",
			__func__, utd_entry, utd_entry->num_active_tags,
			utd_entry->num_pqd);

		BUG_ON(!(utd_entry->num_active_tags || utd_entry->num_pqd));
	}
}

static void free_tag_ref_from_utd_entry(struct tag_ref *tr_entry,
			struct uid_tag_data *utd_entry)
{
	if (!tr_entry->num_sock_tags) {
		WARN_ON(!utd_entry->num_active_tags);
		utd_entry->num_active_tags--;
		rb_erase(&tr_entry->tn.node, &utd_entry->tag_ref_tree);
		DR_DEBUG(TAG": %s(): erased %p\n", __func__, tr_entry);
		kfree(tr_entry);
	}
}

static void put_tag_ref_tree(tag_t full_tag, struct uid_tag_data *utd_entry)
{
	struct rb_node *node;
	struct tag_ref *tr_entry;
	tag_t acct_tag;

	DR_DEBUG(TAG": %s(tag=0x%llx (uid=%u))\n", __func__,
		full_tag, get_uid_from_tag(full_tag));

	acct_tag = get_atag_from_tag(full_tag);
	node = rb_first(&utd_entry->tag_ref_tree);
	while (node) {
		tr_entry = rb_entry(node, struct tag_ref, tn.node);
		node = rb_next(node);
		if (!acct_tag || tr_entry->tn.tag == full_tag)
			free_tag_ref_from_utd_entry(tr_entry, utd_entry);
	}
}

/*****************************************************************************/
static void uid_tag_data_tree_insert(struct uid_tag_data *data, struct rb_root *root)
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		struct uid_tag_data *this =
			rb_entry(*new, struct uid_tag_data, node);

		parent = *new;
		if (data->uid < this->uid)
			new = &((*new)->rb_left);
		else if (data->uid > this->uid)
			new = &((*new)->rb_right);
		else
			BUG();
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, root);
}

static struct uid_tag_data *uid_tag_data_tree_search(struct rb_root *root, uid_t uid)
{
	struct rb_node *node = root->rb_node;

	while (node) {
		struct uid_tag_data *data =
			rb_entry(node, struct uid_tag_data, node);

		if (uid < data->uid)
			node = node->rb_left;
		else if (uid > data->uid)
			node = node->rb_right;
		else
			return data;
	}
	return NULL;
}

struct uid_tag_data *get_uid_data(uid_t uid, bool *found_res)
{
	struct uid_tag_data *utd_entry = NULL;

	/* Look for top level uid_tag_data for the UID */
	utd_entry = uid_tag_data_tree_search(&uid_tag_data_tree, uid);
	pr_info(TAG": get_uid_data(%u) utd=%p\n", uid, utd_entry);
	if (found_res)
		*found_res = utd_entry;
	if (utd_entry)
		return utd_entry;

	utd_entry = kzalloc(sizeof(*utd_entry), GFP_ATOMIC);
	if (!utd_entry) {
		return ERR_PTR(-ENOMEM);
	}

	utd_entry->uid = uid;
	utd_entry->tag_ref_tree = RB_ROOT;
	uid_tag_data_tree_insert(utd_entry, &uid_tag_data_tree);

	pr_info(TAG": get_uid_data(%u) allocated new utd=%p size=%d\n",
		uid, utd_entry, sizeof(*utd_entry));

	return utd_entry;
}

/*****************************************************************************/
static struct sock_tag *sock_tag_tree_search(struct rb_root *root,
					     const struct sock *sk)
{
	struct rb_node *node = root->rb_node;

	while (node) {
		struct sock_tag *data = rb_entry(node, struct sock_tag,
						 sock_node);
		if (sk < data->sk)
			node = node->rb_left;
		else if (sk > data->sk)
			node = node->rb_right;
		else
			return data;
	}
	return NULL;
}

static void sock_tag_tree_insert(struct sock_tag *data, struct rb_root *root)
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		struct sock_tag *this =
			rb_entry(*new, struct sock_tag, sock_node);

		parent = *new;
		if (data->sk < this->sk)
			new = &((*new)->rb_left);
		else if (data->sk > this->sk)
			new = &((*new)->rb_right);
		else
			BUG();
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->sock_node, parent, new);
	rb_insert_color(&data->sock_node, root);
}

static void sock_tag_tree_erase(struct rb_root *st_to_free_tree)
{
	struct rb_node *node;
	struct sock_tag *st_entry;

	node = rb_first(st_to_free_tree);
	while (node) {
		st_entry = rb_entry(node, struct sock_tag, sock_node);
		node = rb_next(node);
		CT_DEBUG(TAG ": %s(): "
			"erase st: sk=%p tag=0x%llx (uid=%u)\n", __func__,
			st_entry->sk,
			st_entry->tag,
			get_uid_from_tag(st_entry->tag));
		rb_erase(&st_entry->sock_node, st_to_free_tree);
		sockfd_put(st_entry->socket);
		kfree(st_entry);
	}
}

static struct sock_tag *get_sock_stat_nl(const struct sock *sk)
{
	return sock_tag_tree_search(&sock_tag_tree, sk);
}

static struct sock_tag *get_sock_stat(const struct sock *sk)
{
	struct sock_tag *sock_tag_entry;
	if (!sk)
		return NULL;
	spin_lock_bh(&sock_tag_list_lock);
	sock_tag_entry = get_sock_stat_nl(sk);
	spin_unlock_bh(&sock_tag_list_lock);
}

/*****************************************************************************/

static inline void dc_add_byte_packets(struct data_counters *counters, int set,
	enum ifs_tx_rx direction, enum ifs_proto ifs_proto, int bytes, int packets)
{
	counters->bpc[set][direction][ifs_proto].bytes += bytes;
	counters->bpc[set][direction][ifs_proto].packets += packets;
}

static inline uint64_t dc_sum_bytes(struct data_counters *counters,
	int set, enum ifs_tx_rx direction)
{
	return counters->bpc[set][direction][IFS_TCP].bytes
		+ counters->bpc[set][direction][IFS_UDP].bytes
		+ counters->bpc[set][direction][IFS_PROTO_OTHER].bytes;
}

static inline uint64_t dc_sum_packets(struct data_counters *counters,
	int set, enum ifs_tx_rx direction)
{
	return counters->bpc[set][direction][IFS_TCP].packets
		+ counters->bpc[set][direction][IFS_UDP].packets
		+ counters->bpc[set][direction][IFS_PROTO_OTHER].packets;
}

static void data_counters_update(struct data_counters *dc, int set,
	enum ifs_tx_rx direction, int proto, int bytes)
{
	switch (proto) {
	case IPPROTO_TCP:
		dc_add_byte_packets(dc, set, direction, IFS_TCP, bytes, 1);
		break;
	case IPPROTO_UDP:
		dc_add_byte_packets(dc, set, direction, IFS_UDP, bytes, 1);
		break;
	case IPPROTO_IP:
	default:
		dc_add_byte_packets(dc, set, direction, IFS_PROTO_OTHER, bytes, 1);
	}
}

static void tag_stat_update(struct tag_stat *tag_entry,
	enum ifs_tx_rx direction, int proto, int bytes)
{
	int active_set;
	active_set = get_active_counter_set(tag_entry->tn.tag);

	pr_info(TAG": tag_stat_update(tag=0x%llx (uid=%u) set=%d "
		"dir=%d proto=%d bytes=%d)\n",
		tag_entry->tn.tag, get_uid_from_tag(tag_entry->tn.tag),
		active_set, direction, proto, bytes);

	data_counters_update(&tag_entry->counters, active_set, direction,
		proto, bytes);

	if (tag_entry->parent_counters) {
		data_counters_update(tag_entry->parent_counters, active_set,
			direction, proto, bytes);
	}
}

/*****************************************************************************/

static struct sock *qtaguid_find_sk(const struct sk_buff *skb,
	const struct xt_match_param *par)
{
	const struct iphdr *iph = NULL;
	struct ipv6hdr *iph6 = NULL;
	struct udphdr _hdr, *hp;
	int tproto = 0, thoff;
	struct sock *sk = NULL;
	unsigned int hook_type = (1 << par->hooknum);

	if (!(hook_type & XT_SOCKET_SUPPORTED_HOOKS))
		return NULL;

	switch (par->family) {
	case NFPROTO_IPV4:
		/* sk = xt_socket_get4_sk(skb, par); */
		iph = ip_hdr(skb);
		if (iph->protocol == IPPROTO_UDP || iph->protocol == IPPROTO_TCP) {
			hp = skb_header_pointer(skb, ip_hdrlen(skb), sizeof(_hdr), &_hdr);
			pr_info(TAG": IPV4(%u) packet hdr=%p\n", par->family, hp);
		}
		break;
	case NFPROTO_IPV6:
		/* sk = xt_socket_get6_sk(skb, par); */
		iph6 = ipv6_hdr(skb);
		//TODO: ipv6_find_tlv is available, but not ipv6_find_hdr
		//tproto = ipv6_find_hdr(skb, &thoff, -1, NULL);
		if (tproto == IPPROTO_UDP || tproto == IPPROTO_TCP) {
			hp = skb_header_pointer(skb, thoff, sizeof(_hdr), &_hdr);

		/*	saddr = &iph6->saddr;
			sport = hp->source;
			daddr = &iph6->daddr;
			dport = hp->dest;
		*/
			pr_info(TAG": IPV6(%u) packet hdr=%p\n", par->family, iph6);
		}
		break;
	default:
		pr_warning(TAG": unknown protocol %u\n", par->family);
	}
	if (sk) {
		pr_info(TAG": find_sk(%p) ->sk_proto=%u "
			"->sk_state=%d\n", sk, sk->sk_protocol, sk->sk_state);
	}
	return sk;
}

static void xt_socket_put_sk(struct sock *sk)
{
	if (sk != NULL) sock_put(sk);
}

/*
 * Match filter
 */
static bool
qta_owner2_mt(const struct sk_buff *skb, const struct xt_match_param *par)
{
	bool res = false;
	const struct xt_owner_match_info *info = par->matchinfo;
	const struct file *filp;
	unsigned int hook_type = (1 << par->hooknum);
	struct sock *sk;
	bool got_sock = false;

	if (!(hook_type & XT_SOCKET_SUPPORTED_HOOKS)) {
		return false;
	}

	atomic64_inc(&qtu_events.match_calls);

	/* default res */
	res = (info->match ^ info->invert) == 0;

	if (skb == NULL  || unlikely(module_passive)) {
		return res;
	}

#if (DEBUG_LOGS)
	pr_info(TAG": match family=%u info->match=0x%x, invert=0x%x def=%d\n",
		par->family, info->match, info->invert, res?1:0);
#endif

	sk = skb->sk;
	if (sk == NULL) {
		sk = qtaguid_find_sk(skb, par);
		got_sock = sk;
		if (sk)
			atomic64_inc(&qtu_events.match_found_sk_in_ct);
		else
			atomic64_inc(&qtu_events.match_found_no_sk_in_ct);
	} else {
		atomic64_inc(&qtu_events.match_found_sk);
	}

	if (sk == NULL || sk->sk_socket == NULL) {
		atomic64_inc(&qtu_events.match_no_sk);
		goto put_sock_ret_res;
	}

	if (info->match & info->invert & XT_OWNER_SOCKET)
		/*
		 * Socket exists but user wanted ! --socket-exists.
		 * (Single ampersands intended.)
		 */
		return false;

	filp = sk->sk_socket->file;
	if (filp == NULL)
		return ((info->match ^ info->invert) &
			(XT_OWNER_UID | XT_OWNER_GID)) == 0;

	if (info->match & XT_OWNER_UID)
		if ((filp->f_cred->fsuid >= info->uid_min &&
			filp->f_cred->fsuid <= info->uid_max) ^
			!(info->invert & XT_OWNER_UID))
			return false;

	if (info->match & XT_OWNER_GID)
		if ((filp->f_cred->fsgid >= info->gid_min &&
			filp->f_cred->fsgid <= info->gid_max) ^
			!(info->invert & XT_OWNER_GID))
			return false;

	return true;

put_sock_ret_res:
	if (got_sock) xt_socket_put_sk(sk);
	return res;
}
/*****************************************************************************/

#ifdef DDEBUG
/* This function is not in xt_qtaguid_print.c because of locks visibility */
static void prdebug_full_state(int indent_level, const char *fmt, ...)
{
	va_list args;
	char *fmt_buff;
	char *buff;

	if (!unlikely(qtaguid_debug_mask & DDEBUG_MASK))
		return;

	fmt_buff = kasprintf(GFP_ATOMIC,
				TAG": %s(): %s {\n", __func__, fmt);
	BUG_ON(!fmt_buff);
	va_start(args, fmt);
	buff = kvasprintf(GFP_ATOMIC, fmt_buff, args);
	BUG_ON(!buff);
	pr_debug("%s", buff);
	kfree(fmt_buff);
	kfree(buff);
	va_end(args);

	spin_lock_bh(&sock_tag_list_lock);
	prdebug_sock_tag_tree(indent_level, &sock_tag_tree);
	spin_unlock_bh(&sock_tag_list_lock);

	spin_lock_bh(&sock_tag_list_lock);
	spin_lock_bh(&uid_tag_data_tree_lock);
	prdebug_uid_tag_data_tree(indent_level, &uid_tag_data_tree);
	prdebug_proc_qtu_data_tree(indent_level, &proc_qtu_data_tree);
	spin_unlock_bh(&uid_tag_data_tree_lock);
	spin_unlock_bh(&sock_tag_list_lock);

	spin_lock_bh(&iface_stat_list_lock);
	prdebug_iface_stat_list(indent_level, &iface_stat_list);
	spin_unlock_bh(&iface_stat_list_lock);

	pr_debug(TAG": %s(): }\n", __func__);
}
#else
static void prdebug_full_state(int indent_level, const char *fmt, ...) {}
#endif

struct qta_proc_print_info {
	char *outp;
	char **num_items_returned;
	struct iface_stat *iface_entry;
	struct tag_stat *ts_entry;
	int item_index;
	int items_to_skip;
	int char_count;
};

static int qta_pp_stats_line(struct qta_proc_print_info *ppi, int cnt_set)
{
	int len;
	if (!ppi->item_index) {
		if (ppi->item_index++ < ppi->items_to_skip)
			return 0;
		/*
		len = snprintf(ppi->outp, ppi->char_count,
		       "idx iface acct_tag_hex uid_tag_int cnt_set "
		       "rx_bytes rx_packets "
		       "tx_bytes tx_packets "
		       "rx_tcp_packets rx_tcp_bytes "
		       "rx_udp_packets rx_udp_bytes "
		       "rx_other_packets rx_other_bytes "
		       "tx_tcp_packets tx_tcp_bytes "
		       "tx_udp_packets tx_udp_bytes "
		       "tx_other_packets tx_other_bytes\n");
		*/
		len = snprintf(ppi->outp, ppi->char_count, "0\n");
	} else {
		//...
		if (ppi->item_index++ < ppi->items_to_skip)
			return 0;
		len = snprintf(ppi->outp, ppi->char_count, "0\n");
	}
/*		len = snprintf(
			ppi->outp, ppi->char_count,
			"%d %s 0x%llx %u %u "
			"%llu %llu "
			"%llu %llu "
			"%llu %llu "
			"%llu %llu "
			"%llu %llu "
			"%llu %llu "
			"%llu %llu "
			"%llu %llu\n",
			ppi->item_index,
			ppi->iface_entry->ifname,
			get_atag_from_tag(tag),
			stat_uid,
			cnt_set,
			dc_sum_bytes(cnts, cnt_set, IFS_RX),
			dc_sum_packets(cnts, cnt_set, IFS_RX),
			dc_sum_bytes(cnts, cnt_set, IFS_TX),
			dc_sum_packets(cnts, cnt_set, IFS_TX),
			cnts->bpc[cnt_set][IFS_RX][IFS_TCP].bytes,
			cnts->bpc[cnt_set][IFS_RX][IFS_TCP].packets,
			cnts->bpc[cnt_set][IFS_RX][IFS_UDP].bytes,
			cnts->bpc[cnt_set][IFS_RX][IFS_UDP].packets,
			cnts->bpc[cnt_set][IFS_RX][IFS_PROTO_OTHER].bytes,
			cnts->bpc[cnt_set][IFS_RX][IFS_PROTO_OTHER].packets,
			cnts->bpc[cnt_set][IFS_TX][IFS_TCP].bytes,
			cnts->bpc[cnt_set][IFS_TX][IFS_TCP].packets,
			cnts->bpc[cnt_set][IFS_TX][IFS_UDP].bytes,
			cnts->bpc[cnt_set][IFS_TX][IFS_UDP].packets,
			cnts->bpc[cnt_set][IFS_TX][IFS_PROTO_OTHER].bytes,
			cnts->bpc[cnt_set][IFS_TX][IFS_PROTO_OTHER].packets);
*/
	return len;
}

bool qta_pp_sets(struct qta_proc_print_info *ppi)
{
	int len, counter_set;
	for (counter_set = 0; counter_set < IFS_MAX_COUNTER_SETS; counter_set++) {
		len = qta_pp_stats_line(ppi, counter_set);
		if (len >= ppi->char_count) {
			*ppi->outp = '\0';
			return false;
		}
		if (len) {
			ppi->outp += len;
			ppi->char_count -= len;
			(*ppi->num_items_returned)++;
		}
	}
	return true;
}

/* debug packet match activity */
static int proc_counter_read(char *page, char **num_items_returned,
	off_t offset, int count, int *eof, void *data)
{
	int ret = 0;
	if (offset > 0) return 0;

	ret = scnprintf(page, count, "0x%llx\n", atomic64_read(&qtu_events.match_calls));
	return ret;
}

static int proc_stats_read(char *page, char **num_items_returned,
	off_t items_to_skip, int char_count, int *eof, void *data)
{
	int len;
	struct qta_proc_print_info ppi;

	if (unlikely(module_passive)) {
		*eof = 1;
		return 0;
	}

	ppi.outp = page;
	ppi.item_index = 0;
	ppi.char_count = char_count;
	ppi.num_items_returned = num_items_returned;
	ppi.items_to_skip = items_to_skip;

#if (DEBUG_LOGS)
	pr_info(TAG": proc stats page=%p *num_items_returned=%p off=%ld "
		"char_count=%d *eof=%d\n", page, *num_items_returned,
		items_to_skip, char_count, *eof);
#endif
	if (*eof)
		return 0;

	/* The idx is there to help debug when things go belly up. */
	len = qta_pp_stats_line(&ppi, 0);
	/* Don't advance the outp unless the whole line was printed */
	if (len >= ppi.char_count) {
		*ppi.outp = '\0';
		return ppi.outp - page;
	}
	if (len) {
		ppi.outp += len;
		ppi.char_count -= len;
		(*num_items_returned)++;
	}

	*eof = 1;
	return ppi.outp - page;
}

/*****************************************************************************/

static int ctrl_cmd_counter_set(const char *input)
{
	int res = 0, argc;

	char cmd;
	int counter_set;
	uid_t uid = 0;

	tag_t tag;
	struct tag_counter_set *tcs;

	argc = sscanf(input, "%c %d %u", &cmd, &counter_set, &uid);
	if (argc != 3) {
		res = -EINVAL;
		goto err;
	}
	tag = make_tag_from_uid(uid);

	spin_lock_bh(&tag_counter_set_list_lock);
	tcs = tag_counter_set_tree_search(&tag_counter_set_tree, tag);
	if (!tcs) {
		tcs = kzalloc(sizeof(*tcs), GFP_ATOMIC);
		if (!tcs) {
			spin_unlock_bh(&tag_counter_set_list_lock);
			pr_err(TAG ": ctrl_counterset(%s): "
				"failed to alloc counter set\n",
				input);
			res = -ENOMEM;
			goto err;
		}
		tcs->tn.tag = tag;
		tag_counter_set_tree_insert(tcs, &tag_counter_set_tree);
		pr_info(TAG ": tag_counter allocated uid=%u set=%d\n",
			get_uid_from_tag(tag), counter_set);
	}
	tcs->active_set = counter_set;
	spin_unlock_bh(&tag_counter_set_list_lock);
	atomic64_inc(&qtu_events.counter_set_changes);
err:
	return res;
}

static int ctrl_cmd_delete(const char *input)
{
	int res = 0, argc;

	char cmd;
	tag_t acct_tag;
	tag_t tag;
	uid_t uid = 0;
	struct tag_counter_set *tcs_entry;

	argc = sscanf(input, "%c %llu %u", &cmd, &acct_tag, &uid);
	if (argc < 2) {
		res = -EINVAL;
		goto err;
	}

#if (DEBUG_LOGS)
	pr_info(TAG": ctrl_delete(%s): argc=%d cmd=%c "
		"user_tag=0x%llx uid=%u\n",
		input, argc, cmd, acct_tag, uid);
#endif

	if (!valid_atag(acct_tag)) {
		pr_err(TAG": ctrl_delete(%s): invalid tag\n", input);
		res = -EINVAL;
		goto err;
	}
	if (argc < 3) {
		uid = current_fsuid();
	} else if (!can_impersonate_uid(uid)) {
		pr_info(TAG": ctrl_delete(%s): "
			"insufficient priv from pid=%u tgid=%u uid=%u\n",
			input, current->pid, current->tgid, current_fsuid());
		res = -EPERM;
		goto err;
	}

	tag = combine_atag_with_uid(acct_tag, uid);

	/* Counter sets are only on the app uid tag, not a full tag */
	spin_lock_bh(&tag_counter_set_list_lock);
	tcs_entry = tag_counter_set_tree_search(&tag_counter_set_tree, tag);
	if (tcs_entry) {
		pr_info(TAG": ctrl_delete(%s): "
			"erase tcs: tag=0x%llx (uid=%u) set=%d\n",
			input,
			tcs_entry->tn.tag,
			get_uid_from_tag(tcs_entry->tn.tag),
			tcs_entry->active_set);
		rb_erase(&tcs_entry->tn.node, &tag_counter_set_tree);
		kfree(tcs_entry);
	}
	spin_unlock_bh(&tag_counter_set_list_lock);
	atomic64_inc(&qtu_events.delete_cmds);
err:
	return res;
}

static int ctrl_cmd_tag(const char *input)
{
#if (DEBUG_LOGS)
	pr_warning(TAG":cmd_tag stub (%s)\n", input);
#endif
	atomic64_inc(&qtu_events.sockets_tagged);
	return 0;
}

static int ctrl_cmd_untag(const char *input)
{
#if (DEBUG_LOGS)
	pr_warning(TAG":cmd_untag stub (%s)\n", input);
#endif
	atomic64_inc(&qtu_events.sockets_untagged);
	return 0;
}

/*****************************************************************************/

static int qta_ctrl_parse(const char *input, int count)
{
	int res=0;
	char cmd = input[0];

	/* Collect params for commands */
	switch (cmd) {
	case 'd':
		res = ctrl_cmd_delete(input);
		break;
	case 's':
		res = ctrl_cmd_counter_set(input);
		break;
	case 't':
		res = ctrl_cmd_tag(input);
		break;
	case 'u':
		res = ctrl_cmd_untag(input);
		break;

	default:
		res = -EINVAL;
		goto err;
	}
	if (!res)
		res = count;

	return res;
err:
	pr_err(TAG": ctrl(%s): res=%d\n", input, res);
	return res;
}

#define MAX_QTAGUID_CTRL_INPUT_LEN 255
static int proc_ctrl_write(struct file *filp, const char __user *buffer, unsigned long count, void *data) {

	char input_buf[MAX_QTAGUID_CTRL_INPUT_LEN];

	if (count >= MAX_QTAGUID_CTRL_INPUT_LEN)
		return -EINVAL;

	if (copy_from_user(input_buf, buffer, count))
		return -EFAULT;

	input_buf[count] = '\0';
	return qta_ctrl_parse(input_buf, count);
}

static int qtaguid_ctrl_proc_read(char *page, char **num_items_returned,
	off_t items_to_skip, int char_count, int *eof, void *data)
{
	char *outp = page;
	int len;
	uid_t uid;
	struct rb_node *node;
	struct sock_tag *sock_tag_entry;
	int item_index = 0;
	int indent_level = 0;
	long f_count;

	if (unlikely(module_passive)) {
		*eof = 1;
		return 0;
	}
	if (*eof) return 0;

	spin_lock_bh(&sock_tag_list_lock);
	for (node = rb_first(&sock_tag_tree); node; node = rb_next(node)) {

		if (item_index++ < items_to_skip)
			continue;

		sock_tag_entry = rb_entry(node, struct sock_tag, sock_node);
		uid = get_uid_from_tag(sock_tag_entry->tag);
		f_count = atomic_long_read(&sock_tag_entry->socket->file->f_count);

		len = snprintf(outp, char_count,
			"sock=%p tag=0x%llx (uid=%u) pid=%u "
			"f_count=%lu\n",
			sock_tag_entry->sk,
			sock_tag_entry->tag, uid,
			sock_tag_entry->pid, f_count);

		if (len >= char_count) {
			spin_unlock_bh(&sock_tag_list_lock);
			*outp = '\0';
			return outp - page;
		}
		outp += len;
		char_count -= len;
		(*num_items_returned)++;
	}
	spin_unlock_bh(&sock_tag_list_lock);

	if (item_index++ >= items_to_skip) {
		len = snprintf(outp, char_count,
			       "events: sockets_tagged=%llu "
			       "sockets_untagged=%llu "
			       "counter_set_changes=%llu "
			       "delete_cmds=%llu "
			       "iface_events=%llu "
			       "match_calls=%llu "
			       "match_found_sk=%llu "
			       "match_found_sk_in_ct=%llu "
			       "match_found_no_sk_in_ct=%llu "
			       "match_no_sk=%llu "
			       "match_no_sk_file=%llu\n",
			       atomic64_read(&qtu_events.sockets_tagged),
			       atomic64_read(&qtu_events.sockets_untagged),
			       atomic64_read(&qtu_events.counter_set_changes),
			       atomic64_read(&qtu_events.delete_cmds),
			       atomic64_read(&qtu_events.iface_events),
			       atomic64_read(&qtu_events.match_calls),
			       atomic64_read(&qtu_events.match_found_sk),
			       atomic64_read(&qtu_events.match_found_sk_in_ct),
			       atomic64_read(
				       &qtu_events.match_found_no_sk_in_ct),
			       atomic64_read(&qtu_events.match_no_sk),
			       atomic64_read(&qtu_events.match_no_sk_file));
		if (len >= char_count) {
			*outp = '\0';
			return outp - page;
		}
		outp += len;
		char_count -= len;
		(*num_items_returned)++;
	}

	/* Count the following as part of the last item_index */
	if (item_index > items_to_skip) {
		prdebug_full_state(indent_level, "proc ctrl");
	}

	*eof = 1;
	return outp - page;
}

static struct xt_match qta_owner2_reg __read_mostly = {
	.name       = "owner",
	.revision   = 1,
	.family     = NFPROTO_UNSPEC,
	.match      = qta_owner2_mt,
	.matchsize  = sizeof(struct xt_owner_match_info),
	.hooks      = (1 << NF_INET_LOCAL_IN) |
	              (1 << NF_INET_LOCAL_OUT) |
	              (1 << NF_INET_PRE_ROUTING),
	.me         = THIS_MODULE,
};

static int __init qta_owner2_init(void)
{
	pr_notice("Loading xt_qtaguid light for Motorola Defy");
	proc_root = proc_mkdir(QTA_PROC_DIR, init_net.proc_net);
	if (proc_root) {
		struct proc_dir_entry *proc_entry;

		proc_entry = create_proc_read_entry("ctrl", 0662, proc_root, qtaguid_ctrl_proc_read, NULL);
		proc_entry->write_proc = proc_ctrl_write;
		proc_entry = create_proc_read_entry("stats", 0444, proc_root, proc_stats_read, NULL);

		/* to debug xt filter */
		proc_entry = create_proc_read_entry("counter", 0444, proc_root, proc_counter_read, NULL);
	}
	else {
		pr_err(TAG ": unable to create /proc/self/net/" QTA_PROC_DIR);
		return -1;
	}
	return xt_register_match(&qta_owner2_reg);
}

static void __exit qta_owner2_exit(void)
{
	if (proc_root) {
		remove_proc_entry("ctrl", proc_root);
		remove_proc_entry("stats", proc_root);
		remove_proc_entry("counter", proc_root);
		remove_proc_entry(QTA_PROC_DIR, init_net.proc_net);
	}
	xt_unregister_match(&qta_owner2_reg);
}

module_init(qta_owner2_init);
module_exit(qta_owner2_exit); /* Note: Android will prevent module unload */

MODULE_AUTHOR("Tanguy Pruvot, CyanogenDefy");
MODULE_DESCRIPTION("Xtables: socket owner match for android usage stats");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.6");
MODULE_ALIAS("ipt_owner");
MODULE_ALIAS("ip6t_owner");
MODULE_ALIAS("ipt_qtaguid");
MODULE_ALIAS("ip6t_qtaguid");
