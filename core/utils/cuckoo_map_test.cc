// Copyright (c) 2016-2017, Nefeli Networks, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// * Neither the names of the copyright holders nor the names of their
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "cuckoo_map.h"

#include <map>
#include <unordered_map>
#include <vector>
#include <gtest/gtest.h>
#include <rte_hash.h>
#include "random.h"

#include <rte_lcore.h>
#include <dpdk.h>
#include <rte_hash_crc.h>



 struct FlowId {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint32_t src_port;
    uint32_t dst_port;
    uint8_t protocol;
  };
struct rte_hash_parameters session_map_params = {
      .name= "test1",
      .entries = 16,
      .reserved = 0,
      .key_len = sizeof(int),
      .hash_func =rte_hash_crc,
      .hash_func_init_val = 0,
      .socket_id = 0,
      .extra_flag = RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY};
      
      

struct CopyConstructorOnly {
  // FIXME: CuckooMap should work without this default constructor
  CopyConstructorOnly() = default;
  CopyConstructorOnly(CopyConstructorOnly &&other) = delete;

  CopyConstructorOnly(int aa, int bb): a(aa), b(bb) {}
  CopyConstructorOnly(const CopyConstructorOnly &other)
      : a(other.a), b(other.b) {}

  int a;
  int b;
};

struct MoveConstructorOnly {
  // FIXME: CuckooMap should work without this default constructor
  MoveConstructorOnly() = default;
  MoveConstructorOnly(const MoveConstructorOnly &other) = delete;

  MoveConstructorOnly(int aa, int bb): a(aa), b(bb) {}
  MoveConstructorOnly(MoveConstructorOnly &&other) noexcept
      : a(other.a), b(other.b) {
    other.a = 0;
    other.b = 0;
  }

  int a;
  int b;
};

// C++ has no clean way to specialize templates for derived typess...
// so we just define a hash functor for each.

template <>
struct std::hash<CopyConstructorOnly> {
  std::size_t operator()(const CopyConstructorOnly &t) const noexcept {
    return std::hash<int>()(t.a + t.b);  // doesn't need to be a good one...
  }
};

template <>
struct std::hash<MoveConstructorOnly> {
  std::size_t operator()(const MoveConstructorOnly &t) const noexcept {
    return std::hash<int>()(t.a * t.b);  // doesn't need to be a good one...
  }
};

