/*
 * klogger: A kernel logging subsystem addon for adb
 *
 * v2.0: also allow to use a bigger buffer for 'main' log
 *
 * Copyright (C) 2007-2008 Google, Inc.
 * 2012, Tanguy Pruvot, CyanogenDefy <tpruvot@github>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/time.h>
#include <linux/console.h>
#include <linux/slab.h>

#include "logger.h"

#include <asm/ioctls.h>

#define TAG "klogger"

#ifndef DEFYPLUS
#define RESIZE_LOG /* not required on Defy+ */
#endif

#define KLOG_MAIN_KB   512
#define KLOG_KERNEL_KB 128

#include "hook.h"

#ifdef RESIZE_LOG
static bool hooked = false;
#endif

/*
 * struct logger_log - represents a specific log, such as 'main' or 'radio'
 *
 * This structure lives from module insertion until module removal, so it does
 * not need additional reference counting. The structure is protected by the
 * mutex 'mutex'.
 */
struct logger_log {
	unsigned char 		*buffer;/* the ring buffer itself */
	struct miscdevice	misc;	/* misc device representing the log */
	wait_queue_head_t	wq;	/* wait queue for readers */
	struct list_head	readers; /* this log's readers */
	struct mutex		mutex;	/* mutex protecting buffer */
	size_t			w_off;	/* current write head offset */
	size_t			head;	/* new readers start here */
	size_t			size;	/* size of the log */
};

/*
 * struct logger_reader - a logging device open for reading
 *
 * This object lives from open to release, so we don't need additional
 * reference counting. The structure is protected by log->mutex.
 */
struct logger_reader {
	struct logger_log	*log;	/* associated log */
	struct list_head	list;	/* entry in logger_log's list */
	size_t			r_off;	/* current read head offset */
};

static void klogger_kernel_write(struct console *co, const char *s, unsigned count);

static struct console loggercons = {
	name:	"logger",
	write:	klogger_kernel_write,
	flags:	CON_ENABLED,
	index:	-1,
};

/* logger_offset - returns index 'n' into the log via (optimized) modulus */
#define logger_offset(n)	((n) & (log->size - 1))

/*
 * file_get_log - Given a file structure, return the associated log
 *
 * This isn't aesthetic. We have several goals:
 *
 * 	1) Need to quickly obtain the associated log during an I/O operation
 * 	2) Readers need to maintain state (logger_reader)
 * 	3) Writers need to be very fast (open() should be a near no-op)
 *
 * In the reader case, we can trivially go file->logger_reader->logger_log.
 * For a writer, we don't want to maintain a logger_reader, so we just go
 * file->logger_log. Thus what file->private_data points at depends on whether
 * or not the file was opened for reading. This function hides that dirtiness.
 */
static inline struct logger_log *file_get_log(struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		return reader->log;
	} else
		return file->private_data;
}

/*
 * get_entry_len - Grabs the length of the payload of the next entry starting
 * from 'off'.
 *
 * Caller needs to hold log->mutex.
 */
static __u32 get_entry_len(struct logger_log *log, size_t off)
{
	__u16 val;

	switch (log->size - off) {
	case 1:
		memcpy(&val, log->buffer + off, 1);
		memcpy(((char *) &val) + 1, log->buffer, 1);
		break;
	default:
		memcpy(&val, log->buffer + off, 2);
	}

	return sizeof(struct logger_entry) + val;
}

/*
 * do_read_log_to_user - reads exactly 'count' bytes from 'log' into the
 * user-space buffer 'buf'. Returns 'count' on success.
 *
 * Caller must hold log->mutex.
 */
static ssize_t do_read_log_to_user(struct logger_log *log,
				   struct logger_reader *reader,
				   char __user *buf,
				   size_t count)
{
	size_t len;

	/*
	 * We read from the log in two disjoint operations. First, we read from
	 * the current read head offset up to 'count' bytes or to the end of
	 * the log, whichever comes first.
	 */
	len = min(count, log->size - reader->r_off);
	if (copy_to_user(buf, log->buffer + reader->r_off, len))
		return -EFAULT;

	/*
	 * Second, we read any remaining bytes, starting back at the head of
	 * the log.
	 */
	if (count != len)
		if (copy_to_user(buf + len, log->buffer, count - len))
			return -EFAULT;

	reader->r_off = logger_offset(reader->r_off + count);

	return count;
}

