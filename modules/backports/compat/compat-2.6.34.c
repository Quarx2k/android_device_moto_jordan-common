/*
 * Copyright 2012    Luis R. Rodriguez <mcgrof@do-not-panic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Backport functionality introduced in Linux 2.6.34.
 */

#include <linux/mmc/sdio_func.h>
#include <linux/seq_file.h>
#include "compat-2.6.34.h"

static mmc_pm_flag_t backport_mmc_pm_flags;

void backport_init_mmc_pm_flags(void)
{
	backport_mmc_pm_flags = 0;
}
/*
mmc_pm_flag_t sdio_get_host_pm_caps(struct sdio_func *func)
{
	return backport_mmc_pm_flags;
}
EXPORT_SYMBOL_GPL(sdio_get_host_pm_caps);

int sdio_set_host_pm_flags(struct sdio_func *func, mmc_pm_flag_t flags)
{
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(sdio_set_host_pm_flags);
*/
/**
 * seq_hlist_start - start an iteration of a hlist
 * @head: the head of the hlist
 * @pos:  the start position of the sequence
 *
 * Called at seq_file->op->start().
 */
static struct hlist_node *
seq_hlist_start(struct hlist_head *head, loff_t pos)
{
	struct hlist_node *node;

	hlist_for_each(node, head)
		if (pos-- == 0)
			return node;
	return NULL;
}

/**
 * seq_hlist_start_head - start an iteration of a hlist
 * @head: the head of the hlist
 * @pos:  the start position of the sequence
 *
 * Called at seq_file->op->start(). Call this function if you want to
 * print a header at the top of the output.
 */
struct hlist_node *seq_hlist_start_head(struct hlist_head *head, loff_t pos)
{
	if (!pos)
		return SEQ_START_TOKEN;

	return seq_hlist_start(head, pos - 1);
}
EXPORT_SYMBOL_GPL(seq_hlist_start_head);

/**
 * seq_hlist_next - move to the next position of the hlist
 * @v:    the current iterator
 * @head: the head of the hlist
 * @ppos: the current position
 *
 * Called at seq_file->op->next().
 */
struct hlist_node *seq_hlist_next(void *v, struct hlist_head *head,
				  loff_t *ppos)
{
	struct hlist_node *node = v;

	++*ppos;
	if (v == SEQ_START_TOKEN)
		return head->first;
	else
		return node->next;
}
EXPORT_SYMBOL_GPL(seq_hlist_next);
