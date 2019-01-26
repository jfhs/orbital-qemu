/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vm_map.h	8.9 (Berkeley) 5/17/95
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * $FreeBSD$
 */

/*
 *	Virtual memory map module definitions.
 */
#ifndef	_VM_MAP_
#define	_VM_MAP_

// #include <sys/lock.h>
#include "freebsd/sys/vm/pmap.h"
#include "freebsd/sys/sys/_sx.h"
#include "freebsd/sys/vm/vm.h"
#include "freebsd/sys/sys/_mutex.h"

/*
 *	Types defined:
 *
 *	vm_map_t		the high-level address map data structure.
 *	vm_map_entry_t		an entry in an address map.
 */

typedef u_char vm_flags_t;
typedef u_int vm_eflags_t;

/*
 *	Objects which live in maps may be either VM objects, or
 *	another map (called a "sharing map") which denotes read-write
 *	sharing with other maps.
 */
union vm_map_object {
	struct vm_object *vm_object;	/* object object */
	struct vm_map *sub_map;		/* belongs to another map */
};

/*
 *	Address map entries consist of start and end addresses,
 *	a VM object (or sharing map) and offset into that object,
 *	and user-exported inheritance and protection information.
 *	Also included is control information for virtual copy operations.
 */
struct vm_map_entry {
	struct vm_map_entry *prev;	/* previous entry */
	struct vm_map_entry *next;	/* next entry */
	struct vm_map_entry *left;	/* left child in binary search tree */
	struct vm_map_entry *right;	/* right child in binary search tree */
	vm_offset_t start;		/* start address */
	vm_offset_t end;		/* end address */
	vm_offset_t avail_ssize;	/* amt can grow if this is a stack */
	vm_size_t adj_free;		/* amount of adjacent free space */
	vm_size_t max_free;		/* max free space in subtree */
	union vm_map_object object;	/* object I point to */
	vm_ooffset_t offset;		/* offset into object */
	vm_eflags_t eflags;		/* map entry flags */
	vm_prot_t protection;		/* protection code */
	vm_prot_t max_protection;	/* maximum protection */
	vm_inherit_t inheritance;	/* inheritance */
	int wired_count;		/* can be paged if = 0 */
	vm_pindex_t lastr;		/* last read */
	struct ucred *cred;		/* tmp storage for creator ref */
};

#define MAP_ENTRY_NOSYNC		0x0001
#define MAP_ENTRY_IS_SUB_MAP		0x0002
#define MAP_ENTRY_COW			0x0004
#define MAP_ENTRY_NEEDS_COPY		0x0008
#define MAP_ENTRY_NOFAULT		0x0010
#define MAP_ENTRY_USER_WIRED		0x0020

#define MAP_ENTRY_BEHAV_NORMAL		0x0000	/* default behavior */
#define MAP_ENTRY_BEHAV_SEQUENTIAL	0x0040	/* expect sequential access */
#define MAP_ENTRY_BEHAV_RANDOM		0x0080	/* expect random access */
#define MAP_ENTRY_BEHAV_RESERVED	0x00C0	/* future use */

#define MAP_ENTRY_BEHAV_MASK		0x00C0

#define MAP_ENTRY_IN_TRANSITION		0x0100	/* entry being changed */
#define MAP_ENTRY_NEEDS_WAKEUP		0x0200	/* waiters in transition */
#define MAP_ENTRY_NOCOREDUMP		0x0400	/* don't include in a core */

#define	MAP_ENTRY_GROWS_DOWN		0x1000	/* Top-down stacks */
#define	MAP_ENTRY_GROWS_UP		0x2000	/* Bottom-up stacks */

#define	MAP_ENTRY_WIRE_SKIPPED		0x4000

#ifdef	_KERNEL
static __inline u_char
vm_map_entry_behavior(vm_map_entry_t entry)
{
	return (entry->eflags & MAP_ENTRY_BEHAV_MASK);
}

static __inline int
vm_map_entry_user_wired_count(vm_map_entry_t entry)
{
	if (entry->eflags & MAP_ENTRY_USER_WIRED)
		return (1);
	return (0);
}

static __inline int
vm_map_entry_system_wired_count(vm_map_entry_t entry)
{
	return (entry->wired_count - vm_map_entry_user_wired_count(entry));
}
#endif	/* _KERNEL */

