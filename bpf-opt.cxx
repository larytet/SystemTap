// bpf translation pass
// Copyright (C) 2016 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "config.h"
#include "staptree.h"
#include "session.h"

#include <iostream>
#include <cassert>
#include "bpf-internal.h"
#include "bpf-bitset.h"

namespace bpf {

static void
fixup_operands(program &p)
{
  const unsigned nblocks = p.blocks.size();

  for (unsigned i = 0; i < nblocks; ++i)
    {
      block *b = p.blocks[i];

      for (insn *j = b->first; j != NULL; j = j->next)
	{
	  // Any plain move is already handled.
	  if (j->is_move())
	    continue;

	  // The second source cannot handle 64-bit integers.
	  value *s1 = j->src1;
	  if (s1 && s1->is_imm() && s1->imm() != (int32_t)s1->imm())
	    {
	      value *n = p.new_reg();
	      insn_before_inserter ins(b, j);
	      p.mk_mov(ins, n, s1);
	      j->src1 = s1 = n;
	    }

	  if (value *s0 = j->src0)
	    {
	      if (value *d = j->dest)
		{
		  // Binary operators must have dest == src0.
		  if (d == s0)
		    ;
		  else if (d == s1)
		    {
		      if (j->is_commutative())
			{
			  j->src0 = s1;
			  j->src1 = s0;
			}
		      else
			{
			  // Special care for x = y - x
			  value *n = p.new_reg();
			  {
			    insn_before_inserter ins(b, j);
			    p.mk_mov(ins, n, s0);
			  }
			  j->src0 = n;
			  j->dest = n;
			  {
			    insn_after_inserter ins(b, j);
			    p.mk_mov(ins, d, n);
			  }
			}
		    }
		  else
		    {
		      // Transform { x = y - z } to { x = y; x -= z; }
		      insn_before_inserter ins(b, j);
		      p.mk_mov(ins, d, s0);
		      j->src0 = d;
		    }
		}
	      else if (s0->is_imm())
		{
		  // Comparisons can't have src0 constant.
		  value *n = p.new_reg();
		  insn_before_inserter ins(b, j);
		  p.mk_mov(ins, n, s0);
		  j->src0 = n;
		}
	    }
	}
    }
}

static void
thread_jumps(program &p)
{
  const unsigned nblocks = p.blocks.size ();
  std::vector<block *> fwds(nblocks);

  // Identify blocks that do nothing except jump to another block.
  for (unsigned i = 0; i < nblocks; ++i)
    fwds[i] = p.blocks[i]->is_forwarder ();

  // Propagate chains of forwarder blocks.
  {
    bool changed;
    do
      {
	changed = false;
	for (unsigned i = 0; i < nblocks; ++i)
	  if (block *fi = fwds[i])
	    {
	      unsigned j = fi->id;
	      if (block *fj = fwds[j])
		{
		  if (i != j)
		    {
		      fwds[i] = fj;
		      changed = true;
		    }
		}
	    }
      }
    while (changed);
  }

  // Perform jump threading.
  for (unsigned i = 0; i < nblocks; ++i)
    {
      block *b = p.blocks[i];

      if (edge *e = b->taken)
	{
	  if (block *n = fwds[e->next->id])
	    e->redirect_next (n);
	}
      if (edge *e = b->fallthru)
	{
	  if (block *n = fwds[e->next->id])
	    e->redirect_next (n);
	}
    }
}

static void
fold_jumps(program &p)
{
  const unsigned nblocks = p.blocks.size ();

  for (unsigned i = 0; i < nblocks; ++i)
    {
      block *b = p.blocks[i];

      if (b->taken
	  && b->fallthru
	  && b->taken->next == b->fallthru->next)
	{
	  insn *l = b->last;
	  assert (BPF_CLASS (l->code) == BPF_JMP);
	  l->code = BPF_JMP | BPF_JA;
	  delete b->fallthru;
	}
    }
}

static void
reorder_blocks(program &p)
{
  unsigned nblocks = p.blocks.size ();
  std::vector<bool> visited(nblocks);
  std::vector<block *> ordered;
  std::vector<block *> worklist;

  // Begin with the entry block.
  worklist.push_back(p.blocks[0]);

  // Iterate until all blocks placed.
  while (!worklist.empty())
    {
      block *b = worklist.back ();
      worklist.pop_back ();

      // Don't place a block twice, we're not duplicating paths.
      if (visited[b->id])
	continue;

      // Place this block now.
      ordered.push_back (b);
      visited[b->id] = true;

      edge *t = b->taken;
      edge *f = b->fallthru;

      // Look for an IF-THEN triangle where the IF condition might
      // do well to be reversed.  We could find larger subgraphs with
      // postdominators, but since we can't reverse all jumps, it's
      // probably not worth it.  Also look for cases where the taken
      // edge has not been placed, but the fallthru has.
      if (t && f
	  && ((t->next->fallthru && t->next->fallthru->next == f->next)
	      || (visited[f->next->id] && !visited[t->next->id])))
	switch (b->last->code)
	  {
	  case BPF_JMP | BPF_JEQ | BPF_X:
	  case BPF_JMP | BPF_JEQ | BPF_K:
	  case BPF_JMP | BPF_JNE | BPF_X:
	  case BPF_JMP | BPF_JNE | BPF_K:
	    b->last->code ^= BPF_JEQ ^ BPF_JNE;
	    std::swap (t, f);
	    b->taken = t;
	    b->fallthru = f;
	    break;
	  }

      // Plase the two subsequent blocks.
      // Note the LIFO nature of the worklist and place fallthru second.
      if (t)
	{
	  block *o = t->next;
	  if (!visited[o->id])
	    worklist.push_back (o);
	}
      if (f)
	{
	  block *o = f->next;
	  if (visited[o->id])
	    {
	      // The fallthru has been placed.  This means that we
	      // require an extra jump, and possibly a new block in
	      // which to hold it.
	      if (t)
		{
		  block *n = p.new_block ();
		  insn_append_inserter ins(n);
		  p.mk_jmp (ins, o);
		  ordered.push_back (n);
		  f->redirect_next (n);
		}
	      else
		{
		  delete f;
		  insn_after_inserter ins(b, b->last);
		  p.mk_jmp (ins, o);
		}
	    }
	  else
	    worklist.push_back (o);
	}
    }

  // Remove any unreachable blocks.
  for (unsigned i = 0; i < nblocks; ++i)
    if (!visited[i])
      delete p.blocks[i];

  // Renumber the blocks for the new ordering.
  nblocks = ordered.size ();
  for (unsigned i = 0; i < nblocks; ++i)
    {
      block *b = ordered[i];
      b->id = i;
    }

  p.blocks = ordered;
}

struct interference_graph
{
  // ??? Quadratic size for a sparsely populated set.  However, for small
  // sizes (less than hundreds of registers) this is probably more time
  // and space efficient than std::set<std::pair<regno, regno>>.

