#ifndef __BACKPORT_LINUX_TTY_H
#define __BACKPORT_LINUX_TTY_H
#include_next <linux/tty.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39) && \
    LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#define tty_set_termios LINUX_BACKPORT(tty_set_termios)
extern int tty_set_termios(struct tty_struct *tty, struct ktermios *kt);
#endif

/*
 * This really belongs into uapi/asm-generic/termbits.h but
 * that doesn't usually get included directly.
 */
#ifndef EXTPROC
#define EXTPROC	0200000
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#include <linux/smp_lock.h>
static inline void tty_lock(void) __acquires(kernel_lock)
{
#ifdef CONFIG_LOCK_KERNEL
	/* kernel_locked is 1 for !CONFIG_LOCK_KERNEL */
	WARN_ON(kernel_locked());
#endif
	lock_kernel();
}
static inline void tty_unlock(void) __releases(kernel_lock)
{
	unlock_kernel();
}
#define tty_locked()           (kernel_locked())
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
#define n_tty_ioctl_helper LINUX_BACKPORT(n_tty_ioctl_helper)
extern int n_tty_ioctl_helper(struct tty_struct *tty, struct file *file,
		       unsigned int cmd, unsigned long arg);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
/* Backports tty_lock: Localise the lock */
#define tty_lock(__tty) tty_lock()
#define tty_unlock(__tty) tty_unlock()

#define tty_port_register_device(port, driver, index, device) \
	tty_register_device(driver, index, device)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28))
extern void tty_port_tty_wakeup(struct tty_port *port);
extern void tty_port_tty_hangup(struct tty_port *port, bool check_clocal);
#endif

#endif /* __BACKPORT_LINUX_TTY_H */