/*
 * klogger_read - our log's read() method
 *
 * Behavior:
 *
 * 	- O_NONBLOCK works
 * 	- If there are no log entries to read, blocks until log is written to
 * 	- Atomically reads exactly one log entry
 *
 * Optimal read size is LOGGER_ENTRY_MAX_LEN. Will set errno to EINVAL if read
 * buffer is insufficient to hold next entry.
 */
static ssize_t klogger_read(struct file *file, char __user *buf,
			    size_t count, loff_t *pos)
{
	struct logger_reader *reader = file->private_data;
	struct logger_log *log = reader->log;
	ssize_t ret;
	DEFINE_WAIT(wait);

start:
	while (1) {
		prepare_to_wait(&log->wq, &wait, TASK_INTERRUPTIBLE);

		mutex_lock(&log->mutex);
		ret = (log->w_off == reader->r_off);
		mutex_unlock(&log->mutex);
		if (!ret)
			break;

		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}

		schedule();
	}

	finish_wait(&log->wq, &wait);
	if (ret)
		return ret;

	mutex_lock(&log->mutex);

	/* is there still something to read or did we race? */
	if (unlikely(log->w_off == reader->r_off)) {
		mutex_unlock(&log->mutex);
		goto start;
	}

	/* get the size of the next entry */
	ret = get_entry_len(log, reader->r_off);
	if (count < ret) {
		ret = -EINVAL;
		goto out;
	}

	/* get exactly one entry from the log */
	ret = do_read_log_to_user(log, reader, buf, ret);

out:
	mutex_unlock(&log->mutex);

	return ret;
}

/*
 * get_next_entry - return the offset of the first valid entry at least 'len'
 * bytes after 'off'.
 *
 * Caller must hold log->mutex.
 */
static size_t get_next_entry(struct logger_log *log, size_t off, size_t len)
{
	size_t count = 0;

	do {
		size_t nr = get_entry_len(log, off);
		off = logger_offset(off + nr);
		count += nr;
	} while (count < len);

	return off;
}

/*
 * clock_interval - is a < c < b in mod-space? Put another way, does the line
 * from a to b cross c?
 */
static inline int clock_interval(size_t a, size_t b, size_t c)
{
	if (b < a) {
		if (a < c || b >= c)
			return 1;
	} else {
		if (a < c && b >= c)
			return 1;
	}
	return 0;
}

/*
 * fix_up_readers - walk the list of all readers and "fix up" any who were
 * lapped by the writer; also do the same for the default "start head".
 * We do this by "pulling forward" the readers and start head to the first
 * entry after the new write head.
 *
 * The caller needs to hold log->mutex.
 */
static void fix_up_readers(struct logger_log *log, size_t len)
{
	size_t old = log->w_off;
	size_t new = logger_offset(old + len);
	struct logger_reader *reader;

	if (clock_interval(old, new, log->head))
		log->head = get_next_entry(log, log->head, len);

	list_for_each_entry(reader, &log->readers, list)
		if (clock_interval(old, new, reader->r_off))
			reader->r_off = get_next_entry(log, reader->r_off, len);
}

/*
 * do_write_log - writes 'len' bytes from 'buf' to 'log'
 *
 * The caller needs to hold log->mutex.
 */
static void do_write_log(struct logger_log *log, const void *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	memcpy(log->buffer + log->w_off, buf, len);

	if (count != len)
		memcpy(log->buffer, buf + len, count - len);

	log->w_off = logger_offset(log->w_off + count);

}

/*
 * do_write_log_user - writes 'len' bytes from the user-space buffer 'buf' to
 * the log 'log'
 *
 * The caller needs to hold log->mutex.
 *
 * Returns 'count' on success, negative error code on failure.
 */
