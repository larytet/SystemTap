// -*- C++ -*-
// Copyright (C) 2016 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "bpf-bitset.h"

namespace bpf {

void
throw_out_of_range(const char *where)
{
  throw std::out_of_range(where);
}

namespace bitset {

bool
set1_const_ref::empty () const
{
  for (size_t i = 0; i < words; ++i)
    if (data[i])
      return false;
  return true;
}

bool
set1_const_ref::operator== (const set1_const_ref &o) const
{
  if (words != o.words)
    return false;
  for (size_t i = 0; i < words; ++i)
    if (data[i] != o.data[i])
      return false;
  return true;
}

bool
set1_const_ref::is_subset_of(const set1_const_ref &o) const
{
  size_t n = std::min(words, o.words);

  for (size_t i = 0; i < n; ++i)
    if (data[i] & ~o.data[i])
      return false;
  for (; n < words; ++n)
    if (data[n])
      return false;

  return true;
}

size_t
set1_const_ref::find_first() const
{
  for (size_t i = 0; i < words; ++i)
    if (data[i])
      return i * bits_per_word + __builtin_ctzl (data[i]);
  return npos;
}

size_t
set1_const_ref::find_next(size_t last) const
{
  size_t i = (last + 1) / bits_per_word;
  size_t o = (last + 1) % bits_per_word;

  if (__builtin_expect (i >= words, 0))
    return npos;

  word_t w = data[i] & ((word_t)-1 << o);
  for (; w == 0; w = data[i])
    if (++i >= words)
      return npos;
  return i * bits_per_word + __builtin_ctzl (w);
}

size_t
set1_const_ref::find_next_zero(size_t last) const
{
  size_t i = (last + 1) / bits_per_word;
  size_t o = (last + 1) % bits_per_word;

  if (__builtin_expect (i >= words, 0))
    return npos;

  word_t w = ~data[i] & ((word_t)-1 << o);
  for (; w == 0; w = ~data[i])
    if (++i >= words)
      return npos;
  return i * bits_per_word + __builtin_ctzl (w);
}

set1::set1(size_t bits)
  : set1_ref(new word_t[round_words(bits)], round_words(bits))
{
  memset(data, 0, words * sizeof(word_t));
}

set1::set1(const set1_const_ref &o)
  : set1_ref(new word_t[o.words], o.words)
{
  memcpy(data, o.data, words * sizeof(word_t));
}

set1::~set1()
{
  delete[] data;
}

set2::set2(size_t m1, size_t m2)
  : n1(m1), w2(round_words(m2)), data(new word_t[m1 * w2])
{
  memset(data, 0, n1 * w2 * sizeof(word_t));
}

set2::set2(const set2 &o)
  : n1(o.n1), w2(o.w2), data(new word_t[o.n1 * o.w2])
{
  memcpy(data, o.data, n1 * w2 * sizeof(word_t));
}

set2::~set2()
{
  delete[] data;
}


std::ostream&
operator<< (std::ostream &o, const set1_const_ref &s)
{
  char sep = '{';
  for (size_t n = s.size(), i = 0; i < n; ++i)
    if (s.test(i))
      {
	o << sep << i;
	sep = ',';
      }
  if (sep == '{')
    o << sep;
  return o << '}';
}

} // namespace bitset
} // namespace bpf
