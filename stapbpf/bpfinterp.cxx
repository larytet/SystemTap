/* bpfinterp.c - SystemTap BPF interpreter
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2016 Red Hat, Inc.
 *
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <vector>
#include <unordered_set>
#include <inttypes.h>
#include <unordered_map>
#include "bpfinterp.h"

#ifdef __OPTIMIZE__
namespace {
#endif

// DERIVED FROM:
// --------------------------------------------------------------------
// lookup2.c, by Bob Jenkins, December 1996, Public Domain.
// hash(), hash2(), hash3, and mix() are externally useful functions.
// Routines to test the hash are included if SELF_TEST is defined.
// You can use this free for any purpose.  It has no warranty.
// --------------------------------------------------------------------
//
// mix -- mix 3 32-bit values reversibly.
// For every delta with one or two bit set, and the deltas of all three
// high bits or all three low bits, whether the original value of a,b,c
// is almost all zero or is uniformly distributed,
// * If mix() is run forward or backward, at least 32 bits in a,b,c
//   have at least 1/4 probability of changing.
// * If mix() is run forward, every bit of c will change between 1/3 and
//   2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
// mix was built out of 36 single-cycle latency instructions in a 
// structure that could supported 2x parallelism, like so:

inline void
iter_hash_mix(uint32_t &a, uint32_t &b, uint32_t &c)
{
  a -= b; a -= c; a ^= c>>13;
  b -= c; b -= a; b ^= a<< 8;
  c -= a; c -= b; c ^= b>>13;
  a -= b; a -= c; a ^= c>>12;
  b -= c; b -= a; b ^= a<<16;
  c -= a; c -= b; c ^= b>> 5;
  a -= b; a -= c; a ^= c>> 3;
  b -= c; b -= a; b ^= a<<10;
  c -= a; c -= b; c ^= b>>15;
}

// load32 -- load an unaligned little-endian 32-bit value

inline uint32_t
load32(const unsigned char *k)
{
  uint32_t r;
  if (__BYTE_ORDER == __LITTLE_ENDIAN)
    memcpy(&r, k, 4);
  else
    {
      r = k[0];
      r |= (uint32_t)k[1] << 8;
      r |= (uint32_t)k[2] << 16;
      r |= (uint32_t)k[3] << 24;
    }
  return r;
}

// hash -- hash a variable-length key into a 32-bit value
//  k     : the key (the unaligned variable-length array of bytes)
//  len   : the length of the key, counting by bytes
//  level : can be any 4-byte value
// Returns a 32-bit value.  Every bit of the key affects every bit of
// the return value.  Every 1-bit and 2-bit delta achieves avalanche.
// About 36+6len instructions.
//
// By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
// code any way you wish, private, educational, or commercial.  It's free.
// 
// See http://burtleburtle.net/bob/hash/evahash.html
// Use for hash table lookup, or anything where one collision in 2^32 is
// acceptable.  Do NOT use for cryptographic purposes.

static uint32_t
iterative_hash (const unsigned char *k,		// the key
                uint32_t length,		// the length of the key
                uint32_t initval = 0)		// the previous hash
{
  uint32_t a, b, c, len;

  // Set up the internal state.
  len = length;
  a = b = 0x9e3779b9;	// the golden ratio; an arbitrary value
  c = initval;		// the previous hash value

  // Handle most of the key.
  while (len >= 12)
    {
      a += load32(k + 0);
      b += load32(k + 4);
      c += load32(k + 8);
      iter_hash_mix(a,b,c);
      k += 12; len -= 12;
    }

  // Handle the last 11 bytes.
  c += length;
  switch (len)
    {
    // the first byte of c is reserved for the length
    case 11:		c += ((uint32_t)k[10] << 24);
    case 10:		c += ((uint32_t)k[9] << 16);
    case 9:		c += ((uint32_t)k[8] << 8);
			goto last_8;

    case 7:		b += ((uint32_t)k[6] << 16);
    case 6:		b += ((uint32_t)k[5] << 8);
    case 5:		b += k[4];
			goto last_4;

    case 3:		a += ((uint32_t)k[2] << 16);
    case 2:		a += ((uint32_t)k[1] << 8);
    case 1:		a += k[0];
    case 0:		break;

    case 8: last_8:	b += load32(k + 4);
    case 4: last_4:	a += load32(k + 0);
			break;
    }
  iter_hash_mix(a,b,c);

  return c;
}
// --------------------------------------------------------------------

// ??? Is there a trivial way to reuse a standard container without
// having to replicate the key/value size data in each element?

struct bpf_map
{
  const uint32_t key_size;
  const uint32_t val_size;
  const uint32_t max_size;

  bpf_map(uint32_t ks, uint32_t vs, uint32_t ms)
    : key_size(ks), val_size(vs), max_size(ms)
  { }
  
  virtual ~bpf_map() { }
  virtual void *lookup(const void *key) = 0;
  virtual int update(const void *key, const void *val, unsigned flags) = 0;
  virtual int remove(const void *key) = 0;
  virtual void sys_export(int fd) = 0;
  virtual void sys_import(int fd) = 0;
};

class bpf_hash_map : public bpf_map
{
  struct elem
  {
    const uint32_t key_size;
    const uint32_t val_size;
    void *key_data;
    void *val_data;

    elem(uint32_t k, uint32_t v)
      : key_size(k), val_size(v), key_data(NULL), val_data(NULL)
    { }

    elem(uint32_t k, uint32_t v, const void *kd, const void *vd)
      : key_size(k), val_size(v)
    {
      alloc_data();
      memcpy(key_data, kd, k);
      memcpy(val_data, vd, v);
    }

    elem &operator=(const elem &) = delete;
    elem(const elem &) = delete;

    elem &operator=(elem &&o)
    {
      key_data = o.key_data;
      val_data = o.val_data;
      o.key_data = NULL;
      o.val_data = NULL;
      return *this;
    }

    elem(elem &&o)
      : key_size(o.key_size), val_size(o.val_size),
	key_data(o.key_data), val_data(o.val_data)
    {
      o.key_data = NULL;
      o.val_data = NULL;
    }
    
    ~elem() { free(key_data); }

    bool operator==(const elem &o) const
    {
      return memcmp(key_data, o.key_data, key_size) == 0;
    }

    size_t hash() const
    {
      return iterative_hash((const unsigned char *)key_data, key_size);
    }

    void alloc_data();
  };

  struct elem_temp : public elem
  {
    elem_temp(uint32_t ks, void *kd) : elem(ks, 0) { key_data = kd; }
    ~elem_temp() { key_data = NULL; }
  };

  struct elem_hash
  {
    size_t operator()(const elem &o) const { return o.hash(); }
  };

  std::unordered_set<elem, elem_hash> map;

public:
  bpf_hash_map(uint32_t ks, uint32_t vs, uint32_t ms)
    : bpf_map(ks, vs, ms)
  { }

  ~bpf_hash_map() { }
  virtual void *lookup(const void *key);
  virtual int update(const void *key, const void *val, unsigned flags);
  virtual int remove(const void *key);
  virtual void sys_export(int fd);
  virtual void sys_import(int fd);
};

void
bpf_hash_map::elem::alloc_data()
{
  size_t key_round = (key_size + 7) & -8;
  void *data = malloc(key_round + val_size);
  if (data == NULL)
    throw std::bad_alloc();

  key_data = data;
  val_data = (void *)((char *)data + key_round);
}

void *
bpf_hash_map::lookup(const void *key)
{
  elem_temp test(key_size, const_cast<void *>(key));
  auto i = map.find(test);

  if (i == map.end())
    return NULL;
  return i->val_data;
}

int
bpf_hash_map::update(const void *key, const void *val, unsigned flags)
{
  if (flags > BPF_EXIST)
    return -EINVAL;

  elem_temp test(key_size, const_cast<void *>(key));
  auto i = map.find(test);

  if (i == map.end())
    {
      if (flags == BPF_EXIST)
	return -ENOENT;
      if (map.size() == max_size)
	return -E2BIG;
      map.emplace(key_size, val_size, key, val);
    }
  else
    {
      if (flags == BPF_NOEXIST)
	return -EEXIST;
      memcpy(i->val_data, val, val_size);
    }
  return 0;
}

int
bpf_hash_map::remove(const void *key)
{
  elem_temp test(key_size, const_cast<void *>(key));
  auto i = map.find(test);

  if (i == map.end())
    return -ENOENT;
  map.erase(i);
  return 0;
}

void
bpf_hash_map::sys_export(int fd)
{
  for (auto i = map.begin(); i != map.end(); ++i)
    bpf_update_elem(fd, i->key_data, i->val_data, BPF_ANY);
}

void
bpf_hash_map::sys_import(int fd)
{
  map.clear();

  char key[key_size], val[val_size];
  memset(key, -1, key_size);

  while (bpf_get_next_key(fd, key, key) >= 0)
    {
      bpf_lookup_elem(fd, key, val);
      update(key, val, 0);
    }
}

struct bpf_array_map : public bpf_map
{
  void *data;

  bpf_array_map(uint32_t vs, uint32_t ms)
    : bpf_map(4, vs, ms)
  {
    data = calloc(ms, vs);
    if (data == NULL)
      throw std::bad_alloc();
  }

  void *val_ptr(uint32_t k) { return (void *)((char *)data + k * val_size); }

  virtual ~bpf_array_map() { free(data); }
  virtual void *lookup(const void *key);
  virtual int update(const void *key, const void *val, unsigned flags);
  virtual int remove(const void *key);
  virtual void sys_export(int fd);
  virtual void sys_import(int fd);
};

void *
bpf_array_map::lookup(const void *key)
{
  uint32_t k = load32((const unsigned char *)key);
  if (k >= max_size)
    return NULL;
  return val_ptr(k);
}

int
bpf_array_map::update(const void *key, const void *val, unsigned flags)
{
  if (flags > BPF_EXIST)
    return -EINVAL;

  uint32_t k = load32((const unsigned char *)key);
  if (k >= max_size)
    return -E2BIG;
  if (flags == BPF_NOEXIST)
    return -EEXIST;

  memcpy(val_ptr(k), val, val_size);
  return 0;
}

int
bpf_array_map::remove(const void *)
{
  return -EINVAL;
}

void
bpf_array_map::sys_export(int fd)
{
  for (uint32_t i = 0; i < max_size; ++i)
    bpf_update_elem(fd, &i, val_ptr(i), 0);
}

void
bpf_array_map::sys_import(int fd)
{
  for (uint32_t i = 0; i < max_size; ++i)
    bpf_lookup_elem(fd, &i, val_ptr(i));
}

#ifdef __OPTIMIZE__
} // anon namespace
#endif

struct bpf_context
{
  std::vector<bpf_map *> maps;

  bpf_context(size_t n) : maps(n) { }
  ~bpf_context();
};

bpf_context::~bpf_context()
{
  for (auto i = maps.begin(); i != maps.end(); ++i)
    delete *i;
}

bpf_context *
bpf_context_create(size_t nmaps, const struct bpf_map_def attrs[])
{
  struct bpf_context *c = new bpf_context(nmaps);
  size_t i;

  for (i = 0; i < nmaps; ++i)
    {
      bpf_map *m;
      switch (attrs[i].type)
	{
	case BPF_MAP_TYPE_HASH:
	  m = new bpf_hash_map(attrs[i].key_size, attrs[i].value_size,
			       attrs[i].max_entries);
	  break;
	case BPF_MAP_TYPE_ARRAY:
	  if (attrs[i].key_size != 4)
	    throw std::invalid_argument("invalid array key size");
	  m = new bpf_array_map(attrs[i].value_size, attrs[i].max_entries);
	  break;
	default:
	  throw std::invalid_argument("unhandled map type");
	}
      c->maps[i] = m;
    }

  return c;
}

void
bpf_context_free(bpf_context *c)
{
  delete c;
}

void
bpf_context_export(bpf_context *c, int fds[])
{
  size_t i, nmaps = c->maps.size();
  for (i = 0; i < nmaps; ++i)
    c->maps[i]->sys_export(fds[i]);
}

void
bpf_context_import(struct bpf_context *c, int fds[])
{
  size_t i, nmaps = c->maps.size();
  for (i = 0; i < nmaps; ++i)
    c->maps[i]->sys_import(fds[i]);
}

inline uintptr_t
as_int(void *ptr)
{
  return reinterpret_cast<uintptr_t>(ptr);
}

inline void *
as_ptr(uintptr_t ptr)
{
  return reinterpret_cast<void *>(ptr);
}

inline bpf_map *
as_map(uintptr_t ptr)
{
  return reinterpret_cast<bpf_map *>(ptr);
}

inline char *
as_str(uintptr_t ptr)
{
  return reinterpret_cast<char *>(ptr);
}

uint64_t
bpf_interpret(bpf_context *c, size_t ninsns, const struct bpf_insn insns[])
{
  uint64_t stack[512 / 8];
  uint64_t regs[MAX_BPF_REG];
  const struct bpf_insn *i = insns;

  regs[BPF_REG_10] = (uintptr_t)stack + sizeof(stack);

  while ((size_t)(i - insns) < ninsns)
    {
      uint64_t dr, sr, si, s1;

      dr = regs[i->dst_reg];
      sr = regs[i->src_reg];
      si = i->imm;
      s1 = i->code & BPF_X ? sr : si;

      switch (i->code)
	{
	case BPF_LDX | BPF_MEM | BPF_B:
	  dr = *(uint8_t *)((uintptr_t)sr + i->off);
	  break;
	case BPF_LDX | BPF_MEM | BPF_H:
	  dr = *(uint16_t *)((uintptr_t)sr + i->off);
	  break;
	case BPF_LDX | BPF_MEM | BPF_W:
	  dr = *(uint32_t *)((uintptr_t)sr + i->off);
	  break;
	case BPF_LDX | BPF_MEM | BPF_DW:
	  dr = *(uint64_t *)((uintptr_t)sr + i->off);
	  break;

	case BPF_ST | BPF_MEM | BPF_B:
	  sr = si;
	case BPF_STX | BPF_MEM | BPF_B:
	  *(uint8_t *)((uintptr_t)dr + i->off) = sr;
	  goto nowrite;
	case BPF_ST | BPF_MEM | BPF_H:
	  sr = si;
	case BPF_STX | BPF_MEM | BPF_H:
	  *(uint16_t *)((uintptr_t)dr + i->off) = sr;
	  goto nowrite;
	case BPF_ST | BPF_MEM | BPF_W:
	  sr = si;
	case BPF_STX | BPF_MEM | BPF_W:
	  *(uint32_t *)((uintptr_t)dr + i->off) = sr;
	  goto nowrite;
	case BPF_ST | BPF_MEM | BPF_DW:
	  sr = si;
	case BPF_STX | BPF_MEM | BPF_DW:
	  *(uint64_t *)((uintptr_t)dr + i->off) = sr;
	  goto nowrite;

	case BPF_ALU64 | BPF_ADD | BPF_X:
	case BPF_ALU64 | BPF_ADD | BPF_K:  dr += s1; break;
	case BPF_ALU64 | BPF_SUB | BPF_X:
	case BPF_ALU64 | BPF_SUB | BPF_K:  dr -= s1; break;
	case BPF_ALU64 | BPF_AND | BPF_X:
	case BPF_ALU64 | BPF_AND | BPF_K:  dr &= s1; break;
	case BPF_ALU64 | BPF_OR  | BPF_X:
	case BPF_ALU64 | BPF_OR  | BPF_K:  dr |= s1; break;
	case BPF_ALU64 | BPF_LSH | BPF_X:
	case BPF_ALU64 | BPF_LSH | BPF_K:  dr <<= s1; break;
	case BPF_ALU64 | BPF_RSH | BPF_X:
	case BPF_ALU64 | BPF_RSH | BPF_K:  dr >>= s1; break;
	case BPF_ALU64 | BPF_XOR | BPF_X:
	case BPF_ALU64 | BPF_XOR | BPF_K:  dr ^= s1; break;
	case BPF_ALU64 | BPF_MUL | BPF_X:
	case BPF_ALU64 | BPF_MUL | BPF_K:  dr *= s1; break;
	case BPF_ALU64 | BPF_MOV | BPF_X:
	case BPF_ALU64 | BPF_MOV | BPF_K:  dr = s1; break;
	case BPF_ALU64 | BPF_ARSH | BPF_X:
	case BPF_ALU64 | BPF_ARSH | BPF_K: dr = (int64_t)dr >> s1; break;
	case BPF_ALU64 | BPF_NEG:	   dr = -sr;
	case BPF_ALU64 | BPF_DIV | BPF_X:
	case BPF_ALU64 | BPF_DIV | BPF_K:
	  if (s1 == 0)
	    return 0;
	  dr /= s1;
	  break;
	case BPF_ALU64 | BPF_MOD | BPF_X:
	case BPF_ALU64 | BPF_MOD | BPF_K:
	  if (s1 == 0)
	    return 0;
	  dr %= s1;
	  break;

	case BPF_ALU | BPF_ADD | BPF_X:
	case BPF_ALU | BPF_ADD | BPF_K:  dr = (uint32_t)(dr + s1); break;
	case BPF_ALU | BPF_SUB | BPF_X:
	case BPF_ALU | BPF_SUB | BPF_K:  dr = (uint32_t)(dr - s1); break;
	case BPF_ALU | BPF_AND | BPF_X:
	case BPF_ALU | BPF_AND | BPF_K:  dr = (uint32_t)(dr & s1); break;
	case BPF_ALU | BPF_OR  | BPF_X:
	case BPF_ALU | BPF_OR  | BPF_K:  dr = (uint32_t)(dr | s1); break;
	case BPF_ALU | BPF_LSH | BPF_X:
	case BPF_ALU | BPF_LSH | BPF_K:  dr = (uint32_t)dr << s1; break;
	case BPF_ALU | BPF_RSH | BPF_X:
	case BPF_ALU | BPF_RSH | BPF_K:  dr = (uint32_t)dr >> s1; break;
	case BPF_ALU | BPF_XOR | BPF_X:
	case BPF_ALU | BPF_XOR | BPF_K:  dr = (uint32_t)(dr ^ s1); break;
	case BPF_ALU | BPF_MUL | BPF_X:
	case BPF_ALU | BPF_MUL | BPF_K:  dr = (uint32_t)(dr * s1); break;
	case BPF_ALU | BPF_MOV | BPF_X:
	case BPF_ALU | BPF_MOV | BPF_K:  dr = (uint32_t)s1; break;
	case BPF_ALU | BPF_ARSH | BPF_X:
	case BPF_ALU | BPF_ARSH | BPF_K: dr = (int32_t)dr >> s1; break;
	case BPF_ALU | BPF_NEG:		 dr = -(uint32_t)sr;
	case BPF_ALU | BPF_DIV | BPF_X:
	case BPF_ALU | BPF_DIV | BPF_K:
	  if ((uint32_t)s1 == 0)
	    return 0;
	  dr = (uint32_t)dr / (uint32_t)s1;
	  break;
	case BPF_ALU | BPF_MOD | BPF_X:
	case BPF_ALU | BPF_MOD | BPF_K:
	  if ((uint32_t)s1 == 0)
	    return 0;
	  dr = (uint32_t)dr % (uint32_t)s1;
	  break;

	case BPF_LD | BPF_IMM | BPF_DW:
	  switch (i->src_reg)
	    {
	    case 0:
	      dr = (uint32_t)si | ((uint64_t)i[1].imm << 32);
	      break;
	    case BPF_PSEUDO_MAP_FD:
	      if (si >= c->maps.size())
		return 0;
	      dr = as_int(c->maps[si]);
	      break;
	    default:
	      abort();
	    }
	  regs[i->dst_reg] = dr;
	  i += 2;
	  continue;

	case BPF_JMP | BPF_JEQ | BPF_X:
	case BPF_JMP | BPF_JEQ | BPF_K:
	  if (dr == s1)
	    goto dojmp;
	  goto nowrite;
	case BPF_JMP | BPF_JNE | BPF_X:
	case BPF_JMP | BPF_JNE | BPF_K:
	  if (dr != s1)
	    goto dojmp;
	  goto nowrite;
	case BPF_JMP | BPF_JGT | BPF_X:
	case BPF_JMP | BPF_JGT | BPF_K:
	  if (dr > s1)
	    goto dojmp;
	  goto nowrite;
	case BPF_JMP | BPF_JGE | BPF_X:
	case BPF_JMP | BPF_JGE | BPF_K:
	  if (dr >= s1)
	    goto dojmp;
	  goto nowrite;
	case BPF_JMP | BPF_JSGT | BPF_X:
	case BPF_JMP | BPF_JSGT | BPF_K:
	  if ((int64_t)dr > (int64_t)s1)
	    goto dojmp;
	  goto nowrite;
	case BPF_JMP | BPF_JSGE | BPF_X:
	case BPF_JMP | BPF_JSGE | BPF_K:
	  if ((int64_t)dr >= (int64_t)s1)
	    goto dojmp;
	  goto nowrite;
	case BPF_JMP | BPF_JSET | BPF_X:
	case BPF_JMP | BPF_JSET | BPF_K:
	  if (dr & s1)
	    goto dojmp;
	  goto nowrite;
	case BPF_JMP | BPF_JA:
	dojmp:
	  i += 1 + i->off;
	  continue;

	case BPF_JMP | BPF_CALL:
	  switch (si)
	    {
	    case BPF_FUNC_map_lookup_elem:
	      dr = as_int(as_map(regs[1])->lookup(as_ptr(regs[2])));
	      break;
	    case BPF_FUNC_map_update_elem:
	      dr = as_map(regs[1])->update(as_ptr(regs[2]),
					   as_ptr(regs[3]), regs[4]);
	      break;
	    case BPF_FUNC_map_delete_elem:
	      dr = as_map(regs[1])->remove(as_ptr(regs[2]));
	      break;
	    case BPF_FUNC_trace_printk:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
              // regs[2] is the strlen(regs[1]) - not used by printf(3);
              // instead we assume regs[1] string is \0 terminated
	      dr = printf(as_str(regs[1]), /*regs[2],*/ regs[3], regs[4], regs[5]);
#pragma GCC diagnostic pop
	      break;
	    default:
	      abort();
	    }
	  regs[0] = dr;
	  regs[1] = 0xdeadbeef;
	  regs[2] = 0xdeadbeef;
	  regs[3] = 0xdeadbeef;
	  regs[4] = 0xdeadbeef;
	  goto nowrite;

	case BPF_JMP | BPF_EXIT:
	  return regs[0];

	default:
	  abort();
	}

      regs[i->dst_reg] = dr;
    nowrite:
      i++;
    }
  return 0;
}
