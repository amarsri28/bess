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

#define print 0

using bess::utils::CuckooMap;
struct rte_hash *hash = nullptr;
typedef uint32_t value_t;
//value_t* keys;
uint32_t** Keybulk;//[100000] ;
uint32_t *Key64;
 

struct rte_hash_parameters session_map_params = {
      .name= "test1",
      .entries = 0,
      .reserved = 0,
      .key_len = sizeof(uint32_t),    //amar
      .hash_func = rte_hash_crc,
      .hash_func_init_val = 0,
      .socket_id = (int)rte_socket_id(),
     .extra_flag = /*RTE_HASH_EXTRA_FLAGS_EXT_TABLE | */RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY /*| RTE_HASH_EXTRA_FLAGS_TRANS_MEM_SUPPORT*/};



static inline value_t derive_val(uint32_t key) {
  return (value_t)(key + 3);
}
static inline value_t derive_val64(uint32_t k) {
  return (value_t)(k + 3);
}
static Random rng;

// Performs CuckooMap setup / teardown.
class CuckooMapFixture : public benchmark::Fixture {
 public:
  CuckooMapFixture() : cuckoo_(), bess_map_() {}
  

virtual void SetUp(benchmark::State &state) {
    
    bess_map_ = new CuckooMap<uint32_t, value_t>();
    uint32_t entry= state.range(0);
    session_map_params.entries=entry<<1 ;
    cuckoo_ = new CuckooMap<uint32_t, value_t>(0,0,&session_map_params);

       
    int ret =-1;

    for (uint32_t i = 0; i < entry; i++) {
     value_t val = derive_val64(*Keybulk[i]); 

      auto it = bess_map_->Insert(*Keybulk[i], val);
      if(it->second != val)
       throw std::runtime_error("pure bess insert failed");

      ret = cuckoo_->Insert_dpdk(Keybulk[i], (void*)Keybulk[i]); //key and value same but ey copied and pointer saved as value(not data inside pointer) 
      if (ret != 0)
        throw std::runtime_error("dpdk-bess insert failed");

    }
  }


  virtual void TearDown(benchmark::State &) {
    delete bess_map_;
    delete cuckoo_;
  }

 protected:
  CuckooMap<uint32_t, value_t> *cuckoo_;
  CuckooMap<uint32_t, value_t> *bess_map_;
  uint64_t keyi=0;
};


// Benchmarks the Find() method in CuckooMap, which is inlined.
BENCHMARK_DEFINE_F(CuckooMapFixture, CuckooMapInlinedGet)
(benchmark::State &state) {

  void* data[32];

  while (true) {
    const size_t n = state.range(0);
    int ret=-1;
    uint64_t hit_mask=0; 

   for (size_t i = 0; i < n; i=i+32 ) 
    {
hit_mask=0; 
      benchmark::DoNotOptimize(ret = cuckoo_->Lookup_Bulk_data((const void**)&Keybulk[i],32, &hit_mask, data));


if((ret != 32) && (hit_mask != 0xffffffff))
  throw std::runtime_error("Bulk find failed");

#if 0
size_t i1=i;
  for(int j=0;j<32;j++,i1++)
  {
    if(derive_val( *Keybulk[i1]) != *(uint32_t*)data[j])
      throw std::runtime_error("find failed");
  }
#endif 

      if (!state.KeepRunning()) {
#if print        
        std::cout<<i1<<std::endl;
#endif        
        state.SetItemsProcessed(state.iterations());
        return;
      }
    
    }
  }
}


BENCHMARK_REGISTER_F(CuckooMapFixture, CuckooMapInlinedGet)
   // ->Iterations(1)//41285403)
    ->RangeMultiplier(2)
    ->Range(32, 4<<23);//4 << 18);



BENCHMARK_DEFINE_F(CuckooMapFixture, CuckooMapDpdkBessfindInlinedGet)
(benchmark::State &state) {
  
//int i1=0;
  while (true) {
    const size_t n = state.range(0);
    int ret=-1;
    void* val1; //amar
    size_t j=0;
    for (size_t i = 0; i < n;  ) 
    {
      for (j = i; ((j < n) && (j< i+32)); j++ ) 
      {
      benchmark::DoNotOptimize(ret = cuckoo_->Find_dpdk(Keybulk[j],&val1));  //tested

  
      if(*(value_t*)val1 != *Keybulk[j])
        throw std::runtime_error("DpdkBess single-find failed");
      }
       i=j;

      if (!state.KeepRunning()) {
#if print        
        std::cout<<i1<<std::endl;
#endif        
        state.SetItemsProcessed(state.iterations());
        return;
      }
    
    }
  }
}
//#endif


BENCHMARK_REGISTER_F(CuckooMapFixture, CuckooMapDpdkBessfindInlinedGet)
 // ->Iterations(1)//41285403)
    ->RangeMultiplier(2)
    ->Range(32, 4 << 23);//18);



BENCHMARK_DEFINE_F(CuckooMapFixture, CuckooMapOrigBessfindInlinedGet)
(benchmark::State &state) {

std::pair<uint32_t,value_t> *ans;
  size_t j=0;

  while (true) {
    const size_t n = state.range(0);

    for (size_t i = 0; i < n;) 
    {

      for (j = i; ((j < n) && (j< i+32)); j++ ) 
      {
      benchmark::DoNotOptimize(ans = bess_map_->Find(*Keybulk[j])); //amar

      if(ans->second != derive_val64(*Keybulk[j]))
        throw std::runtime_error("Bess -find failed");
      }
       i=j;
//std::cout<<i<<std::endl;
      if (!state.KeepRunning()) {
#if print        
        std::cout<<i1<<std::endl;
#endif
        state.SetItemsProcessed(state.iterations());
        return;
      }
    
    }
  }
}


BENCHMARK_REGISTER_F(CuckooMapFixture, CuckooMapOrigBessfindInlinedGet)
 // ->Iterations(1)//41285403)
    ->RangeMultiplier(2)
    ->Range(32, 4 << 23);//8);



//BENCHMARK_MAIN();

int main(int argc, char** argv) {        \
     
      bess::InitDpdk(2048);\
      Key64 = new value_t[100000000];
      Keybulk =  new uint32_t*[100000000];

       rng.SetSeed(0);
      

    for (int i = 0; i < 100000000; i++) {
       Key64[i] = (uint32_t) rng.Get();
       Keybulk[i]= &Key64[i];
    }

  
    ::benchmark::Initialize(&argc, argv);  \
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1; \
    ::benchmark::RunSpecifiedBenchmarks(); \
  }
