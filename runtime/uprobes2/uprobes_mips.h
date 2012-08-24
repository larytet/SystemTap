#ifndef _ASM_UPROBES_H
#define _ASM_UPROBES_H
/*
 *  Userspace Probes (UProbes)
 *  uprobes.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) Cisco Systems 2011
 */

#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <asm/thread_info.h>
#include <asm/inst.h>
#include <asm/branch.h>
#include <asm/break.h>

/* Normally defined in Kconfig */
#define CONFIG_URETPROBES 1
#define CONFIG_UPROBES_SSOL 1

#define SSTEP_SIGNAL SIGTRAP
#define BREAKPOINT_SIGNAL SIGTRAP

typedef union mips_instruction uprobe_opcode_t;

static union mips_instruction breakpoint_insn = {
	.b_format = {
		.opcode = spec_op,
		.code = BRK_USERBP,
		.func = break_op
	}
};

#define BREAKPOINT_INSTRUCTION breakpoint_insn
#define IS_BREAKPOINT_INSTRUCTION(opcode) \
	(opcode.word == breakpoint_insn.word)

#define BP_INSN_SIZE 4
#define MAX_UINSN_BYTES 8

struct uprobe_probept_arch_info {
	/*
	 * cache copy of instruction at branch delay slot
	 * for the probes placed on a branch instruction
	 */
	union mips_instruction bd_insn;
};

#define SKIP_DELAYSLOT  	1

struct uprobe_task_arch_info {
	/* Per-thread fields, used while emulating branches */
	unsigned long flags;
 	unsigned long target_epc;
};

struct uprobe_probept;
struct uprobe_task;

int
arch_validate_probed_insn(struct uprobe_probept *ppt,
						struct task_struct *tsk);

unsigned long
arch_hijack_uret_addr(unsigned long trampoline_addr, struct pt_regs *regs,
			struct uprobe_task *utask);

static inline unsigned long
arch_get_probept(struct pt_regs *regs)
{
	return exception_epc(regs);
}

static inline void
arch_reset_ip_for_sstep(struct pt_regs *regs)
{
	/* For SSIL only, which is not supported on MIPS */
}

static inline void
arch_restore_uret_addr(unsigned long ret_addr, struct pt_regs *regs)
{
	regs->regs[31] = ret_addr;
	regs->cp0_epc = ret_addr;
}

static inline unsigned long
arch_get_cur_sp(struct pt_regs *regs)
{
	return (unsigned long) regs->regs[29];
}

static inline unsigned long
arch_predict_sp_at_ret(struct pt_regs *regs, struct task_struct *tsk)
{
	return regs->regs[29];
}


#endif