static ssize_t do_write_log_from_user(struct logger_log *log,
				      const void __user *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	if (len && copy_from_user(log->buffer + log->w_off, buf, len))
		return -EFAULT;

	if (count != len)
		if (copy_from_user(log->buffer, buf + len, count - len))
			return -EFAULT;

	log->w_off = logger_offset(log->w_off + count);

	return count;
}

/*
 * klogger_aio_write - our write method, implementing support for write(),
 * writev(), and aio_write(). Writes are our fast path, and we try to optimize
 * them above all else.
 */
ssize_t klogger_aio_write(struct kiocb *iocb, const struct iovec *iov,
			 unsigned long nr_segs, loff_t ppos)
{
	struct logger_log *log = file_get_log(iocb->ki_filp);
	size_t orig = log->w_off;
	struct logger_entry header;
	struct timespec now;
	ssize_t ret = 0;

	now = current_kernel_time();

	header.pid = current->tgid;
	header.tid = current->pid;
	header.sec = now.tv_sec;
	header.nsec = now.tv_nsec;
	header.len = min_t(size_t, iocb->ki_left, LOGGER_ENTRY_MAX_PAYLOAD);

	/* null writes succeed, return zero */
	if (unlikely(!header.len))
		return 0;

	mutex_lock(&log->mutex);

	/*
	 * Fix up any readers, pulling them forward to the first readable
	 * entry after (what will be) the new write offset. We do this now
	 * because if we partially fail, we can end up with clobbered log
	 * entries that encroach on readable buffer.
	 */
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	do_write_log(log, &header, sizeof(struct logger_entry));

	while (nr_segs-- > 0) {
		size_t len;
		ssize_t nr;

		/* figure out how much of this vector we can keep */
		len = min_t(size_t, iov->iov_len, header.len - ret);

		/* write out this segment's payload */
		nr = do_write_log_from_user(log, iov->iov_base, len);
		if (unlikely(nr < 0)) {
			log->w_off = orig;
			mutex_unlock(&log->mutex);
			return nr;
		}

		iov++;
		ret += nr;
	}

	mutex_unlock(&log->mutex);

	/* wake up any blocked readers */
	wake_up_interruptible(&log->wq);

	return ret;
}

static struct logger_log *get_log_from_minor(int);

/*
 * klogger_open - the log's open() file operation
 *
 * Note how near a no-op this is in the write-only case. Keep it that way!
 */
static int klogger_open(struct inode *inode, struct file *file)
{
	struct logger_log *log;
	int ret;

	ret = nonseekable_open(inode, file);
	if (ret)
		return ret;

	log = get_log_from_minor(MINOR(inode->i_rdev));
	if (!log)
		return -ENODEV;

	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader;

		reader = kmalloc(sizeof(struct logger_reader), GFP_KERNEL);
		if (!reader)
			return -ENOMEM;

		reader->log = log;
		INIT_LIST_HEAD(&reader->list);

		mutex_lock(&log->mutex);
		reader->r_off = log->head;
		list_add_tail(&reader->list, &log->readers);
		mutex_unlock(&log->mutex);

		file->private_data = reader;
	} else
		file->private_data = log;

	return 0;
}

/*
 * klogger_release - the log's release file operation
 *
 * Note this is a total no-op in the write-only case. Keep it that way!
 */
static int klogger_release(struct inode *ignored, struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		struct logger_log *log = reader->log;
		mutex_lock(&log->mutex);
		list_del(&reader->list);
		mutex_unlock(&log->mutex);
		kfree(reader);
	}

	return 0;
}

/*
 * klogger_poll - the log's poll file operation, for poll/select/epoll
 *
 * Note we always return POLLOUT, because you can always write() to the log.
 * Note also that, strictly speaking, a return value of POLLIN does not
 * guarantee that the log is readable without blocking, as there is a small
 * chance that the writer can lap the reader in the interim between poll()
 * returning and the read() request.
 */
