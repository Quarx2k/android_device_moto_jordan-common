#ifndef __BACKPORT_PCMCIA_DS_H
#define __BACKPORT_PCMCIA_DS_H
#include_next <pcmcia/ds.h>

#ifndef module_pcmcia_driver
/**
 * backport of:
 *
 * commit 6ed7ffddcf61f668114edb676417e5fb33773b59
 * Author: H Hartley Sweeten <hsweeten@visionengravers.com>
 * Date:   Wed Mar 6 11:24:44 2013 -0700
 *
 *     pcmcia/ds.h: introduce helper for pcmcia_driver module boilerplate
 */

/**
 * module_pcmcia_driver() - Helper macro for registering a pcmcia driver
 * @__pcmcia_driver: pcmcia_driver struct
 *
 * Helper macro for pcmcia drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only use
 * this macro once, and calling it replaces module_init() and module_exit().
 */
#define module_pcmcia_driver(__pcmcia_driver) \
	module_driver(__pcmcia_driver, pcmcia_register_driver, \
			pcmcia_unregister_driver)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
#ifndef pcmcia_enable_device
#define pcmcia_enable_device(link)	pcmcia_request_configuration(link, &(link)->conf)
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#define pcmcia_read_config_byte LINUX_BACKPORT(pcmcia_read_config_byte)
static inline int pcmcia_read_config_byte(struct pcmcia_device *p_dev, off_t where, u8 *val)
{
        int ret;
        conf_reg_t reg = { 0, CS_READ, where, 0 };
        ret = pcmcia_access_configuration_register(p_dev, &reg);
        *val = reg.Value;
        return ret;
}

#define pcmcia_write_config_byte LINUX_BACKPORT(pcmcia_write_config_byte)
static inline int pcmcia_write_config_byte(struct pcmcia_device *p_dev, off_t where, u8 val)
{
	conf_reg_t reg = { 0, CS_WRITE, where, val };
	return pcmcia_access_configuration_register(p_dev, &reg);
}
#endif
#if 0
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
#define pcmcia_request_window(a, b, c) pcmcia_request_window(&a, b, c)
#define pcmcia_map_mem_page(a, b, c) pcmcia_map_mem_page(b, c)

#define pcmcia_loop_tuple LINUX_BACKPORT(pcmcia_loop_tuple)
int pcmcia_loop_tuple(struct pcmcia_device *p_dev, cisdata_t code,
		      int (*loop_tuple) (struct pcmcia_device *p_dev,
					 tuple_t *tuple,
					 void *priv_data),
		      void *priv_data);

#define pccard_loop_tuple LINUX_BACKPORT(pccard_loop_tuple)
int pccard_loop_tuple(struct pcmcia_socket *s, unsigned int function,
		      cisdata_t code, cisparse_t *parse, void *priv_data,
		      int (*loop_tuple) (tuple_t *tuple,
					 cisparse_t *parse,
					 void *priv_data));
#endif /* < 2.6.33 */
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
#ifdef pcmcia_parse_tuple
#undef pcmcia_parse_tuple
#define pcmcia_parse_tuple(tuple, parse) pccard_parse_tuple(tuple, parse)
#endif

/* From : include/pcmcia/ds.h */
/* loop CIS entries for valid configuration */
#define pcmcia_loop_config LINUX_BACKPORT(pcmcia_loop_config)
int pcmcia_loop_config(struct pcmcia_device *p_dev,
		       int	(*conf_check)	(struct pcmcia_device *p_dev,
						 cistpl_cftable_entry_t *cfg,
						 cistpl_cftable_entry_t *dflt,
						 unsigned int vcc,
						 void *priv_data),
		       void *priv_data);
#endif

#endif /* __BACKPORT_PCMCIA_DS_H */
