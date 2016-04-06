#pragma once

#include "../__dep__.h"
#include "../communicator.h"

namespace rococo {

class Simplecommand;
class TapirCommo : public Communicator {
 public:
  using Communicator::Communicator;

  void SendDispatch(SimpleCommand &cmd,
                    Coordinator *coo,
                    const function<void(int, Command &)> &callback);
  void BroadcastFastAccept(parid_t par_id,
                           cmdid_t cmd_id,
                           const function<void(int32_t)>& callback);
  void BroadcastDecide(parid_t,
                       cmdid_t cmd_id,
                       int32_t decision);
  void BroadcastAccept(parid_t,
                       cmdid_t,
                       ballot_t,
                       int decision,
                       const function<void(Future*)>&);
};

}
