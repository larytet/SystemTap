/* -*- linux-c -*- 
 * Map Runtime Functions
 * Copyright (C) 2012-2016 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_MAP_RUNTIME_H_
#define _STAPDYN_MAP_RUNTIME_H_

#include <pthread.h>

#define MAP_GET_CPU()	STAT_GET_CPU()
#define MAP_PUT_CPU()	STAT_PUT_CPU()

struct pmap {
	int bit_shift;    /* scale factor for integer arithmetic */
	int stat_ops;     /* related statistical operators */
	offptr_t oagg;    /* aggregation map */
	offptr_t omap[];  /* per-cpu maps */
};

static inline MAP _stp_pmap_get_agg(PMAP p)
{
	return offptr_get(&p->oagg);
}

static inline void _stp_pmap_set_agg(PMAP p, MAP agg)
{
	offptr_set(&p->oagg, agg);
}

static inline MAP _stp_pmap_get_map(PMAP p, unsigned cpu)
{
	if (cpu >= _stp_runtime_num_contexts)
		cpu = 0;
	return offptr_get(&p->omap[cpu]);
}

static inline void _stp_pmap_set_map(PMAP p, MAP m, unsigned cpu)
{
	if (cpu >= _stp_runtime_num_contexts)
		cpu = 0;
	offptr_set(&p->omap[cpu], m);
}


static void __stp_map_del(MAP map)
{
}


/** Deletes a map.
 * Deletes a map, freeing all memory in all elements.
 * Normally done only when the module exits.
 * @param map
 */

static void _stp_map_del(MAP map)
{
	__stp_map_del(map);
	_stp_shm_free(map);
}

static void _stp_pmap_del(PMAP pmap)
{
	int i;

	/* The pmap is one giant allocation, so do only
	 * the basic cleanup for each map.  */
	for_each_possible_cpu(i)
		__stp_map_del(_stp_pmap_get_map (pmap, i));
	__stp_map_del(_stp_pmap_get_agg(pmap));
	_stp_shm_free(pmap);
}


static int
_stp_map_init(MAP m, unsigned max_entries, unsigned hash_table_mask, int wrap, int node_size)
{
	unsigned i;

	/* The node memory is allocated right after the map (incl. the hash table).  */
	void *node_mem = (void*)(m + 1) + sizeof(struct mhlist_head)*(hash_table_mask+1);

	INIT_MLIST_HEAD(&m->pool);
	INIT_MLIST_HEAD(&m->head);
        m->hash_table_mask = hash_table_mask;
	for (i = 0; i <= m->hash_table_mask; i++)
		INIT_MHLIST_HEAD(&m->hashes[i]);

	m->maxnum = max_entries;
	m->wrap = wrap;

	for (i = 0; i < max_entries; i++) {
		struct map_node *node = node_mem + i * node_size;
		mlist_add(&node->lnode, &m->pool);
		INIT_MHLIST_NODE(&node->hnode);
	}

	return 0;
}


/** Create a new map.
 * Maps must be created at module initialization time.
 * @param max_entries The maximum number of entries allowed. Currently that
 * number will be preallocated.If more entries are required, the oldest ones
 * will be deleted. This makes it effectively a circular buffer.
 * @return A MAP on success or NULL on failure.
 * @ingroup map_create
 */

static MAP
_stp_map_new(unsigned max_entries, int wrap, int node_size,
		int cpu __attribute((unused)))
{
	MAP m;

	/* NB: Allocate the map in one big chuck.
	 * (See _stp_pmap_new for more explanation) */
        unsigned hash_table_mask = HASHTABLESIZE(max_entries)-1; /* usable as bitmask */
	size_t map_size = sizeof(struct map_root)
                + sizeof(struct mhlist_head) * (hash_table_mask+1)
                + node_size * max_entries;
	m = _stp_shm_zalloc(map_size);
	if (m == NULL)
		return NULL;

	if (_stp_map_init(m, max_entries, hash_table_mask, wrap, node_size)) {
		_stp_map_del(m);
		return NULL;
	}
	return m;
}

static PMAP
_stp_pmap_new(unsigned max_entries, int wrap, int node_size)
{
	int i;
	MAP m;
	PMAP pmap;
	void *map_mem;
        unsigned hash_table_mask = HASHTABLESIZE(max_entries)-1; /* usable as bitmask */

	/* Allocate the pmap in one big chuck.
	 *
	 * The reason for this is that we're allocating in the shared memory
	 * mmap, which may have to move locations in order to grow.  If some
	 * smaller unit of the pmap allocation were to cause the whole thing to
	 * move, then we'd lose track of the prior allocations.
	 *
	 * Once returned from here, we'll always access the pmap via the global
	 * shared memory base.  So if other map/pmap/stat/etc. allocations
	 * cause it to move later, that's ok.
	 */

	size_t map_size = sizeof(struct map_root)
                + sizeof(struct mhlist_head) * (hash_table_mask+1)
                + node_size * max_entries;
	size_t pmap_size = sizeof(struct pmap) +
		sizeof(offptr_t) * _stp_runtime_num_contexts;
	size_t total_size = pmap_size +
		map_size * (_stp_runtime_num_contexts + 1);

	map_mem = pmap = _stp_shm_zalloc(total_size);
	if (pmap == NULL)
		return NULL;
	map_mem += pmap_size;

	for_each_possible_cpu(i)
                _stp_pmap_set_map(pmap, NULL, i);
        _stp_pmap_set_agg(pmap, NULL);

	/* Initialize the per-cpu maps.  */
	for_each_possible_cpu(i) {
		m = map_mem;
		if (_stp_map_init(m, max_entries, hash_table_mask, wrap, node_size) != 0)
			goto err;
                _stp_pmap_set_map(pmap, m, i);
		map_mem += map_size;
	}

	/* Initialize the aggregate map.  */
	m = map_mem;
	if (_stp_map_init(m, max_entries, hash_table_mask, wrap, node_size) != 0)
		goto err;
        _stp_pmap_set_agg(pmap, m);

	return pmap;

err:
	_stp_pmap_del(pmap);
	return NULL;
}

#endif /* _STAPDYN_MAP_RUNTIME_H_ */
