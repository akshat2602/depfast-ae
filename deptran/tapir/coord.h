#pragma once

#include "../none/coord.h"

namespace rococo {

#define MAGIC_FACCEPT_BALLOT 1;
#define MAGIC_SACCEPT_BALLOT 2;

class TapirCommo;
class TapirCoord : public ClassicCoord {
 public:
  enum Phase {INIT_END, DISPATCH, FAST_ACCEPT, DECIDE};
  enum Decision { UNKNOWN, COMMIT, ABORT};
  Decision decision_ = UNKNOWN;
  map<parid_t, int> n_accept_oks_ = {};
  map<parid_t, int> n_accpet_rejects_ = {};
  map<parid_t, int> n_fast_accept_oks_ = {};
  map<parid_t, int> n_fast_accept_rejects_ = {};
  using ClassicCoord::ClassicCoord;

//  void do_one(TxnRequest &) override;
  void Reset() override;
  TapirCommo* commo();

  void Dispatch() override;
  void DispatchAck(phase_t, int32_t res, ContainerCommand &cmd) override;

  void FastAccept();
  void FastAcceptAck(phase_t phase, parid_t par_id, int32_t res);

  bool AllFastQuorumReached();
  bool AllSlowQuorumReached();

  // either commit or abort.
  void Decide();

  void Accept();
  void AcceptAck(phase_t phase, parid_t par_id, Future *fu);

  void Restart() override;
  void GotoNextPhase() override;

  int GetFastQuorum(parid_t par_id);
  int GetSlowQuorum(parid_t par_id);

  bool FastQuorumPossible();
};

} // namespace rococo