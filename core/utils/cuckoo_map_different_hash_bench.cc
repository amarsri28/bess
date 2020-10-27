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

// Benchmarks for our custom hashtable implementation.
//
// TODO(barath): Add dpdk benchmarks from oldtests/htable.cc once we re-enable
// dpdk memory allocation.

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

// Benchmarks for our custom hashtable implementation.
//
// TODO(barath): Add dpdk benchmarks from oldtests/htable.cc once we re-enable
// dpdk memory allocation.
#include <iostream>
#include "cuckoo_map.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <functional>
#include <unordered_map>

#include <benchmark/benchmark.h>
#include <glog/logging.h>

#include "common.h"
#include "random.h"
#include "rte_lcore.h"
#include "dpdk.h"
#include <rte_hash_crc.h>
#include <rte_jhash.h>
#include <rte_hash.h>

#define bess_dpdk 1   //1 means dpdk-bess;., 0 means pure dpdk api direct usage
#define pure_bess 0


 typedef uint32_t HashResult;
static HashResult hash1(const void *data, __attribute__((unused)) uint32_t data_len, __attribute__((unused)) uint32_t init_val)/*(const K& key, const H& hasher) */
 {
  
    return std::hash<uint32_t>{}(*(uint32_t*)data | (1u << 31));
  }


uint32_t *keyptr;

using bess::utils::CuckooMap;
struct rte_hash *hash = nullptr;
typedef uint16_t value_t;
struct rte_hash_parameters session_map_params = {
      .name= "test1",
      .entries = 0,
      .reserved = 0,
      .key_len = sizeof(uint32_t),
      .hash_func = hash1, //rte_hash_crc,//
      .hash_func_init_val = 0,
      .socket_id = (int)rte_socket_id(),
      .extra_flag =  RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY};



static inline value_t derive_val(uint32_t key) {
  return (value_t)(key + 3);
}

static Random rng;

// Performs CuckooMap setup / teardown.
class CuckooMapFixture : public benchmark::Fixture {
 public:
  CuckooMapFixture() : cuckoo_(), Bess_dpdk() {}

#if bess_dpdk
  virtual void SetUp(benchmark::State &state) {
       rng.SetSeed(0);
      int ret =-1;
      int n = state.range(0);
  
session_map_params.entries=n<<1 ;
    cuckoo_ = new CuckooMap<uint32_t, value_t>();//;
    Bess_dpdk = new CuckooMap<uint32_t, value_t>(0,0,&session_map_params);

    for (int i = 0; i < n; i++) {
      uint32_t key = rng.Get();
      value_t val = derive_val(key);
     
      std::pair<uint32_t,value_t> *ans = cuckoo_->Insert(key, val);
      if(ans->second != derive_val(key))
          throw std::runtime_error("Original Bess insert failed");
    
       keyptr[i] = val;
      ret = Bess_dpdk->Insert_dpdk(&key, (void*)&keyptr[i]);

      if (ret < 0)
      throw std::runtime_error("Dpdk- Bess Insert failed");


    }
  }
#else
 virtual void SetUp(benchmark::State &state) {
    
    stl_map_ = new std::unordered_map<uint32_t, value_t>();
   // cuckoo_ = new CuckooMap<uint32_t, value_t>(0,0,&session_map_params);

    rng.SetSeed(0);

    for (int i = 0; i < state.range(0); i++) {
      uint32_t key = rng.Get();
      value_t val = derive_val(key);

      (*stl_map_)[key] = val;
      
      int ret1 =     rte_hash_add_key_data(hash, (&key), &val);
      if (ret1 <0)
      //std::cout<<"add failed";
      throw std::runtime_error("add failed");
    }
  }
#endif

  virtual void TearDown(benchmark::State &) {
    delete Bess_dpdk;
    delete cuckoo_;  
  }

 protected:
  CuckooMap<uint32_t, value_t> *cuckoo_;
  CuckooMap<uint32_t, value_t> *Bess_dpdk;
};

