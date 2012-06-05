/*
 * 2.6.32.9 Kernel compat
 *
 * Goal: emulate or replace these missing functions :
 *  sk = xt_socket_get4_sk(skb);
 *  sk = xt_socket_get6_sk(skb);
 *  xt_socket_put_sk(sk);
 *
 */
#include "atomic.h"

#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/inetdevice.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_socket.h>
#include <linux/skbuff.h>
#include <net/addrconf.h>
#include <net/sock.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <net/inet_sock.h>
#include <net/inet_hashtables.h>
#include <net/inet6_hashtables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include "nf_tproxy_core.h"


#include "xt_qtaguid_print.h"
#include "xt_qtaguid_internal.h"

#define TAG "qtaguid" /* after internal.h to get missing debug symbol */

/*
 * We only use the xt_socket funcs within a similar context to avoid unexpected
 * return values.
 */
#define XT_SOCKET_SUPPORTED_HOOKS \
	((1 << NF_INET_PRE_ROUTING) | (1 << NF_INET_LOCAL_IN))

#ifndef pr_warn_once
#define pr_warn_once(fmt, ...) \
        printk_once(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#endif

/*
 * find the offset to specified header or the protocol number of last header
 * if target < 0. "last header" is transport protocol header, ESP, or
 * "No next header".
 *
 * If target header is found, its offset is set in *offset and return protocol
 * number. Otherwise, return -1.
 *
 * If the first fragment doesn't contain the final protocol header or
 * NEXTHDR_NONE it is considered invalid.
 *
 * Note that non-1st fragment is special case that "the protocol number
 * of last header" is "next header" field in Fragment header. In this case,
 * *offset is meaningless and fragment offset is stored in *fragoff if fragoff
 * isn't NULL.
 *
 */
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
int qta_ipv6_find_hdr(const struct sk_buff *skb, unsigned int *offset,
		  int target, unsigned short *fragoff)
{
	unsigned int start = skb_network_offset(skb) + sizeof(struct ipv6hdr);
	u8 nexthdr = ipv6_hdr(skb)->nexthdr;
	unsigned int len = skb->len - start;

	if (fragoff)
		*fragoff = 0;

	while (nexthdr != target) {
		struct ipv6_opt_hdr _hdr, *hp;
		unsigned int hdrlen;

		if ((!ipv6_ext_hdr(nexthdr)) || nexthdr == NEXTHDR_NONE) {
			if (target < 0)
				break;
			return -ENOENT;
		}

		hp = skb_header_pointer(skb, start, sizeof(_hdr), &_hdr);
		if (hp == NULL)
			return -EBADMSG;
		if (nexthdr == NEXTHDR_FRAGMENT) {
			unsigned short _frag_off;
			__be16 *fp;
			fp = skb_header_pointer(skb,
						start+offsetof(struct frag_hdr,
							       frag_off),
						sizeof(_frag_off),
						&_frag_off);
			if (fp == NULL)
				return -EBADMSG;

			_frag_off = ntohs(*fp) & ~0x7;
			if (_frag_off) {
				if (target < 0 &&
				    ((!ipv6_ext_hdr(hp->nexthdr)) ||
				     hp->nexthdr == NEXTHDR_NONE)) {
					if (fragoff)
						*fragoff = _frag_off;
					return hp->nexthdr;
				}
				return -ENOENT;
			}
			hdrlen = 8;
		} else if (nexthdr == NEXTHDR_AUTH)
			hdrlen = (hp->hdrlen + 2) << 2;
		else
			hdrlen = ipv6_optlen(hp);

		nexthdr = hp->nexthdr;
		len -= hdrlen;
		start += hdrlen;
	}

	*offset = start;
	return nexthdr;
}
#endif

void qtaguid_put_sk(struct sock *sk) {
	if (sk != NULL) sock_put(sk);
}

struct sock *qtaguid_find_sk(const struct sk_buff *skb, const struct xt_match_param *par)
{
	struct sock *sk = NULL;

	const struct iphdr *iph = NULL;
	struct ipv6hdr *iph6 = NULL;
	struct udphdr _hdr, *hp;
	int tproto = 0, thoff;
	unsigned int hook_type = (1 << par->hooknum);

	MT_DEBUG(TAG": find_sk(skb=%p) hooknum=%d family=%d\n", skb,
		 par->hooknum, par->family);

	/*
	 * Let's not abuse the the xt_socket_get*_sk(), or else it will
	 * return garbage SKs.
	 */
	if (!(hook_type & XT_SOCKET_SUPPORTED_HOOKS))
		return NULL;

	switch (par->family) {
	case NFPROTO_IPV4:
		/* sk = xt_socket_get4_sk(skb, par); */
		iph = ip_hdr(skb);
		if (iph->protocol == IPPROTO_UDP || iph->protocol == IPPROTO_TCP) {
			hp = skb_header_pointer(skb, ip_hdrlen(skb), sizeof(_hdr), &_hdr);
			if (hp) {
				__be32 saddr = iph->saddr, daddr= iph->daddr;
				sk = nf_tproxy_get_sock_v4(dev_net(skb->dev), iph->protocol,
					saddr, daddr, hp->source, hp->dest, par->in, NFT_LOOKUP_ANY);
				MT_DEBUG(TAG": IPv4 packet SPT=%u DPT=%u sk=%p\n",
					ntohs(hp->source), ntohs(hp->dest), sk);
			}
		}
		break;
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	case NFPROTO_IPV6:
		MT_DEBUG(TAG": IPv6 packets not tracked yet\n");
#if 0 // to (deep) check
		/* sk = xt_socket_get6_sk(skb, par); */
		iph6 = ipv6_hdr(skb);
		tproto = qta_ipv6_find_hdr(skb, &thoff, -1, NULL);
		if (tproto == IPPROTO_TCP) {
			hp = skb_header_pointer(skb, thoff, sizeof(_hdr), &_hdr);
			if (hp) {
				sk = nf_tproxy_get_sock_v6(dev_net(skb->dev), tproto,
						&iph6->saddr, &iph6->daddr, hp->source, hp->dest,
						par->in, NFT_LOOKUP_ANY);
				MT_DEBUG(TAG": IPv6 packet SPT=%u DPT=%u sk=%p\n",
					ntohs(hp->source), ntohs(hp->dest), sk);
			}
		} else if (iph->protocol == IPPROTO_UDP) {
			MT_DEBUG(TAG": IPv6 udp packet not supported\n");
		}
#endif
		break;
#endif
	default:
		pr_warn_once(TAG": unknown protocol %u\n", par->family);
	}

	/*
	 * Seems to be issues on the file ptr for TCP_TIME_WAIT SKs.
	 * http://kerneltrap.org/mailarchive/linux-netdev/2010/10/21/6287959
	 * Not fixed in 3.0-r3 :(
	 */
	if (sk) {
		MT_DEBUG(TAG": %p->sk_proto=%u "
			 "->sk_state=%d\n", sk, sk->sk_protocol, sk->sk_state);
		if (sk->sk_state  == TCP_TIME_WAIT) {
			qtaguid_put_sk(sk);
			sk = NULL;
		}
	}
	return sk;
}