static unsigned int klogger_poll(struct file *file, poll_table *wait)
{
	struct logger_reader *reader;
	struct logger_log *log;
	unsigned int ret = POLLOUT | POLLWRNORM;

	if (!(file->f_mode & FMODE_READ))
		return ret;

	reader = file->private_data;
	log = reader->log;

	poll_wait(file, &log->wq, wait);

	mutex_lock(&log->mutex);
	if (log->w_off != reader->r_off)
		ret |= POLLIN | POLLRDNORM;
	mutex_unlock(&log->mutex);

	return ret;
}

static long klogger_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct logger_log *log = file_get_log(file);
	struct logger_reader *reader;
	long ret = -ENOTTY;

	mutex_lock(&log->mutex);

	switch (cmd) {
	case LOGGER_GET_LOG_BUF_SIZE:
		ret = log->size;
		break;
	case LOGGER_GET_LOG_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		if (log->w_off >= reader->r_off)
			ret = log->w_off - reader->r_off;
		else
			ret = (log->size - reader->r_off) + log->w_off;
		break;
	case LOGGER_GET_NEXT_ENTRY_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		if (log->w_off != reader->r_off)
			ret = get_entry_len(log, reader->r_off);
		else
			ret = 0;
		break;
	case LOGGER_FLUSH_LOG:
		if (!(file->f_mode & FMODE_WRITE)) {
			ret = -EBADF;
			break;
		}
		list_for_each_entry(reader, &log->readers, list)
			reader->r_off = log->w_off;
		log->head = log->w_off;
		ret = 0;
		break;
	}

	mutex_unlock(&log->mutex);

	return ret;
}

#ifdef RESIZE_LOG
static unsigned char _buf_log_big[KLOG_MAIN_KB*1024];
static bool resized = false;
static struct logger_log *main_log = NULL;
static struct logger_log prev_state;
#endif

static unsigned char _buf_log_kernel[KLOG_KERNEL_KB*1024];

static struct file_operations kernel_logger_fops = {
	.owner = THIS_MODULE,
	.read = klogger_read,
	.poll = klogger_poll,
	.unlocked_ioctl = klogger_ioctl,
	.compat_ioctl = klogger_ioctl,
	.open = klogger_open,
	.release = klogger_release,
};

static struct logger_log log_kernel = {
	.buffer = _buf_log_kernel,
	.misc ={
		.minor = MISC_DYNAMIC_MINOR,
		.name = "log_kernel",
		.fops = &kernel_logger_fops,
		.parent = NULL,
	},
	.wq = __WAIT_QUEUE_HEAD_INITIALIZER(log_kernel.wq),
	.readers = LIST_HEAD_INIT(log_kernel.readers),
	.mutex = __MUTEX_INITIALIZER(log_kernel.mutex),
	.w_off = 0,
	.head = 0,
	.size = sizeof(_buf_log_kernel),
};

static void klogger_kernel_write(struct console *co, const char *s, unsigned count)
{
	struct logger_entry header;
	struct timespec now;
	struct logger_log *log= &log_kernel;
	unsigned int msg_len;
	int prio=3;
	struct iovec vec[4];
	struct iovec *iov;
	int vec_count=3;
	ssize_t ret = 1;
	const char tag[7] = "kernel\0";
	iov=&vec[0];

#ifdef RESIZE_LOG
	/* remove hook if it is not more required */
	if (hooked && resized) {
		hook_exit();
		hooked = false;
	}
#endif

	/* since s is a pointer to LOG_BUF and we know LOG_BUF is continuous,
	 * s[-3]='<', s[-2] is the log level and s[-1]='>'.
	 * If not, we set loglevel as default value 0
	 */
	switch (s[-2]) {
		case '0': prio=3;break;
		case '1': prio=3;break;
		case '2': prio=3;break;
		case '3': prio=6;break;
		case '4': prio=5;break;
		case '5': prio=4;break;
		case '6': prio=4;break;
		case '7': prio=3;break;
		default:  prio=3;
	}
	vec[0].iov_base = (unsigned char *) &prio;
	vec[0].iov_len  = 1;
	vec[1].iov_base = (void *)tag;
	vec[1].iov_len = strlen(tag) + 1;
	vec[2].iov_base = (void *)s;
	vec[2].iov_len  = count;
	vec[3].iov_base = NULL;
	vec[3].iov_len  = 0;

	now = current_kernel_time();

	header.pid = 0;
	header.tid = 0;
	header.sec = now.tv_sec;
	header.nsec= now.tv_nsec;

	msg_len = vec[0].iov_len + vec[1].iov_len + vec[2].iov_len;
	header.len = min_t(size_t, msg_len, LOGGER_ENTRY_MAX_PAYLOAD - 1);
	do_write_log(log, &header, sizeof(struct logger_entry));
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	while (vec_count-- > 0) {
		size_t len;

		/* figure out how much of this vector we can keep */
		len = min_t(size_t, iov->iov_len, header.len - ret);

		/* write out this segment's payload */
		do_write_log(log, iov->iov_base, len);

		iov++;
	}

	/* wake up any blocked readers */
	wake_up_interruptible(&log->wq);

	return ;
}

