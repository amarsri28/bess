#ifndef BESS_MODULES_VXLANENCAP_H_
#define BESS_MODULES_VXLANENCAP_H_

#include "../module.h"
#include "../module_msg.pb.h"

using bess::metadata::Attribute;

class VXLANEncap final : public Module {
 public:
  VXLANEncap() : Module(), dstport_() {}

  pb_error_t Init(const bess::pb::VXLANEncapArg &arg);

  virtual void ProcessBatch(bess::PacketBatch *batch);

 private:
  uint16_t dstport_;
};

#endif  // BESS_MODULES_VXLANENCAP_H_
