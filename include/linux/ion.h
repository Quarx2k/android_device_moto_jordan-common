/*
 * include/linux/ion.h
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_ION_H
#define _LINUX_ION_H

#include <linux/types.h>

struct ion_handle;
/**
 * enum ion_heap_types - list of all possible types of heaps
 * @ION_HEAP_TYPE_SYSTEM:	 memory allocated via vmalloc
 * @ION_HEAP_TYPE_SYSTEM_CONTIG: memory allocated via kmalloc
 * @ION_HEAP_TYPE_CARVEOUT:	 memory allocated from a prereserved
 * 				 carveout heap, allocations are physically
 * 				 contiguous
 * @ION_HEAP_END:		 helper for iterating over heaps
 */
enum ion_heap_type {
	ION_HEAP_TYPE_SYSTEM,
	ION_HEAP_TYPE_SYSTEM_CONTIG,
	ION_HEAP_TYPE_CARVEOUT,
	ION_HEAP_TYPE_CUSTOM, /* must be last so device specific heaps always
				 are at the end of this enum */
	ION_NUM_HEAPS,
};

#define ION_HEAP_SYSTEM_MASK		(1 << ION_HEAP_TYPE_SYSTEM)
#define ION_HEAP_SYSTEM_CONTIG_MASK	(1 << ION_HEAP_TYPE_SYSTEM_CONTIG)
#define ION_HEAP_CARVEOUT_MASK		(1 << ION_HEAP_TYPE_CARVEOUT)

#ifdef __KERNEL__
struct ion_device;
struct ion_heap;
struct ion_mapper;
struct ion_client;
struct ion_buffer;

/* This should be removed some day when phys_addr_t's are fully
   plumbed in the kernel, and all instances of ion_phys_addr_t should
   be converted to phys_addr_t.  For the time being many kernel interfaces
   do not accept phys_addr_t's that would have to */
#define ion_phys_addr_t unsigned long

/**
 * struct ion_platform_heap - defines a heap in the given platform
 * @type:	type of the heap from ion_heap_type enum
 * @id:		unique identifier for heap.  When allocating (lower numbers 
 * 		will be allocated from first)
 * @name:	used for debug purposes
 * @base:	base address of heap in physical memory if applicable
 * @size:	size of the heap in bytes if applicable
 *
 * Provided by the board file.
 */
struct ion_platform_heap {
	enum ion_heap_type type;
	unsigned int id;
	const char *name;
	ion_phys_addr_t base;
	size_t size;
};

/**
 * struct ion_platform_data - array of platform heaps passed from board file
 * @nr:		number of structures in the array
 * @heaps:	array of platform_heap structions
 *
 * Provided by the board file in the form of platform data to a platform device.
 */
struct ion_platform_data {
	int nr;
	struct ion_platform_heap heaps[];
};

/**
 * ion_client_create() -  allocate a client and returns it
 * @dev:	the global ion device
 * @heap_mask:	mask of heaps this client can allocate from
 * @name:	used for debugging
 */
struct ion_client *ion_client_create(struct ion_device *dev,
				     unsigned int heap_mask, const char *name);

/**
 * ion_client_destroy() -  free's a client and all it's handles
 * @client:	the client
 *
 * Free the provided client and all it's resources including
 * any handles it is holding.
 */
void ion_client_destroy(struct ion_client *client);

/**
 * ion_alloc - allocate ion memory
 * @client:	the client
 * @len:	size of the allocation
 * @align:	requested allocation alignment, lots of hardware blocks have
 *		alignment requirements of some kind
 * @flags:	mask of heaps to allocate from, if multiple bits are set
 *		heaps will be tried in order from lowest to highest order bit
 *
 * Allocate memory in one of the heaps provided in heap mask and return
 * an opaque handle to it.
 */
struct ion_handle *ion_alloc(struct ion_client *client, size_t len,
			     size_t align, unsigned int flags);

/**
 * ion_free - free a handle
 * @client:	the client
 * @handle:	the handle to free
 *
 * Free the provided handle.
 */
void ion_free(struct ion_client *client, struct ion_handle *handle);

/**
 * ion_phys - returns the physical address and len of a handle
 * @client:	the client
 * @handle:	the handle
 * @addr:	a pointer to put the address in
 * @len:	a pointer to put the length in
 *
 * This function queries the heap for a particular handle to get the
 * handle's physical address.  It't output is only correct if
 * a heap returns physically contiguous memory -- in other cases
 * this api should not be implemented -- ion_map_dma should be used
 * instead.  Returns -EINVAL if the handle is invalid.  This has
 * no implications on the reference counting of the handle --
 * the returned value may not be valid if the caller is not
 * holding a reference.
 */