#if bess_dpdk==0 //pure dpdk
// Benchmarks the Find() method in CuckooMap, which is inlined.
BENCHMARK_DEFINE_F(CuckooMapFixture, CuckooMapInlinedGet)
(benchmark::State &state) {
  while (true) {
    const size_t n = state.range(0);
    rng.SetSeed(0);

    for (size_t i = 0; i < n; i++) {
      uint32_t key = rng.Get();
     // std::pair<uint32_t, value_t> *val;
      int *ans;
    
      int ret = rte_hash_lookup_data(hash, (&key),(void**) &ans/*&data*/);
      if (ret <0)
      throw std::runtime_error("lookup failed");
     //  if(ret<0) 
     // DCHECK(val);
      DCHECK_EQ(*ans, derive_val(key));

         
      if (!state.KeepRunning()) {
        state.SetItemsProcessed(state.iterations());
        return;
      }
    
    }
  }
}
#else
// Benchmarks the Find() method in CuckooMap, which is inlined.
BENCHMARK_DEFINE_F(CuckooMapFixture, CuckooMapInlinedGet)
(benchmark::State &state) {
  while (true) {
    const size_t n = state.range(0);
    rng.SetSeed(0);
    int ret=-1;
    
    for (size_t i = 0; i < n; i++) {
      uint32_t key = rng.Get();
      void* val1;
       
      benchmark::DoNotOptimize(ret = Bess_dpdk->Find_dpdk(&key,&val1)); 
       if((*(value_t*)val1) != derive_val(key) )
         throw std::runtime_error("Dpdk- Bess Find failed");
         
      if (!state.KeepRunning()) {
        state.SetItemsProcessed(state.iterations());
        return;
      }
    
    }
  }
}
#endif



BENCHMARK_REGISTER_F(CuckooMapFixture, CuckooMapInlinedGet)
    ->RangeMultiplier(2)
    ->Range(1024, 4 << 23);
    

// Benchmarks the find method on the STL unordered_map.
BENCHMARK_DEFINE_F(CuckooMapFixture, OrigBessUnorderedMapGet)
(benchmark::State &state) {
  while (true) {
    const size_t n = state.range(0);
    rng.SetSeed(0);

    for (size_t i = 0; i < n; i++) {
      uint32_t key = rng.Get();
    
      std::pair<uint32_t, value_t> *ans;
      benchmark::DoNotOptimize(ans = cuckoo_->Find(key)); 

      if (ans->second != derive_val(key))
      throw std::runtime_error("find failed");

      if (!state.KeepRunning()) {
        state.SetItemsProcessed(state.iterations());
        return;
      }
    }
  }
}

BENCHMARK_REGISTER_F(CuckooMapFixture, OrigBessUnorderedMapGet)
    ->RangeMultiplier(2)
    ->Range(1024, 4 << 23);

//BENCHMARK_MAIN();

#if bess_dpdk==0 //means pure dpdk api..
int main(int argc, char** argv) {        \
      bess::InitDpdk(2048);\
      hash = rte_hash_create(&session_map_params); \
    ::benchmark::Initialize(&argc, argv);  \
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1; \
    ::benchmark::RunSpecifiedBenchmarks(); \
  }
#else //bessdpdk..
int main(int argc, char** argv) {        \
      bess::InitDpdk(2048);\
      keyptr = new uint32_t[100000000];

    ::benchmark::Initialize(&argc, argv);  \
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1; \
    ::benchmark::RunSpecifiedBenchmarks(); \
  }
#endif

















#if 0
#include "cuckoo_map.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <functional>
#include <unordered_map>

#include <benchmark/benchmark.h>
#include <glog/logging.h>

#include "common.h"
#include "random.h"
#include "rte_lcore.h"
#include "dpdk.h"
#include <rte_hash_crc.h>
#include <rte_hash.h>

using bess::utils::CuckooMap;
struct rte_hash *hash = nullptr;
typedef uint16_t value_t;
struct rte_hash_parameters session_map_params = {
      .name= "test1",
      .entries = 1<<22,
      .reserved = 0,
      .key_len = sizeof(uint32_t),
      .hash_func =rte_hash_crc,
      .hash_func_init_val = 0,
      .socket_id = 0,
      .extra_flag = RTE_HASH_EXTRA_FLAGS_EXT_TABLE | RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF | RTE_HASH_EXTRA_FLAGS_TRANS_MEM_SUPPORT};
     /* .extra_flag = RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY};*/

static inline value_t derive_val(uint32_t key) {
  return (value_t)(key + 3);
}

static Random rng;