  bitset::set2 data;

  interference_graph(size_t n) : data(n, n) { }

  bool test(unsigned a, unsigned b) const
  {
    return data[a].test(b);
  }

  void add(unsigned a, unsigned b)
  {
    data[a].set(b);
    data[b].set(a);
  }

  void merge(unsigned a, unsigned b)
  {
    data[a] |= data[b];
    data[b] = data[a];
  }
};

struct copy_graph
{
  struct entry
  {
    unsigned short count;
    regno i, j;

    entry(regno ii, regno jj) : count(0), i(ii), j(jj) { }

    bool operator< (const entry &o) const
    {
      return (count < o.count
	      || (count == o.count
		  && (i < o.i || (i == o.i && j < o.j))));
    }
  };

  std::vector<entry> data;
  std::unordered_map<uint32_t, uint32_t> map;

  void add(regno i, regno j);
  void sort();
};

void
copy_graph::add(regno i, regno j)
{
  if (i == j)
    return;
  if (i > j)
    std::swap(i, j);

  uint32_t ij = (uint32_t)i << 16 | j;
  uint32_t k;
  auto iter = map.find(ij);
  if (iter == map.end())
    {
      k = data.size();
      data.push_back(entry(i, j));
      auto ok = map.insert(std::pair<uint32_t, uint32_t>(ij, k));
      assert(ok.second);
    }
  else
    k = iter->second;

  data[k].count += 1;
}

void
copy_graph::sort()
{
  map.clear();
  std::sort(data.begin(), data.end());
}

struct life_data
{
  bitset::set2 live_in;
  bitset::set2 live_out;
  bitset::set2 used;
  bitset::set2 killed;
  bitset::set1 cross_call;
  std::vector<unsigned short> uses;
  unsigned short npartitions;