int ion_phys(struct ion_client *client, struct ion_handle *handle,
	     ion_phys_addr_t *addr, size_t *len);

/**
 * ion_map_kernel - create mapping for the given handle
 * @client:	the client
 * @handle:	handle to map
 *
 * Map the given handle into the kernel and return a kernel address that
 * can be used to access this address.
 */
void *ion_map_kernel(struct ion_client *client, struct ion_handle *handle);

/**
 * ion_unmap_kernel() - destroy a kernel mapping for a handle
 * @client:	the client
 * @handle:	handle to unmap
 */
void ion_unmap_kernel(struct ion_client *client, struct ion_handle *handle);

/**
 * ion_map_dma - create a dma mapping for a given handle
 * @client:	the client
 * @handle:	handle to map
 *
 * Return an sglist describing the given handle
 */
struct scatterlist *ion_map_dma(struct ion_client *client,
				struct ion_handle *handle);

/**
 * ion_unmap_dma() - destroy a dma mapping for a handle
 * @client:	the client
 * @handle:	handle to unmap
 */
void ion_unmap_dma(struct ion_client *client, struct ion_handle *handle);

/**
 * ion_share() - given a handle, obtain a buffer to pass to other clients
 * @client:	the client
 * @handle:	the handle to share
 *
 * Given a handle, return a buffer, which exists in a global name
 * space, and can be passed to other clients.  Should be passed into ion_import
 * to obtain a new handle for this buffer.
 *
 * NOTE: This function does do not an extra reference.  The burden is on the
 * caller to make sure the buffer doesn't go away while it's being passed to
 * another client.  That is, ion_free should not be called on this handle until
 * the buffer has been imported into the other client.
 */
struct ion_buffer *ion_share(struct ion_client *client,
			     struct ion_handle *handle);

/**
 * ion_import() - given an buffer in another client, import it
 * @client:	this blocks client
 * @buffer:	the buffer to import (as obtained from ion_share)
 *
 * Given a buffer, add it to the client and return the handle to use to refer
 * to it further.  This is called to share a handle from one kernel client to
 * another.
 */
struct ion_handle *ion_import(struct ion_client *client,
			      struct ion_buffer *buffer);

/**
 * ion_import_fd() - given an fd obtained via ION_IOC_SHARE ioctl, import it
 * @client:	this blocks client
 * @fd:		the fd
 *
 * A helper function for drivers that will be recieving ion buffers shared
 * with them from userspace.  These buffers are represented by a file
 * descriptor obtained as the return from the ION_IOC_SHARE ioctl.
 * This function coverts that fd into the underlying buffer, and returns
 * the handle to use to refer to it further.
 */
struct ion_handle *ion_import_fd(struct ion_client *client, int fd);
#endif /* __KERNEL__ */

/**
 * DOC: Ion Userspace API
 *
 * create a client by opening /dev/ion
 * most operations handled via following ioctls
 *
 */

/**
 * struct ion_allocation_data - metadata passed from userspace for allocations
 * @len:	size of the allocation
 * @align:	required alignment of the allocation
 * @flags:	flags passed to heap
 * @handle:	pointer that will be populated with a cookie to use to refer
 *		to this allocation
 *
 * Provided by userspace as an argument to the ioctl
 */
struct ion_allocation_data {
	size_t len;
	size_t align;
	unsigned int flags;
	struct ion_handle *handle;
};

/**
 * struct ion_fd_data - metadata passed to/from userspace for a handle/fd pair
 * @handle:	a handle
 * @fd:		a file descriptor representing that handle
 * @cacheable:	flag indicate whether buffer needs to be cached or not
 *
 * For ION_IOC_SHARE or ION_IOC_MAP userspace populates the handle field with
 * the handle returned from ion alloc, and the kernel returns the file
 * descriptor to share or map in the fd field.  For ION_IOC_IMPORT, userspace
 * provides the file descriptor and the kernel returns the handle.
 */
struct ion_fd_data {
	struct ion_handle *handle;
	int fd;
	unsigned char cacheable;
};

/**
 * struct ion_handle_data - a handle passed to/from the kernel
 * @handle:	a handle
 */
struct ion_handle_data {
	struct ion_handle *handle;
};