// Performs CuckooMap setup / teardown.
class CuckooMapFixture : public benchmark::Fixture {
 public:
  CuckooMapFixture() : cuckoo_(), stl_map_() {}

//#if 0
  virtual void SetUp(benchmark::State &state) {
    // bess::InitDpdk(0); 
     LOG_FIRST_N(ERROR, 1)
            << "AFTER INIT DPDK:\n";
    stl_map_ = new std::unordered_map<uint32_t, value_t>();
    cuckoo_ = new CuckooMap<uint32_t, value_t>(0,0,&session_map_params);
   
    rng.SetSeed(0);

    for (int i = 0; i < state.range(0); i++) {
      uint32_t key = rng.Get();
      value_t val = derive_val(key);

      (*stl_map_)[key] = val;
      cuckoo_->Insert(key, val);
    }
  }
//#endif

#if 0
 virtual void SetUp(benchmark::State &state) {
    
    stl_map_ = new std::unordered_map<uint32_t, value_t>();
   // cuckoo_ = new CuckooMap<uint32_t, value_t>(0,0,&session_map_params);

    rng.SetSeed(0);

    for (int i = 0; i < state.range(0); i++) {
      uint32_t key = rng.Get();
      value_t val = derive_val(key);

      (*stl_map_)[key] = val;
      
      int ret1 =   rte_hash_add_key_data(hash, (&key), &val);
      if (ret1 <0)
      throw std::runtime_error("add failed");
    }
  }

#endif

  virtual void TearDown(benchmark::State &) {
    delete stl_map_;
    delete cuckoo_;
  }

 protected:
  CuckooMap<uint32_t, value_t> *cuckoo_;
  std::unordered_map<uint32_t, value_t> *stl_map_;
};

#if 0
// Benchmarks the Find() method in CuckooMap, which is inlined.
BENCHMARK_DEFINE_F(CuckooMapFixture, CuckooMapInlinedGet)
(benchmark::State &state) {
  while (true) {
    const size_t n = state.range(0);
    rng.SetSeed(0);

    for (size_t i = 0; i < n; i++) {
      uint32_t key = rng.Get();
     // std::pair<uint32_t, value_t> *val;
      int *ans;
    
      int ret = rte_hash_lookup_data(hash, (&key),(void**) &ans/*&data*/);
      if (ret <0)
      throw std::runtime_error("lookup failed");
     //  if(ret<0) 
     // DCHECK(val);
      DCHECK_EQ(*ans, derive_val(key));

         
      if (!state.KeepRunning()) {
        state.SetItemsProcessed(state.iterations());
        return;
      }
    
    }
  }
}

#endif



// Benchmarks the Find() method in CuckooMap, which is inlined.
BENCHMARK_DEFINE_F(CuckooMapFixture, CuckooMapInlinedGet)
(benchmark::State &state) {
  while (true) {
    const size_t n = state.range(0);
    rng.SetSeed(0);

    for (size_t i = 0; i < n; i++) {
      uint32_t key = rng.Get();
      std::pair<uint32_t, value_t> *val;

      benchmark::DoNotOptimize(val = cuckoo_->Find(key));
      DCHECK(val);
      DCHECK_EQ(val->second, derive_val(key));

      if(val->second != derive_val(key))//{
          throw std::runtime_error("lolololo");
          

      if (!state.KeepRunning()) {
        state.SetItemsProcessed(state.iterations());
        return;
      }
    }
  }
}


BENCHMARK_REGISTER_F(CuckooMapFixture, CuckooMapInlinedGet)
    ->RangeMultiplier(4)
    ->Range(4, 4 << 20);

// Benchmarks the find method on the STL unordered_map.
BENCHMARK_DEFINE_F(CuckooMapFixture, STLUnorderedMapGet)
(benchmark::State &state) {
  while (true) {
    const size_t n = state.range(0);
    rng.SetSeed(0);

    for (size_t i = 0; i < n; i++) {
      uint32_t key = rng.Get();
      value_t val;

      benchmark::DoNotOptimize(val = (*stl_map_)[key]);
      DCHECK_EQ(val, derive_val(key));

      if (!state.KeepRunning()) {
        state.SetItemsProcessed(state.iterations());
        return;
      }
    }
  }
}

BENCHMARK_REGISTER_F(CuckooMapFixture, STLUnorderedMapGet)
    ->RangeMultiplier(4)
    ->Range(4, 4 << 20);

//BENCHMARK_MAIN();
int main(int argc, char** argv) {        \
      bess::InitDpdk(0);\
   /*   hash = rte_hash_create(&session_map_params); \*/
    ::benchmark::Initialize(&argc, argv);  \
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1; \
    ::benchmark::RunSpecifiedBenchmarks(); \
  }

#endif