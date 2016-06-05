#pragma once
#include "../txn_chopper.h"
#include "../dtxn.h"

#define PHASE_RCC_DISPATCH (1)
#define PHASE_RCC_COMMIT (2)

namespace rococo {
class RccDTxn: public DTxn {
 public:
  int status_ = TXN_UKN;
  vector<SimpleCommand> dreqs_ = {};
  Vertex <TxnInfo> *tv_{nullptr};
  RccGraph* graph_{nullptr};
  TxnOutput *ptr_output_repy_ = nullptr;
  TxnOutput output_ = {};
  vector<TxnInfo *> conflict_txns_ = {}; // This is read-only transaction
  function<void(int)> finish_reply_callback_ =  [] (int) -> void {verify(0);};
  bool commit_request_received_ = false;
  bool read_only_ = false;
  bool committed = false;
  bool aborted = false;
  bool __debug_replied = false;

  RccDTxn(txnid_t tid, Scheduler *mgr, bool ro);
  virtual ~RccDTxn() {
  }

  virtual void DispatchExecute(const SimpleCommand &cmd,
                               int *res,
                               map<int32_t, Value> *output);

  virtual void CommitExecute();
  virtual void Abort();

  virtual void ReplyFinishOk();

  bool ReadColumn(mdb::Row *row,
                  mdb::column_id_t col_id,
                  Value *value,
                  int hint_flag) override;

  bool WriteColumn(Row *row,
                   column_id_t col_id,
                   const Value &value,
                   int hint_flag = TXN_INSTANT) override;

  void TraceDep(Row* row, column_id_t col_id, int hint_flag);

    virtual bool start_exe_itfr(
      defer_t defer,
      TxnHandler &handler,
      const SimpleCommand& cmd,
      map<int32_t, Value> *output
  );

  // TODO remove
//  virtual void start(
//      const RequestHeader &header,
//      const std::vector <mdb::Value> &input,
//      bool *deferred,
//      ChopStartResponse *res
//  );

  virtual void start_ro(const SimpleCommand&,
                        map<int32_t, Value> &output,
                        rrr::DeferredReply *defer);

//  virtual void commit_anc_finish(
//      Vertex <TxnInfo> *v,
//      rrr::DeferredReply *defer
//  );
//
//  virtual void commit_scc_anc_commit(
//      Vertex <TxnInfo> *v,
//      rrr::DeferredReply *defer
//  );
//
//  void exe_deferred(vector <std::pair<RequestHeader,
//                                      map<int32_t, Value> > > &outputs);


  virtual mdb::Row *CreateRow(
      const mdb::Schema *schema,
      const std::vector<mdb::Value> &values) {
    return RCCRow::create(schema, values);
  }

  virtual void kiss(mdb::Row *r,
                    int col,
                    bool immediate);

  virtual bool UpdateStatus(int s) {
    if (status_ < s) {
      status_ = s;
      return true;
    } else {
      return false;
    }
  }
};
} // namespace rococo