  life_data(size_t nblocks, size_t nregs);
};

life_data::life_data(size_t nblocks, size_t nregs)
  : live_in(nblocks, nregs),
    live_out(nblocks, nregs),
    used(nblocks, nregs),
    killed(nblocks, nregs),
    cross_call(nregs),
    uses(nregs)
{ }

static void
find_lifetimes (life_data &d, program &p)
{
  const unsigned nblocks = p.blocks.size();
  const unsigned nregs = p.max_reg();

  // Collect initial lifetime d from the blocks.
  for (unsigned i = 0; i < nblocks; ++i)
    {
      block *b = p.blocks[i];
      bitset::set1_ref killed = d.killed[i];
      bitset::set1_ref used = d.used[i];

      for (insn *j = b->last; j != NULL; j = j->prev)
	{
	  // Every regno that is set in a block is part of killed.
	  j->mark_sets(killed, 1);
	  // Remove sets from used before adding the uses.
	  j->mark_sets(used, 0);
	  j->mark_uses(used, 1);
	}
      d.live_in[i] = used;
    }

  // Propagate lifetime d around blocks.  We could reduce iteration
  // by processing the blocks in post-dominator order.  But the program
  // sizes we must have (because of bpf restrictions) are is too small
  // to worry about more than simple reverse order.
  bool changed;
  bitset::set1 tmp(nregs);
  do
    {
      changed = false;
      for (unsigned i = nblocks; i-- > 0; )
	{
	  block *b = p.blocks[i];

	  if (b->taken)
	    {
	      tmp = d.live_in[b->taken->next->id];
	      if (b->fallthru)
		tmp |= d.live_in[b->fallthru->next->id];
	    }
	  else if (b->fallthru)
	    tmp = d.live_in[b->fallthru->next->id];
	  else
	    tmp.clear();
	  d.live_out[i] = tmp;

	  tmp -= d.killed[i];
	  tmp |= d.used[i];

	  // Note that in order to ensure termination we must accumulate
	  // into live_in rather than assigning to it.
	  if (!tmp.is_subset_of (d.live_in[i]))
	    {
	      changed = true;
	      d.live_in[i] |= tmp;
	    }
	}
    }
  while (changed);
}

static void
find_block_cgraph (copy_graph &cgraph, block *b)
{
  for (insn *j = b->last; j != NULL; j = j->prev)
    {
      if (j->is_move() && j->src1->is_reg())
	cgraph.add(j->dest->reg(), j->src1->reg());
      else if (j->is_binary() && j->src0->is_reg())
	cgraph.add(j->dest->reg(), j->src0->reg());
    }
}

static void
find_cgraph (copy_graph &cgraph, const program &p)
{
  const unsigned nblocks = p.blocks.size();
  for (unsigned i = 0; i < nblocks; ++i)
    find_block_cgraph (cgraph, p.blocks[i]);
}

static void
find_block_uses (std::vector<unsigned short> &uses, block *b)
{
  for (insn *j = b->last; j != NULL; j = j->prev)
    {
      if (j->src0 && j->src0->is_reg())
	++uses[j->src0->reg()];
      if (j->src1 && j->src1->is_reg())
	++uses[j->src1->reg()];
    }
}

static void
find_uses (std::vector<unsigned short> &uses, const program &p)
{
  const unsigned nblocks = p.blocks.size();
  for (unsigned i = 0; i < nblocks; ++i)
    find_block_uses (uses, p.blocks[i]);
}

static void
find_block_igraph (interference_graph &igraph, bitset::set1_ref &cross_call,
		   block *b, bitset::set1_ref &live)
{
  for (insn *j = b->last; j != NULL; j = j->prev)
    {
      // Remove sets from used before adding the uses.
      j->mark_sets(live, 0);
      if (j->is_call())
	cross_call |= live;
      j->mark_uses(live, 1);

      // Record interference between two simultaneously live registers.
      for (size_t r1 = live.find_first();
	   r1 != bitset::set1_ref::npos;
	   r1 = live.find_next (r1))
	for (size_t r2 = live.find_next(r1);
	     r2 != bitset::set1_ref::npos;
	     r2 = live.find_next (r2))
	  igraph.add(r1, r2);
    }
}

static void
find_igraph (interference_graph &igraph, life_data &d, program &p)
{
  const unsigned nblocks = p.blocks.size();
  const unsigned nregs = p.max_reg();
  bitset::set1 tmp(nregs);

  for (unsigned i = 0; i < nblocks; ++i)
    {
      tmp = d.live_out[i];
      find_block_igraph (igraph, d.cross_call, p.blocks[i], tmp);
    }
}

struct pref_sort_reg
{
  const life_data &d;
  pref_sort_reg(const life_data &dd) : d(dd) { }

