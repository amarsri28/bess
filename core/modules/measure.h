#ifndef BESS_MODULES_MEASURE_H_
#define BESS_MODULES_MEASURE_H_

#include "../module.h"
#include "../module_msg.pb.h"
#include "../utils/histogram.h"

class Measure final : public Module {
 public:
  static const Commands<Module> cmds;
  static const PbCommands pb_cmds;

  Measure()
      : Module(),
        hist_(),
        start_time_(),
        warmup_(),
        pkt_cnt_(),
        bytes_cnt_(),
        total_latency_() {}

  virtual struct snobj *Init(struct snobj *arg);
  pb_error_t InitPb(const bess::pb::MeasureArg &arg);

  virtual void ProcessBatch(bess::PacketBatch *batch);

  struct snobj *CommandGetSummary(struct snobj *arg);
  pb_cmd_response_t CommandGetSummaryPb(const bess::pb::EmptyArg &arg);

 private:
  struct histogram hist_;

  uint64_t start_time_;
  int warmup_; /* second */

  uint64_t pkt_cnt_;
  uint64_t bytes_cnt_;
  uint64_t total_latency_;
};

#endif  // BESS_MODULES_MEASURE_H_