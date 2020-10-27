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



using bess::utils::CuckooMap;
struct rte_hash *hash = nullptr;
typedef uint16_t value_t;
value_t* keys;
uint8_t* Key64 ;

 

struct rte_hash_parameters session_map_params = {
      .name= "test1",
      .entries = 1<<15,
      .reserved = 0,
      .key_len = 64,    //amar
      .hash_func = rte_hash_crc,
      .hash_func_init_val = 0,
      .socket_id = (int)rte_socket_id(),
     .extra_flag = /*RTE_HASH_EXTRA_FLAGS_EXT_TABLE | */RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY /*| RTE_HASH_EXTRA_FLAGS_TRANS_MEM_SUPPORT*/};



static inline value_t derive_val(uint32_t key) {
  return (value_t)(key + 3);
}
static inline value_t derive_val64(uint8_t* k) {
  return (value_t)(k[0] + 3);
}
static Random rng;

// Performs CuckooMap setup / teardown.
class CuckooMapFixture : public benchmark::Fixture {
 public:
  CuckooMapFixture() : cuckoo_(), stl_map_() {}
  


#if bess_dpdk
  virtual void SetUp(benchmark::State &state) {
    
    stl_map_ = new std::unordered_map<uint32_t, value_t>();
#if pure_bess
    cuckoo_ = new CuckooMap<uint32_t, value_t>();//;
#else
uint32_t entry= state.range(0);
session_map_params.entries=entry<<1 ;//+entry/2;
cuckoo_ = new CuckooMap<uint32_t, value_t>(0,0,&session_map_params);
keyi=0;
#endif
    rng.SetSeed(0);
#if (bess_dpdk) && (pure_bess==0)
int ret =-1;
#endif
int i=0;
    for (i = 0; i < state.range(0); i++) {
  Key64[0] = (uint8_t) rng.Get();
     value_t val = derive_val64(Key64);
 
//      uint32_t key = rng.Get();     //amar
//        value_t val = derive_val(key);


 #if (bess_dpdk) && (pure_bess==0)     
      keys[keyi++] = val; 
 #endif     
 
#if pure_bess
      auto it = cuckoo_->Insert(key, val);
      if(it->second != val);
       throw std::runtime_error("pure bess insert failed");
#endif
#if (bess_dpdk) && (pure_bess==0)
ret = cuckoo_->Insert_dpdk(Key64, (void*)&keys[keyi-1]);
//ret = cuckoo_->Insert_dpdk(&key, (void*)&keys[keyi-1]);    //amar
if (ret != 0)
      throw std::runtime_error("dpdk-bess insert failed");
#endif

    }
    std::cout<<"INSERT entries ="<< session_map_params.entries<< "i="<<i<<std::endl;
  }
#else
 virtual void SetUp(benchmark::State &state) {
    
    stl_map_ = new std::unordered_map<uint32_t, value_t>();
    cuckoo_ = new CuckooMap<uint32_t, value_t>(0,0,&session_map_params);

    rng.SetSeed(0);
keyi=0;
    for (int i = 0; i < state.range(0); i++) {
   //   uint32_t key = rng.Get();
      value_t val = derive_val(key);

      keys[keyi++] = val; 
      (*stl_map_)[key] = val;
      
      int ret1 =     rte_hash_add_key_data(hash, (&key), (void*)&keys[keyi-1]);
      if (ret1 <0)
      //std::cout<<"add failed";
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
  uint64_t keyi=0;
};

#if bess_dpdk==0 //pure dpdk
// Benchmarks the Find() method in CuckooMap, which is inlined.
BENCHMARK_DEFINE_F(CuckooMapFixture, CuckooMapInlinedGet)
(benchmark::State &state) {
  while (true) {
    const size_t n = state.range(0);
    rng.SetSeed(0);

    for (size_t i = 0; i < n; i++) {
  //    uint32_t key = rng.Get();
     // std::pair<uint32_t, value_t> *val;
      value_t *ans;
    
      int ret = rte_hash_lookup_data(hash, (&key),(void**) &ans);
      if ((ret <0) || (*ans != derive_val(key)))
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
#if (bess_dpdk) && (pure_bess==0)    
int ret=-1;
void* val1;
#endif
    for (size_t i = 0; i < n; i++) {
//      uint32_t key = rng.Get();   //amar
      Key64[0] = (uint8_t) rng.Get();
       
#if (bess_dpdk) && (pure_bess==0)
      benchmark::DoNotOptimize(ret = cuckoo_->Find_dpdk(Key64,&val1)); 
 //     benchmark::DoNotOptimize(ret = cuckoo_->Find_dpdk(&key,&val1)); //amar
#endif

#if pure_bess
std::pair<uint32_t, value_t> *val;
benchmark::DoNotOptimize(val = cuckoo_->Find(key)); 


      DCHECK(val);
      DCHECK_EQ(val->second, derive_val(key));

      if (val->second != derive_val(key))
      throw std::runtime_error("find-purebess failed");
#endif

#if (bess_dpdk) && (pure_bess==0) 
value_t ans = *((value_t*)val1);
//if (ans != derive_val(key))   //amar

if (ans != derive_val64(Key64))
      throw std::runtime_error("find failed");
#endif      

      if (!state.KeepRunning()) {
        state.SetItemsProcessed(state.iterations());
        return;
      }
    
    }
  }
}
#endif

BENCHMARK_REGISTER_F(CuckooMapFixture, CuckooMapInlinedGet)
   // ->Iterations(1)//41285403)
    ->RangeMultiplier(2)
    ->Range(1024, 4 << 24);



//BENCHMARK_MAIN();

#if bess_dpdk==0 //means pure dpdk api..
int main(int argc, char** argv) {        \
      bess::InitDpdk(2048);\
      keys = new value_t[100000000];
      hash = rte_hash_create(&session_map_params); \
    ::benchmark::Initialize(&argc, argv);  \
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1; \
    ::benchmark::RunSpecifiedBenchmarks(); \
  }
#else //bessdpdk..
int main(int argc, char** argv) {        \
     
      bess::InitDpdk(0);\
      keys = new value_t[100000000];
      Key64 = new uint8_t[64];

  
    ::benchmark::Initialize(&argc, argv);  \
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1; \
    ::benchmark::RunSpecifiedBenchmarks(); \
  }
#endif