#ifndef BESS_MODULES_QUEUEOUT_H_
#define BESS_MODULES_QUEUEOUT_H_

#include "../module.h"
#include "../module_msg.pb.h"
#include "../port.h"

class QueueOut final : public Module {
 public:
  static const gate_idx_t kNumOGates = 0;

  QueueOut() : Module(), port_(), qid_() {}

  pb_error_t Init(const bess::pb::QueueOutArg &arg);

  virtual void Deinit();

  virtual void ProcessBatch(bess::PacketBatch *batch);

  virtual std::string GetDesc() const;

 private:
  Port *port_;
  queue_t qid_;
};

#endif  // BESS_MODULES_QUEUEOUT_H_
