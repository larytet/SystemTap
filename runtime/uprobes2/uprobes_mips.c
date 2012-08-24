/*
 *  Userspace Probes (UProbes)
 *  uprobes_mips.c
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

/*
 * insn_has_ll_or_sc function checks whether instruction is ll or sc
 * one; putting breakpoint on top of atomic ll/sc pair is bad idea;
 * so we need to prevent it and refuse uprobes insertion for such
 * instructions; cannot do much about breakpoint in the middle of
 * ll/sc pair.
 */
static int insn_has_ll_or_sc(union mips_instruction insn)
{
	int ret = 0;
	switch (insn.i_format.opcode) {
	case ll_op:
	case lld_op:
	case sc_op:
	case scd_op:
		ret = 1;
		break;
	default:
		break;
	}
	return ret;
}

/*
 * insn_has_delayslot borrowed from MIPS kprobes implementation, since
 * we don't want uprobes have dependencies on kprobes, we have our own
 * copy here. Note it may change with time.
 */
static int insn_has_delayslot(union mips_instruction insn)
{
	switch (insn.i_format.opcode) {

		/*
		 * This group contains:
		 * jr and jalr are in r_format format.
		 */
	case spec_op:
		switch (insn.r_format.func) {
		case jr_op:
		case jalr_op:
			break;
		default:
			goto insn_ok;
		}

		/*
		 * This group contains:
		 * bltz_op, bgez_op, bltzl_op, bgezl_op,
		 * bltzal_op, bgezal_op, bltzall_op, bgezall_op.
		 */
	case bcond_op:

		/*
		 * These are unconditional and in j_format.
		 */
	case jal_op:
	case j_op:

		/*
		 * These are conditional and in i_format.
		 */
	case beq_op:
	case beql_op:
	case bne_op:
	case bnel_op:
	case blez_op:
	case blezl_op:
	case bgtz_op:
	case bgtzl_op:

		/*
		 * These are the FPA/cp1 branch instructions.
		 */
	case cop1_op:

#ifdef CONFIG_CPU_CAVIUM_OCTEON
	case lwc2_op: /* This is bbit0 on Octeon */
	case ldc2_op: /* This is bbit032 on Octeon */
	case swc2_op: /* This is bbit1 on Octeon */
	case sdc2_op: /* This is bbit132 on Octeon */
#endif
		return 1;
	default:
		break;
	}
insn_ok:
	return 0;
}

static int
probe_on_delayslot(struct uprobe_probept *ppt, struct task_struct *tsk )
{
	union mips_instruction prev_insn;
	unsigned long prev_addr;
    	int ret;

	prev_addr = ppt->vaddr - 4;

	ret = access_process_vm(tsk, prev_addr, &prev_insn, BP_INSN_SIZE, 0);
	if (ret != BP_INSN_SIZE)
		return -EIO;

	if (insn_has_delayslot(prev_insn))
		return 1;

	return 0;
}

