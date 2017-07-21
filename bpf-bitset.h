// -*- C++ -*-
// Copyright (C) 2016 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

// This differs from std::bitset in being runtime sized, and from
// boost::dynamic_bitset in being space efficient in multiple dimensions.
//
// ??? Could be templatized to n-dimensions.

#ifndef BPF_BITSET_H
#define BPF_BITSET_H

#include <stdexcept>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <climits>

namespace bpf {

void throw_out_of_range(const char *) __attribute__((noreturn));

namespace bitset {

typedef size_t word_t;
static size_t const bits_per_word = sizeof(word_t) * CHAR_BIT;

inline size_t round_words(size_t bits)
{
  return (bits + bits_per_word - 1) / bits_per_word;
}

class bit_ref
{
private:
  word_t * const word;
  size_t const index;

  bit_ref();	// not present

public:
  bit_ref(word_t *w, size_t i) : word(w), index(i) { }

  void reset()      { *word &= ~((word_t)1 << index); }
  void set()        { *word |= (word_t)1 << index; }
  void flip()       { *word ^= (word_t)1 << index; }
  void set(bool v)  { if (v) set(); else reset(); }
  bool test() const { return (*word >> index) & 1; }

  operator bool() const { return test(); }
  bool operator!() const { return !test(); }

  bool operator|= (bool o)
  {
    if (test())
      return true;
    else if (o)
      {
	set();
	return true;
      }
    return false;
  }

  bool operator&= (bool o)
  {
    if (test())
      {
	if (o)
	  return true;
	reset();
      }
    return false;
  }

  bool operator-= (bool o)
  {
    if (test())
      {
	if (!o)
	  return true;
	reset();
      }
    return false;
  }

  bool operator^= (bool o)
  {
    if (o)
      flip();
    return test();
  }
};

class set1;
class set1_ref;
class set1_const_ref
{
  friend class set1;
  friend class set1_ref;

private:
  set1_const_ref();					// not present
  set1_const_ref& operator= (const set1_const_ref &);	// not present

protected:
  word_t *data;
  size_t words;

public:
  static const size_t npos = -1;

  set1_const_ref(word_t *d, size_t w) : data(d), words(w) { }

  bool operator!= (const set1_const_ref &o) const
  {
    return !(*this == o);
  }

  size_t size() const { return words * bits_per_word; }

  bool test(size_t i) const
  {
    size_t w = i / bits_per_word;
    size_t o = i % bits_per_word;
    if (w >= words)
      throw_out_of_range("bpf::bitset::set1_ref::test");
    return (data[w] >> o) & 1;
  }

  bool operator[] (size_t i) const { return test(i); }

  bool empty() const;
  bool operator== (const set1_const_ref &o) const;
  bool is_subset_of(const set1_const_ref &o) const;
  size_t find_first() const;
  size_t find_next(size_t i) const;
  size_t find_next_zero(size_t i) const;
};

class set1_ref : public set1_const_ref
{
private:
  set1_ref();					// not present

public:
  set1_ref(size_t *d, size_t w) : set1_const_ref(d, w) { }

  bit_ref operator[] (size_t i)
  {
    size_t w = i / bits_per_word;
    size_t o = i % bits_per_word;
    if (w >= words)
      throw_out_of_range("bpf::bitset::set1_ref::operator[]");
    return bit_ref(data + w, o);
  }

  set1_ref& operator= (const set1_const_ref &o)
  {
    if (words != o.words)
      throw_out_of_range("bpf::bitset::set1_ref::operator=");
    memcpy(data, o.data, words * sizeof(*data));
    return *this;
  }

  set1_ref& operator= (const set1_ref &o)
  {
    return operator=(static_cast<const set1_const_ref &>(o));
  }

  set1_ref& operator|= (const set1_const_ref &o)
  {
    if (words != o.words)
      throw_out_of_range("bpf::bitset::set1_ref::operator|=");
    for (size_t i = 0; i < words; ++i)
      data[i] |= o.data[i];
    return *this;
  }

  set1_ref& operator&= (const set1_const_ref &o)
  {
    if (words != o.words)
      throw_out_of_range("bpf::bitset::set1_ref::operator&=");
    for (size_t i = 0; i < words; ++i)
      data[i] &= o.data[i];
    return *this;
  }

  set1_ref& operator-= (const set1_const_ref &o)
  {
    if (words != o.words)
      throw_out_of_range("bpf::bitset::set1_ref::operator-=");
    for (size_t i = 0; i < words; ++i)
      data[i] &= ~o.data[i];
    return *this;
  }

  void clear() { memset(data, 0, words * sizeof(*data)); }

  void reset(size_t i) { (*this)[i].reset(); }
  void set(size_t i) { (*this)[i].set(); }
  void set(size_t i, bool v) { (*this)[i].set(v); }
};

class set1 : public set1_ref
{
private:
  set1();					// not present

public:
  set1(size_t bits);
  set1(const set1_const_ref &o);
  ~set1();
};

class set2
{
private:
  size_t n1;
  size_t w2;
  word_t *data;

  set2();					// not present

public:
  set2(size_t m1, size_t m2);
  set2(const set2 &o);
  ~set2();

  size_t size() const { return n1; }

  set2& operator= (const set2 &o)
  {
    if (n1 != o.n1 || w2 != o.w2)
      throw_out_of_range("bpf::bitset::set2::operator=");
    memcpy(data, o.data, n1 * w2 * sizeof(word_t));
  }

  set1_ref operator[] (size_t i)
  {
    if (i >= n1)
      throw_out_of_range("set2::operator[]");
    return set1_ref(data + w2 * i, w2);
  }

  set1_const_ref operator[] (size_t i) const
  {
    if (i >= n1)
      throw_out_of_range("set2::operator[]");
    return set1_const_ref(data + w2 * i, w2);
  }

  void clear() { memset(data, 0, n1 * w2 * sizeof(*data)); }
};

std::ostream& operator<< (std::ostream &o, const set1_const_ref &s);

} // namespace bitset
} // namespace bpf

#endif // BPF_BITSET_H