namespace {

using bess::utils::CuckooMap;
  
//dpdk Test Insert func
TEST(CuckooMapTest, Insert) {
  int p=9;
  bess::InitDpdk(0); 
  CuckooMap<uint32_t, uint16_t> Dpdk_Cuckoo(0,0,&session_map_params);
 
  auto *entry = Dpdk_Cuckoo.Insert(11,p);
  EXPECT_EQ(11, entry->first);
  
  auto *entry1 = Dpdk_Cuckoo.Find(11);
 	ASSERT_NE(nullptr, entry1);
  EXPECT_EQ(11, entry1->first);
  EXPECT_EQ(9, entry1->second);
  entry = Dpdk_Cuckoo.Find(11);
  EXPECT_EQ(9, entry->second);

  auto *entry77 = Dpdk_Cuckoo.Insert(45,p);//val1);
   EXPECT_EQ(9, entry77->second);
  
}

TEST(CuckooMapTest, Find) {
  CuckooMap<uint32_t, uint16_t> cuckoo(0,0,&session_map_params);
  auto *entry = cuckoo.Insert(1, 99);
  ASSERT_NE(nullptr, entry);
  cuckoo.Insert(2, 99);
  EXPECT_EQ(entry->second, 99);
  EXPECT_EQ(cuckoo.Find(2)->second, 99);
  auto *entry1 = cuckoo.Find(1);
  ASSERT_NE(nullptr, entry1);
  cuckoo.Insert(1, 2);
  EXPECT_EQ(cuckoo.Find(1)->second, 2);
  EXPECT_EQ(cuckoo.Find(3), nullptr);
  EXPECT_EQ(cuckoo.Find(4), nullptr);
}

// Test Insert function
TEST(CuckooMapTest, DpdkInsert) {
  CuckooMap<uint32_t, uint16_t> cuckoo;
  EXPECT_EQ(cuckoo.Insert(1, 99)->second, 99);
  EXPECT_EQ(cuckoo.Insert(2, 98)->second, 98);
  EXPECT_EQ(cuckoo.Insert(1, 1)->second, 1);
}

template<typename T>
void CompileTimeInstantiation() {
  std::map<int, T> m1;
  std::map<T, int> m2;
  std::map<T, T> m3;
  std::unordered_map<int, T> u1;
  std::unordered_map<T, int> u2;
  std::unordered_map<T, T> u3;
  std::vector<T> v1;

  // FIXME: currently, CuckooMap does not support types without a default
  // constructor. The following will fail with the current code.
  // CuckooMap<int, T> c1;
  // CuckooMap<T, int> c2;
  // CuckooMap<T, T> c3;
}

TEST(CuckooMap, TypeSupport) {
  // Standard containers, such as std::map and std::vector, should be able to
  // contain types with various constructor and assignment restrictions.
  // The below will check this ability at compile time.
  CompileTimeInstantiation<CopyConstructorOnly>();
  CompileTimeInstantiation<MoveConstructorOnly>();
}

// Test insertion with move
TEST(CuckooMapTest, MoveInsert) {
  CuckooMap<uint32_t, MoveConstructorOnly> cuckoo;
  auto expected = MoveConstructorOnly(3, 4);
  auto *entry = cuckoo.Insert(11, std::move(expected));
  ASSERT_NE(nullptr, entry);
  const auto &x = entry->second;
  EXPECT_EQ(3, x.a);
  EXPECT_EQ(4, x.b);
}
// Test insertion with copy
TEST(CuckooMapTest, CopyInsert) {
 
  bess::InitDpdk(0); 
  CuckooMap<uint32_t, CopyConstructorOnly> cuckoo(0,0,&session_map_params);
  auto expected = CopyConstructorOnly(1, 2);
  auto *entry = cuckoo.Insert(10, expected);

  ASSERT_NE(nullptr, entry);
  const auto &x = entry->second;
  EXPECT_EQ(1, x.a);
  EXPECT_EQ(2, x.b);
}



// Test Emplace function
TEST(CuckooMapTest, Emplace) {
  CuckooMap<uint32_t, CopyConstructorOnly> cuckoo;
  auto *entry = cuckoo.Emplace(12, 5, 6);
  ASSERT_NE(nullptr, entry);
  const auto &x = entry->second;
  EXPECT_EQ(5, x.a);
  EXPECT_EQ(6, x.b);
}

// Test Find function


// Test Remove function
TEST(CuckooMapTest, Remove) {
  CuckooMap<uint32_t, uint16_t> cuckoo;

  cuckoo.Insert(1, 99);
  cuckoo.Insert(2, 99);

  EXPECT_EQ(cuckoo.Find(1)->second, 99);
  EXPECT_EQ(cuckoo.Find(2)->second, 99);

  EXPECT_TRUE(cuckoo.Remove(1));
  EXPECT_TRUE(cuckoo.Remove(2));

  EXPECT_EQ(cuckoo.Find(1), nullptr);
  EXPECT_EQ(cuckoo.Find(2), nullptr);
}

// Test Count function
TEST(CuckooMapTest, Count) {
  CuckooMap<uint32_t, uint16_t> cuckoo;

  EXPECT_EQ(cuckoo.Count(), 0);

  cuckoo.Insert(1, 99);
  cuckoo.Insert(2, 99);
  EXPECT_EQ(cuckoo.Count(), 2);

  cuckoo.Insert(1, 2);
  EXPECT_EQ(cuckoo.Count(), 2);

  EXPECT_TRUE(cuckoo.Remove(1));
  EXPECT_TRUE(cuckoo.Remove(2));
  EXPECT_EQ(cuckoo.Count(), 0);
}

// Test Clear function
TEST(CuckooMapTest, Clear) {
  CuckooMap<uint32_t, uint16_t> cuckoo;

  EXPECT_EQ(cuckoo.Count(), 0);

  cuckoo.Insert(1, 99);
  cuckoo.Insert(2, 99);
  EXPECT_EQ(cuckoo.Count(), 2);

  cuckoo.Clear();
  EXPECT_EQ(cuckoo.Count(), 0);

  EXPECT_FALSE(cuckoo.Remove(1));
  EXPECT_FALSE(cuckoo.Remove(2));
}

// Test iterators
TEST(CuckooMapTest, Iterator) {
  CuckooMap<uint32_t, uint16_t> cuckoo;

  EXPECT_EQ(cuckoo.begin(), cuckoo.end());

  cuckoo.Insert(1, 99);
  cuckoo.Insert(2, 99);
  auto it = cuckoo.begin();
  EXPECT_EQ(it->first, 1);
  EXPECT_EQ(it->second, 99);

  ++it;
  EXPECT_EQ(it->first, 2);
  EXPECT_EQ(it->second, 99);

  it++;
  EXPECT_EQ(it, cuckoo.end());
}

// Test different keys with the same hash value
TEST(CuckooMapTest, CollisionTest) {
  class BrokenHash {
   public:
    bess::utils::HashResult operator()(const uint32_t) const { return 9999999; }
  };

  CuckooMap<int, int, BrokenHash> cuckoo;

  // Up to 8 (2 * slots/bucket) hash collision should be acceptable
  const int n = 8;
  for (int i = 0; i < n; i++) {
    EXPECT_TRUE(cuckoo.Insert(i, i + 100));
  }
  EXPECT_EQ(nullptr, cuckoo.Insert(n, n + 100));

  for (int i = 0; i < n; i++) {
    auto *ret = cuckoo.Find(i);
    CHECK_NOTNULL(ret);
    EXPECT_EQ(i + 100, ret->second);
  }
}

// RandomTest
TEST(CuckooMapTest, RandomTest) {
  typedef uint32_t key_t;
  typedef uint64_t value_t;

  const size_t iterations = 10000000;
  const size_t array_size = 100000;
  value_t truth[array_size] = {0};  // 0 means empty
  Random rd;

  CuckooMap<key_t, value_t> cuckoo;

  // populate with 50% occupancy
  for (size_t i = 0; i < array_size / 2; i++) {
    key_t idx = rd.GetRange(array_size);
    value_t val = static_cast<value_t>(rd.Get()) + 1;
    truth[idx] = val;
    cuckoo.Insert(idx, val);
  }

  // check if the initial population succeeded
  for (size_t i = 0; i < array_size; i++) {
    auto ret = cuckoo.Find(i);
    if (truth[i] == 0) {
      EXPECT_EQ(nullptr, ret);
    } else {
      CHECK_NOTNULL(ret);
      EXPECT_EQ(truth[i], ret->second);
    }
  }

  for (size_t i = 0; i < iterations; i++) {
    uint32_t odd = rd.GetRange(10);
    key_t idx = rd.GetRange(array_size);

    if (odd == 0) {
      // 10% insert
      value_t val = static_cast<value_t>(rd.Get()) + 1;
      auto ret = cuckoo.Insert(idx, val);
      EXPECT_NE(nullptr, ret);
      truth[idx] = val;
    } else if (odd == 1) {
      // 10% delete
      bool ret = cuckoo.Remove(idx);
      EXPECT_EQ(truth[idx] != 0, ret);
      truth[idx] = 0;
    } else {
      // 80% lookup
      auto ret = cuckoo.Find(idx);
      if (truth[idx] == 0) {
        EXPECT_EQ(nullptr, ret);
      } else {
        CHECK_NOTNULL(ret);
        EXPECT_EQ(truth[idx], ret->second);
      }
    }
  }
}

}  // namespace