/**
 * struct ion_custom_data - metadata passed to/from userspace for a custom ioctl
 * @cmd:	the custom ioctl function to call
 * @arg:	additional data to pass to the custom ioctl, typically a user
 *		pointer to a predefined structure
 *
 * This works just like the regular cmd and arg fields of an ioctl.
 */
struct ion_custom_data {
	unsigned int cmd;
	unsigned long arg;
};

/**
 * struct ion_map_gralloc_to_ionhandle_data
 * gralloc_handle:	a gralloc handle
 * handleY:			a ion handle mapped to that gralloc handle
 */
struct ion_map_gralloc_to_ionhandle_data {
	void *gralloc_handle;
	struct ion_handle *handleY;
};

/**
 * struct ion_cached_user_buf_data - metadata passed from userspace for
 * flushing or invalidating the ion handle which was mapped cacheable.
 * @handle:	a handle
 * @vaddr: virtual address corresponding to the handle after mapping
 * @size: size of the buffer which should be flushed or invalidated
 *
 * For ION_IOC_FLUSH_CACHED & ION_IOC_INVAL_CACHED, userspace populates
 * the handle field with the ion handle and vaddr with the virtual address
 * corresponding to the handle along with size to be flushed/invalidated.
 */
struct ion_cached_user_buf_data {
	struct ion_handle *handle;
	unsigned long vaddr;
	size_t size;
};

#define ION_IOC_MAGIC		'I'

/**
 * DOC: ION_IOC_ALLOC - allocate memory
 *
 * Takes an ion_allocation_data struct and returns it with the handle field
 * populated with the opaque handle for the allocation.
 */
#define ION_IOC_ALLOC		_IOWR(ION_IOC_MAGIC, 0, \
				      struct ion_allocation_data)

/**
 * DOC: ION_IOC_FREE - free memory
 *
 * Takes an ion_handle_data struct and frees the handle.
 */
#define ION_IOC_FREE		_IOWR(ION_IOC_MAGIC, 1, struct ion_handle_data)

/**
 * DOC: ION_IOC_MAP - get a file descriptor to mmap
 *
 * Takes an ion_fd_data struct with the handle field populated with a valid
 * opaque handle.  Returns the struct with the fd field set to a file
 * descriptor open in the current address space.  This file descriptor
 * can then be used as an argument to mmap.
 */
#define ION_IOC_MAP		_IOWR(ION_IOC_MAGIC, 2, struct ion_fd_data)

/**
 * DOC: ION_IOC_SHARE - creates a file descriptor to use to share an allocation
 *
 * Takes an ion_fd_data struct with the handle field populated with a valid
 * opaque handle.  Returns the struct with the fd field set to a file
 * descriptor open in the current address space.  This file descriptor
 * can then be passed to another process.  The corresponding opaque handle can
 * be retrieved via ION_IOC_IMPORT.
 */
#define ION_IOC_SHARE		_IOWR(ION_IOC_MAGIC, 4, struct ion_fd_data)

/**
 * DOC: ION_IOC_IMPORT - imports a shared file descriptor
 *
 * Takes an ion_fd_data struct with the fd field populated with a valid file
 * descriptor obtained from ION_IOC_SHARE and returns the struct with the handle
 * filed set to the corresponding opaque handle.
 */
#define ION_IOC_IMPORT		_IOWR(ION_IOC_MAGIC, 5, int)

/**
 * DOC: ION_IOC_CUSTOM - call architecture specific ion ioctl
 *
 * Takes the argument of the architecture specific ioctl to call and
 * passes appropriate userdata for that ioctl
 */
#define ION_IOC_CUSTOM		_IOWR(ION_IOC_MAGIC, 6, struct ion_custom_data)

#define ION_IOC_FLUSH_CACHED	_IOWR(ION_IOC_MAGIC, 7, \
					struct ion_cached_user_buf_data)
#define ION_IOC_INVAL_CACHED	_IOWR(ION_IOC_MAGIC, 8, \
					struct ion_cached_user_buf_data)

/**
 * DOC: ION_IOC_MAP_GRALLOC - exports a mapped ion handle
 *
 * Takes an ion_map_gralloc_to_ionhandle_data struct with the gralloc_handle
 * field populated with a valid gralloc handle. Returns the struct with the
 * handleY field set to mapped ion_handle for the corresponding gralloc handle.
 */
#define ION_IOC_MAP_GRALLOC	_IOWR(ION_IOC_MAGIC, 9, \
				struct ion_map_gralloc_to_ionhandle_data)

#endif /* _LINUX_ION_H */