int
arch_validate_probed_insn(struct uprobe_probept *ppt, struct task_struct *tsk)
{
	int ret;

	if (insn_has_ll_or_sc(ppt->opcode)) {
		pr_notice("Uprobes for ll and sc instructions are not supported\n");
		return -EINVAL;
	}

	if (insn_has_delayslot(ppt->opcode)) {
		unsigned long addr = ppt->vaddr + 4;
		union mips_instruction bd_insn;
                /*
                 * On MIPS arch if the instruction at probed address is a
                 * branch instruction, we need to execute the instruction at
                 * Branch Delayslot (BD) at the time of probe hit. As MIPS also
		 * doesn't have single stepping support, the BD instruction can
		 * not be executed in-line and it would be executed on SSOL slot
		 * using a normal breakpoint instruction in the next slot.
		 * So, read the instruction and save it for later execution.
                 */
		ret = access_process_vm(tsk, addr, &bd_insn, BP_INSN_SIZE, 0);
		if (ret != BP_INSN_SIZE)
			return -EIO;

		if (insn_has_delayslot(bd_insn)) {
			pr_notice("Uprobes error: branch instruction in "
				  "delayslot\n");
			return -EINVAL;
		}

		ppt->arch_info.bd_insn.word = bd_insn.word;
		return 0;
	}

	ret = probe_on_delayslot(ppt, tsk);
	if (ret < 0) {
		pr_notice("Uprobes error: Reading instruction bytes\n");
		return ret;
	}
	if (ret == 1) {
		pr_notice("Uprobes on branch delay slot are not supported\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * Borrowed heavily from arch/mips/kernel/branch.c:__compute_return_epc()
 *
 * Evaluate the branch instruction at probed address during probe hit. The
 * result of evaluation would be the updated epc. The insturction in delayslot
 * would actually be single stepped using a normal breakpoint) on SSOL slot.
 *
 * The result is also saved in the per thread utask structure for later use,
 * in case we need to execute the delayslot instruction. The latter will be
 * false for NOP instruction in dealyslot and the branch-likely instructions
 * when the branch is taken. And for those cases we set a flag as
 * SKIP_DELAYSLOT in the per thread utask structure.
 */
static int
evaluate_branch_instruction(struct pt_regs *regs, struct uprobe_probept *ppt,
 			    struct uprobe_task *utask)
{
	union mips_instruction insn = ppt->opcode;
	unsigned int dspcontrol;
	long epc;
	int ret = 0;

	epc = regs->cp0_epc;
	if (epc & 3)
		goto unaligned;

	if (ppt->arch_info.bd_insn.word == 0)
		utask->arch_info.flags |= SKIP_DELAYSLOT;
	else
		utask->arch_info.flags &= ~SKIP_DELAYSLOT;

	ret = __compute_return_epc_for_insn(regs, insn);

	if (ret < 0)
	      return ret;

	if (ret == BRANCH_LIKELY_TAKEN)
	      utask->arch_info.flags |= SKIP_DELAYSLOT;

	utask->arch_info.target_epc = regs->cp0_epc;

	return 0;

unaligned:
	printk("%s: unaligned epc - sending SIGBUS.\n", current->comm);
	force_sig(SIGBUS, current);
	return -EFAULT;

}

static
int uprobe_emulate_insn(struct pt_regs *regs, struct uprobe_probept *ppt,
						struct uprobe_task *utask)
{
	int ret = 0;

	/* Probe is on NOP, nothing to do, get done with probe handling */
	if (ppt->opcode.word == 0)
		return 1;

	if (insn_has_delayslot(ppt->opcode)) {
		/* Evaluate the branch instruction and the delayslot instruction
		 * would be single stepped using the SSOL SLOT
		 */
		ret = evaluate_branch_instruction(regs, ppt, utask);
		if (ret < 0) {
			pr_notice("Uprobes: Error evaluating branch\n");
			return 0;
		}
		if ((!ret) && (utask->arch_info.flags & SKIP_DELAYSLOT))
			return 1;
	}

	return 0;
}

static void
uprobe_pre_ssout(struct uprobe_task *utask, struct uprobe_probept *ppt,
			struct pt_regs *regs)
{
	uprobe_opcode_t bp_insn = BREAKPOINT_INSTRUCTION;
	uprobe_opcode_t current_insn[2];
	struct uprobe_ssol_slot *slot;
	int ret;

	slot = uprobe_get_insn_slot(ppt);
	if (!slot)
		goto err_exit;

	if (ppt->arch_info.bd_insn.word) {
		/*
		 * For probes at branch instructions, we execute the instruction
		 * at branch delay slot using SSOL, so write the saved BD
		 * instruction on SSOL slot, if it is not done yet.
		 */
		ret = access_process_vm(utask->tsk, (unsigned long)
					&(slot->insn[0]),
					&current_insn[0], 4, 0);
		if (ret != 4) {
			pr_notice("Uprobes error: Reading SSOL slot %d\n", ret);
			goto err_exit;
		}

		if (current_insn[0].word != ppt->arch_info.bd_insn.word) {
			ret = access_process_vm(utask->tsk, (unsigned long)
						&(slot->insn[0]),
						&ppt->arch_info.bd_insn.word,
						4, 1);
			if (ret != 4) {
				pr_notice("Uprobes error: Writing SSOL slot "
					  "%d\n", ret);
				goto err_exit;
			}
		}
	}
	/*
	 * MIPS arch does not have single stepping, so write a breakpoint
	 * instruction for single stepping, if it is not done yet
	 */
	ret = access_process_vm(utask->tsk, (unsigned long)&(slot->insn[1]),
				&current_insn[1], 4, 0);
	if (ret != 4) {
		pr_notice("Uprobes error: Reading SSOL slot, ret %d\n", ret);
		goto err_exit;
	}

	if (!IS_BREAKPOINT_INSTRUCTION(current_insn[1])) {
		ret = access_process_vm(utask->tsk, (unsigned long)
					&(slot->insn[1]), &bp_insn.word, 4, 1);
		if (ret != 4) {
			pr_notice("Uprobes error: 2nd Writing SSOL slot %d\n",
				  ret);
			goto err_exit;
		}
	}

	/* Now point the epc to the SSOL slot for single stepping */
	regs->cp0_epc = (unsigned long) &slot->insn[0];
	return;

err_exit:
	utask->doomed = 1;
	return;
}

static void
uprobe_post_ssout(struct uprobe_task *utask, struct uprobe_probept *ppt,
			struct pt_regs *regs)
{
	unsigned long orig_epc = ppt->vaddr;

	up_read(&ppt->slot->rwsem);

	if (ppt->arch_info.bd_insn.word)
		regs->cp0_epc = utask->arch_info.target_epc;
	else
		regs->cp0_epc = orig_epc + BP_INSN_SIZE;
}

unsigned long
arch_hijack_uret_addr(unsigned long trampoline_addr, struct pt_regs *regs,
			 struct uprobe_task *utask)
{
	unsigned long orig_ret_addr = 0;

	orig_ret_addr = regs->regs[31];

	if (orig_ret_addr == trampoline_addr)
		/*
		 * There's another uretprobe on this function, and it was
		 * processed first, so the return address has already
		 * been hijacked.
		 */
		return orig_ret_addr;

	regs->regs[31] = trampoline_addr;

	return orig_ret_addr;
}