static struct logger_log *get_log_from_minor(int minor)
{
	if (log_kernel.misc.minor == minor)
		return &log_kernel;
	return NULL;
}

/* hooked function */
#ifdef RESIZE_LOG
ssize_t logger_aio_write(struct kiocb *iocb, const struct iovec *iov,
			 unsigned long nr_segs, loff_t ppos)
{
	struct logger_log *log = file_get_log(iocb->ki_filp);
	struct miscdevice* dev = &log->misc;

	ssize_t ret = HOOK_INVOKE(logger_aio_write, iocb, iov, nr_segs, ppos);

	mutex_lock(&log->mutex);
	if (log->size != sizeof(_buf_log_big) && strcmp(dev->name, "log_main") == 0) {

		memcpy(&prev_state, log, sizeof(prev_state));
		main_log = log;

		memcpy(_buf_log_big, log->buffer, log->w_off);

		log->buffer = _buf_log_big;
		log->size = sizeof(_buf_log_big);
		resized = true;

		pr_info(TAG ": buffer size of %s increased from %uK to %uK\n",
		        dev->name, (unsigned) prev_state.size >> 10,
		        (unsigned) sizeof(_buf_log_big) >> 10);
	}
	mutex_unlock(&log->mutex);

	return ret;
}
#endif /* RESIZE_LOG */

struct hook_info g_hi[] = {
#ifdef RESIZE_LOG
	HOOK_INIT(logger_aio_write),
#endif
	HOOK_INIT_END
};

static int __init init_log(struct logger_log *log)
{
	int ret = misc_register(&log->misc);
	if (unlikely(ret)) {
		pr_err(TAG ": failed to register misc "
		      "device for log '%s'!\n", log->misc.name);
		return ret;
	}

	pr_info(TAG ": created %uK buffer for %s\n",
	       (unsigned) log->size >> 10, log->misc.name);

	return 0;
}

static int __init klogger_init(void)
{
	int ret;
#ifdef RESIZE_LOG
	memset(&prev_state, 0, sizeof(prev_state));
	hooked = (hook_init() == 0);
#endif
	ret = init_log(&log_kernel);
	if (unlikely(ret))
		goto out;
	register_console(&loggercons);
out:
	return ret;
}

static void __exit klogger_exit(void)
{
#ifdef RESIZE_LOG
	if (hooked) {
		hook_exit();
		hooked = false;
	}
	if (resized) {
		mutex_lock(&main_log->mutex);
		main_log->size = prev_state.size;
		main_log->buffer = prev_state.buffer;
		main_log->w_off = prev_state.w_off;
		mutex_unlock(&main_log->mutex);
		pr_info(TAG ": buffer size restored to %uK\n",
			(unsigned) prev_state.size >> 10);
	}
	main_log = NULL;
#endif
	unregister_console(&loggercons);
	misc_deregister(&log_kernel.misc);
}

module_init(klogger_init);
module_exit(klogger_exit);

MODULE_ALIAS(TAG);
MODULE_DESCRIPTION("Enhanced adb logger");
MODULE_AUTHOR("Tanguy Pruvot, CyanogenDefy");
MODULE_VERSION("2.3");
MODULE_LICENSE("GPL");