  bool cmp(regno a, regno b) const;
  bool operator()(const regno &a, const regno &b) const { return cmp(a, b); }
};

bool
pref_sort_reg::cmp(regno a, regno b) const
{
  // Prefer registers that cross calls first.
  int diff = d.cross_call.test(a) - d.cross_call.test(b);
  if (diff != 0)
    return diff > 0;

  // Prefer registers with more uses.
  if (d.uses[a] > d.uses[b])
    return true;

  // Finally, make the sort stable by comparing regnos.
  return a < b;
}

static void
reg_alloc(program &p)
{
  const unsigned nblocks = p.blocks.size();
  const unsigned nregs = p.max_reg();

  life_data life(nblocks, nregs);
  find_lifetimes(life, p);
  find_uses(life.uses, p);

  std::vector<regno> partition(nregs);

  // Initially, all registers are in their own partition.
  for (unsigned i = 0; i < nregs; ++i)
    partition[i] = i;

  // Compute the interference of all registers.
  interference_graph igraph(nregs);
  find_igraph (igraph, life, p);

  // Merge non-conflicting partitions between copies first.
  {
    copy_graph cgraph;
    find_cgraph(cgraph, p);
    cgraph.sort();

    unsigned ncopies = cgraph.data.size();
    for (unsigned i = 0; i < ncopies; ++i)
      {
	const copy_graph::entry &c = cgraph.data[i];
	unsigned r1 = partition[c.i];
	unsigned r2 = c.j;

	if (r2 >= MAX_BPF_REG
	    && partition[r2] == r2
	    && !igraph.test(r1, r2)
	    && (r1 >= BPF_REG_6 || !life.cross_call.test(r2)))
	  {
	    partition[r2] = r1;
	    igraph.merge(r1, r2);
	    life.cross_call[r1] |= life.cross_call[r2];
	  }
      }
  }

  // Merge all other non-conflicting registers next.
  std::vector<regno> ordered(nregs - MAX_BPF_REG);
  for (unsigned i = MAX_BPF_REG; i < nregs; ++i)
    ordered[i - MAX_BPF_REG] = i;

  // ??? Use C++14 lambda.
  pref_sort_reg sort_obj(life);
  std::sort(ordered.begin(), ordered.end(), sort_obj);

  for (unsigned i = MAX_BPF_REG; i < nregs; ++i)
    {
      unsigned r1 = ordered[i - MAX_BPF_REG];
      if (partition[r1] != r1)
	continue;

      for (unsigned j = i + 1; j < nregs; ++j)
	{
	  unsigned r2 = ordered[j - MAX_BPF_REG];
	  if (partition[r2] == r2 && !igraph.test(r1, r2))
	    {
	      partition[r2] = r1;
	      igraph.merge(r1, r2);
	      life.cross_call[r1] |= life.cross_call[r2];
	    }
	}
    }

  // Finally, perform a simplistic register allocation by
  // merging TMPREG partitions with HARDREG "partitions".
  for (unsigned i = MAX_BPF_REG; i < nregs; ++i)
    {
      unsigned r2 = ordered[i - MAX_BPF_REG];

      // Propagate partition info from previous allocations.
      if (partition[r2] != r2)
	continue;

      unsigned first;
      if (life.cross_call.test(r2))
	first = BPF_REG_6;
      else
	first = BPF_REG_0;
      for (unsigned r1 = first; r1 < BPF_REG_10; ++r1)
	{
	  if (!igraph.test(r1, r2))
	    {
	      partition[r2] = r1;
	      igraph.merge(r1, r2);
	      goto done;
	    }
	}

      // ??? We didn't find a register coloring with 10 regs.
      // ??? The proper thing to do is split live ranges, add
      // spill code, and restart the allocation process.
      throw std::runtime_error(_("unable to register allocate"));

    done:
      ;
    }

  // Finalize the allocation by writing back the partition data
  // to the tmpreg value structures.
  for (unsigned i = MAX_BPF_REG; i < nregs; ++i)
    {
      value *v = p.lookup_reg(i);

      // Hard registers are partition[i] == i,
      // and while other partition members should require
      // no more than three dereferences to yeild a hard reg,
      // we allow for up to ten dereferences.
      unsigned r = partition[i];
      for (int j = 0; r >= MAX_BPF_REG && j < 10; j++)
        r = partition[r];

      assert(r < MAX_BPF_REG);
      v->reg_val = r;
    }
}

static void
post_alloc_cleanup (program &p)
{
  const unsigned nblocks = p.blocks.size();
  unsigned id = 0;

  for (unsigned i = 0; i < nblocks; ++i)
    {
      block *b = p.blocks[i];

      for (insn *n, *j = b->first; j != NULL; j = n)
	{
	  n = j->next;
	  if (j->is_move()
	      && j->src1->is_reg()
	      && j->dest->reg() == j->src1->reg() && n)
	    {
	      // Delete no-op moves created by partition merging.
	      insn *p = j->prev;
	      if (p)
		p->next = n;
	      else
		b->first = n;
	      if (n)
		n->prev = p;
	      else
		b->last = p;
	    }
	  else
	    {
	      j->id = id;
	      // 64-bit immediates take two op slots.
	      id += ((j->code & 0xff) == (BPF_LD | BPF_IMM | BPF_DW) ? 2 : 1);
	    }
	}
    }
}

void
program::generate()
{
  fixup_operands(*this);
  thread_jumps(*this);
  fold_jumps(*this);
  reorder_blocks(*this);
  reg_alloc(*this);
  post_alloc_cleanup (*this);
}

} // namespace bpf
