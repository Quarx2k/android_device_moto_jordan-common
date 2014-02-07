/* Automatically created during backport process */
#ifndef CPTCFG_BACKPORT_BUILD_DMA_SHARED_BUFFER
#include_next <linux/dma-buf.h>
#else
#undef dma_buf_export_named
#define dma_buf_export_named LINUX_BACKPORT(dma_buf_export_named)
#undef dma_buf_fd
#define dma_buf_fd LINUX_BACKPORT(dma_buf_fd)
#undef dma_buf_get
#define dma_buf_get LINUX_BACKPORT(dma_buf_get)
#undef dma_buf_put
#define dma_buf_put LINUX_BACKPORT(dma_buf_put)
#undef dma_buf_attach
#define dma_buf_attach LINUX_BACKPORT(dma_buf_attach)
#undef dma_buf_detach
#define dma_buf_detach LINUX_BACKPORT(dma_buf_detach)
#undef dma_buf_map_attachment
#define dma_buf_map_attachment LINUX_BACKPORT(dma_buf_map_attachment)
#undef dma_buf_unmap_attachment
#define dma_buf_unmap_attachment LINUX_BACKPORT(dma_buf_unmap_attachment)
#undef dma_buf_begin_cpu_access
#define dma_buf_begin_cpu_access LINUX_BACKPORT(dma_buf_begin_cpu_access)
#undef dma_buf_end_cpu_access
#define dma_buf_end_cpu_access LINUX_BACKPORT(dma_buf_end_cpu_access)
#undef dma_buf_kmap_atomic
#define dma_buf_kmap_atomic LINUX_BACKPORT(dma_buf_kmap_atomic)
#undef dma_buf_kunmap_atomic
#define dma_buf_kunmap_atomic LINUX_BACKPORT(dma_buf_kunmap_atomic)
#undef dma_buf_kmap
#define dma_buf_kmap LINUX_BACKPORT(dma_buf_kmap)
#undef dma_buf_kunmap
#define dma_buf_kunmap LINUX_BACKPORT(dma_buf_kunmap)
#undef dma_buf_mmap
#define dma_buf_mmap LINUX_BACKPORT(dma_buf_mmap)
#undef dma_buf_vmap
#define dma_buf_vmap LINUX_BACKPORT(dma_buf_vmap)
#undef dma_buf_vunmap
#define dma_buf_vunmap LINUX_BACKPORT(dma_buf_vunmap)
#include <linux/backport-dma-buf.h>
#endif /* CPTCFG_BACKPORT_BUILD_DMA_SHARED_BUFFER */