/*
 *	A map is a set of map entries.  These map entries are
 *	organized both as a binary search tree and as a doubly-linked
 *	list.  Both structures are ordered based upon the start and
 *	end addresses contained within each map entry.  Sleator and
 *	Tarjan's top-down splay algorithm is employed to control
 *	height imbalance in the binary search tree.
 *
 * List of locks
 *	(c)	const until freed
 */
struct vm_map {
	struct vm_map_entry header;	/* List of entries */
	struct sx lock;			/* Lock for map data */
	struct mtx system_mtx;
	int nentries;			/* Number of entries */
	vm_size_t size;			/* virtual size */
	u_int timestamp;		/* Version number */
	u_char needs_wakeup;
	u_char system_map;		/* (c) Am I a system map? */
	vm_flags_t flags;		/* flags for this vm_map */
	vm_map_entry_t root;		/* Root of a binary search tree */
	pmap_t pmap;			/* (c) Physical map */
#define	min_offset	header.start	/* (c) */
#define	max_offset	header.end	/* (c) */
	int busy;
};

/*
 * vm_flags_t values
 */
#define MAP_WIREFUTURE		0x01	/* wire all future pages */
#define	MAP_BUSY_WAKEUP		0x02

/*
 * Shareable process virtual address space.
 *
 * List of locks
 *	(c)	const until freed
 */
struct vmspace {
	struct vm_map vm_map;	/* VM address map */
	struct shmmap_state *vm_shm;	/* SYS5 shared memory private data XXX */
	segsz_t vm_swrss;	/* resident set size before last swap */
	segsz_t vm_tsize;	/* text size (pages) XXX */
	segsz_t vm_dsize;	/* data size (pages) XXX */
	segsz_t vm_ssize;	/* stack size (pages) */
	caddr_t vm_taddr;	/* (c) user virtual address of text */
	caddr_t vm_daddr;	/* (c) user virtual address of data */
	caddr_t vm_maxsaddr;	/* user VA at max stack growth */
	volatile int vm_refcnt;	/* number of references */
	/*
	 * Keep the PMAP last, so that CPU-specific variations of that
	 * structure on a single architecture don't result in offset
	 * variations of the machine-independent fields in the vmspace.
	 */
	struct pmap vm_pmap;	/* private physical map */
};

/* XXX: number of kernel maps and entries to statically allocate */
#define MAX_KMAP	10
#define	MAX_KMAPENT	128

/*
 * Copy-on-write flags for vm_map operations
 */
#define MAP_UNUSED_01		0x0001
#define MAP_COPY_ON_WRITE	0x0002
#define MAP_NOFAULT		0x0004
#define MAP_PREFAULT		0x0008
#define MAP_PREFAULT_PARTIAL	0x0010
#define MAP_DISABLE_SYNCER	0x0020
#define MAP_DISABLE_COREDUMP	0x0100
#define MAP_PREFAULT_MADVISE	0x0200	/* from (user) madvise request */
#define	MAP_STACK_GROWS_DOWN	0x1000
#define	MAP_STACK_GROWS_UP	0x2000
#define	MAP_ACC_CHARGED		0x4000
#define	MAP_ACC_NO_CHARGE	0x8000

/*
 * vm_fault option flags
 */
#define VM_FAULT_NORMAL 0		/* Nothing special */
#define VM_FAULT_CHANGE_WIRING 1	/* Change the wiring as appropriate */
#define	VM_FAULT_DIRTY 2		/* Dirty the page; use w/VM_PROT_COPY */

/*
 * The following "find_space" options are supported by vm_map_find()
 */
#define	VMFS_NO_SPACE		0	/* don't find; use the given range */
#define	VMFS_ANY_SPACE		1	/* find a range with any alignment */
#define	VMFS_ALIGNED_SPACE	2	/* find a superpage-aligned range */
#if defined(__mips__)
#define	VMFS_TLB_ALIGNED_SPACE	3	/* find a TLB entry aligned range */
#endif

/*
 * vm_map_wire and vm_map_unwire option flags
 */
#define VM_MAP_WIRE_SYSTEM	0	/* wiring in a kernel map */
#define VM_MAP_WIRE_USER	1	/* wiring in a user map */

#define VM_MAP_WIRE_NOHOLES	0	/* region must not have holes */
#define VM_MAP_WIRE_HOLESOK	2	/* region may have holes */

#define VM_MAP_WIRE_WRITE	4	/* Validate writable. */

#endif				/* _VM_MAP_ */
