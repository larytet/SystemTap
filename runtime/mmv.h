/*
 * Copyright (C) 2014 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _MMV_H_
#define _MMV_H_

typedef enum mmv_stats_flags {
    MMV_FLAG_NOPREFIX	= 0x1,	/* Don't prefix metric names by filename */
    MMV_FLAG_PROCESS	= 0x2,	/* Indicates process check on PID needed */
} mmv_stats_flags_t;

typedef struct {
    char		magic[4];	/* MMV\0 */
    int32_t		version;	/* version */
    uint64_t		g1;		/* Generation numbers */
    uint64_t		g2;
    int32_t		tocs;		/* Number of toc entries */
    mmv_stats_flags_t	flags;
    int32_t		process;	/* client process identifier (flags) */
    int32_t		cluster;	/* preferred PMDA cluster identifier */
} mmv_disk_header_t;

typedef enum {
    MMV_TOC_INDOMS	= 1,	/* mmv_disk_indom_t */
    MMV_TOC_INSTANCES	= 2,	/* mmv_disk_instance_t */
    MMV_TOC_METRICS	= 3,	/* mmv_disk_metric_t */
    MMV_TOC_VALUES	= 4,	/* mmv_disk_value_t */
    MMV_TOC_STRINGS	= 5,	/* mmv_disk_string_t */
} mmv_toc_type_t;

typedef struct {
    mmv_toc_type_t	type;		/* What is it? */
    int32_t		count;		/* Number of entries */
    uint64_t		offset;		/* Offset of section from file start */
} mmv_disk_toc_t;

#define MMV_NAMEMAX	64
#define MMV_STRINGMAX	256

typedef enum mmv_metric_type {
    MMV_TYPE_I64       = 2,	/* 64-bit signed integer */
    MMV_TYPE_STRING    = 6,	/* NULL-terminated string */
} mmv_metric_type_t;

typedef enum mmv_metric_sem {
    MMV_SEM_COUNTER    = 1,	/* cumulative counter (monotonic increasing) */
    MMV_SEM_INSTANT    = 3,	/* instantaneous value, continuous domain */
    MMV_SEM_DISCRETE   = 4,	/* instantaneous value, discrete domain */
} mmv_metric_sem_t;

typedef struct {
    uint64_t		indom;		/* Offset into files indom section */
    uint32_t		padding;	/* zero filled, alignment bits */
    int32_t		internal;	/* Internal instance ID */
    char		external[MMV_NAMEMAX];	/* External instance ID */
} mmv_disk_instances_t;

typedef struct {
    uint32_t		serial;		/* Unique identifier */
    uint32_t		count;		/* Number of instances */
    uint64_t		offset;		/* Offset of first instance */
    uint64_t		shorttext;	/* Offset of short help text string */
    uint64_t		helptext;	/* Offset of long help text string */
} mmv_disk_indom_t;

typedef struct {
#ifdef __BIG_ENDIAN_BITFIELD
    signed int		dimSpace : 4;	/* space dimension */
    signed int		dimTime : 4;	/* time dimension */
    signed int		dimCount : 4;	/* event dimension */
    unsigned int	scaleSpace : 4;	/* one of MMV_SPACE_* below */
    unsigned int	scaleTime : 4;	/* one of MMV_TIME_* below */
    signed int		scaleCount : 4;	/* one of MMV_COUNT_* below */
    unsigned int	pad : 8;
#else
    unsigned int	pad : 8;
    signed int		scaleCount : 4;	/* one of MMV_COUNT_* below */
    unsigned int	scaleTime : 4;	/* one of MMV_TIME_* below */
    unsigned int	scaleSpace : 4;	/* one of MMV_SPACE_* below */
    signed int		dimCount : 4;	/* event dimension */
    signed int		dimTime : 4;	/* time dimension */
    signed int		dimSpace : 4;	/* space dimension */
#endif
} mmv_units_t;			/* dimensional units and scale of value */


/* mmv_units_t.scaleSpace */
#define MMV_SPACE_BYTE	0	/* bytes */
#define MMV_SPACE_KBYTE	1	/* Kilobytes (1024) */
#define MMV_SPACE_MBYTE	2	/* Megabytes (1024^2) */
#define MMV_SPACE_GBYTE	3	/* Gigabytes (1024^3) */
#define MMV_SPACE_TBYTE	4	/* Terabytes (1024^4) */
#define MMV_SPACE_PBYTE	5	/* Petabytes (1024^5) */
#define MMV_SPACE_EBYTE	6	/* Exabytes  (1024^6) */

/* mmv_units_t.scaleTime */
#define MMV_TIME_NSEC	0	/* nanoseconds */
#define MMV_TIME_USEC	1	/* microseconds */
#define MMV_TIME_MSEC	2	/* milliseconds */
#define MMV_TIME_SEC	3	/* seconds */
#define MMV_TIME_MIN	4	/* minutes */
#define MMV_TIME_HOUR	5	/* hours */

/*
 * mmv_units_t.scaleCount (e.g. count events, syscalls, interrupts, etc.)
 * -- these are simply powers of 10, and not enumerated here,
 *    e.g. 6 for 10^6, or -3 for 10^-3
 */
#define MMV_COUNT_ONE	0	/* 1 */

typedef struct {
    uint64_t		indom;		/* Offset into files indom section */
    uint32_t		padding;	/* zero filled, alignment bits */
    int32_t		internal;	/* Internal instance ID */
    char		external[MMV_NAMEMAX];	/* External instance ID */
} mmv_disk_instance_t;

typedef struct {
    char		payload[MMV_STRINGMAX];	/* NULL terminated string */
} mmv_disk_string_t;

typedef struct {
    char		name[MMV_NAMEMAX];
    uint32_t		item;		/* Unique identifier */
    mmv_metric_type_t	type;
    mmv_metric_sem_t	semantics;
    mmv_units_t		dimension;
    uint32_t		indom;		/* Indom serial */
    uint32_t		padding;	/* zero filled, alignment bits */
    uint64_t		shorttext;	/* Offset of short help text string */
    uint64_t		helptext;	/* Offset of long help text string */
} mmv_disk_metric_t;

/* Generic Union for Value-Type conversions */
typedef union {
    int64_t		ll;	/* 64-bit signed */
    char		*cp;	/* char ptr */
} mmv_value_t;

typedef struct {
    mmv_value_t		value;		/* Union of all possible value types */
    int64_t		extra;		/* INTEGRAL(starttime)/STRING(offset) */
    uint64_t		metric;		/* Offset into the metric section */
    uint64_t		instance;	/* Offset into the instance section */
} mmv_disk_value_t;

#endif /* _MMV_H_ */
