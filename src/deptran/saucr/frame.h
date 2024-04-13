#pragma once

#include "../communicator.h"
#include "../frame.h"
#include "../constants.h"
#include "commo.h"
#include "server.h"
#include "coordinator.h"

namespace janus {

class SaucrFrame : public Frame {
 private:
  #ifdef SAUCR_TEST_CORO
  static std::mutex saucr_test_mutex_;
  static uint16_t n_replicas_;
  #endif

 public:
  #ifdef SAUCR_TEST_CORO
  static SaucrFrame *replicas_[NSERVERS];
  #endif
  SaucrCommo *commo_ = nullptr;
  SaucrServer *svr_ = nullptr;

  SaucrFrame(int mode);
  virtual ~SaucrFrame();

  Coordinator *CreateCoordinator(cooid_t coo_id,
                                 Config *config,
                                 int benchmark,
                                 ClientControlServiceImpl *ccsi,
                                 uint32_t id,
                                 shared_ptr<TxnRegistry> txn_reg);

  TxLogServer *CreateScheduler() override;
  
  Communicator *CreateCommo(PollMgr *poll = nullptr) override;
  
  vector<rrr::Service *> CreateRpcServices(uint32_t site_id,
                                           TxLogServer *dtxn_sched,
                                           rrr::PollMgr *poll_mgr,
                                           ServerControlServiceImpl *scsi) override;

  void setupCoordinator(SaucrCoordinator *coord, Config *config);
};

} // namespace janus
