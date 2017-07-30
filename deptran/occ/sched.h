#pragma once

#include "../classic/sched.h"

namespace rococo {

class OCCSched: public ClassicSched {
 public:
  OCCSched();
  virtual mdb::Txn *get_mdb_txn(const i64 tid);

  virtual bool HandleConflicts(DTxn& dtxn,
                               innid_t inn_id,
                               vector<string>& conflicts) {
    verify(0);
  };
  virtual bool BeforeAccess(TxBox& tx_box, Row* row, int col_id) {
    Log_fatal("before access not implemented for occ");
  };
  virtual bool DoPrepare(txnid_t tx_id) {
    Log_fatal("doprepare not implemented for occ");
  };
};

} // namespace rococo