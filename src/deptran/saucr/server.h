#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../classic/tpc_command.h"
#include "commo.h"
#include "../scheduler.h"

namespace janus
{
    // Akshat: change SaucrRequest as per your requirements
    class SaucrRequest
    {
    public:
        shared_ptr<Marshallable> cmd;
        string dkey;

        SaucrRequest(shared_ptr<Marshallable> &cmd, string &dkey)
        {
            this->cmd = cmd;
            this->dkey = dkey;
        }
    };

    class SaucrServer : public TxLogServer
    {
    private:
#ifdef SAUCR_TEST_CORO
        int commit_timeout = 300000;
        int rpc_timeout = 2000000;
#else
        int commit_timeout = 10000000; // 10 seconds
        int rpc_timeout = 5000000;     // 5 seconds
#endif
        // metrics
    public:
        map<uint64_t, Timer> start_times;
        vector<double> commit_times;
        vector<double> exec_times;
        int fast = 0;
        int slow = 0;

    private:
        //     void HandleRequest(shared_ptr<Marshallable> &cmd,
        //                        string &dkey,
        //                        const function<void()> &cb);
        //     bool StartPreAccept(shared_ptr<Marshallable> &cmd,
        //                         string &dkey,
        //                         ballot_t &ballot,
        //                         uint64_t &replica_id,
        //                         uint64_t &instance_no,
        //                         int64_t leader_prev_dep_instance_no,
        //                         bool recovery);
        //     bool StartAccept(shared_ptr<Marshallable> &cmd,
        //                      string &dkey,
        //                      ballot_t &ballot,
        //                      uint64_t &seq,
        //                      map<uint64_t, uint64_t> &deps,
        //                      uint64_t &replica_id,
        //                      uint64_t &instance_no);
        //     bool StartCommit(shared_ptr<Marshallable> &cmd,
        //                      string &dkey,
        //                      uint64_t &seq,
        //                      map<uint64_t, uint64_t> &deps,
        //                      uint64_t &replica_id,
        //                      uint64_t &instance_no);
        //     bool StartTryPreAccept(shared_ptr<Marshallable> &cmd,
        //                            string &dkey,
        //                            ballot_t &ballot,
        //                            uint64_t &seq,
        //                            map<uint64_t, uint64_t> &deps,
        //                            uint64_t &replica_id,
        //                            uint64_t &instance_no,
        //                            int64_t leader_prev_dep_instance_no,
        //                            unordered_set<siteid_t> &preaccepted_sites);
        //     bool StartPrepare(uint64_t &replica_id, uint64_t &instance_no);
        //     void PrepareTillCommitted(uint64_t &replica_id, uint64_t &instance_no);
        //     void StartExecution(uint64_t &replica_id, uint64_t &instance_no);

        //     /* Helpers */

        //     shared_ptr<EpaxosCommand> GetCommand(uint64_t replica_id, uint64_t instance_no);
        //     template <class ClassT>
        //     void UpdateHighestSeenBallot(vector<ClassT> &replies, uint64_t &replica_id, uint64_t &instance_no);
        //     void GetLatestAttributes(string &dkey, uint64_t &leader_replica_id, int64_t &leader_prev_dep_instance_no, uint64_t *seq, map<uint64_t, uint64_t> *deps);
        //     void UpdateAttributes(string &dkey, uint64_t &replica_id, uint64_t &instance_no, uint64_t &seq);
        //     void MergeAttributes(vector<EpaxosPreAcceptReply> &replies, uint64_t *seq, map<uint64_t, uint64_t> *deps);
        //     vector<shared_ptr<EpaxosCommand>> GetDependencies(shared_ptr<EpaxosCommand> &ecmd);
        //     void Execute(shared_ptr<EpaxosCommand> &ecmd);
        //     void UpdateCommittedTill(uint64_t &replica_id, uint64_t &instance_no);
        //     bool AreAllDependenciesCommitted(vector<EpaxosPreAcceptReply> &replies, map<uint64_t, uint64_t> &merged_deps);
        //     unordered_set<uint64_t> GetReplicasWithAllDependenciesCommitted(map<uint64_t, uint64_t> &merged_deps);
        //     bool IsInitialBallot(ballot_t &ballot);
        //     int CompareBallots(ballot_t &b1, ballot_t &b2);
        //     ballot_t GetInitialBallot();
        //     ballot_t GetNextBallot(ballot_t &ballot);

        //     void FindTryPreAcceptConflict(shared_ptr<Marshallable> &cmd,
        //                                   string &dkey,
        //                                   uint64_t &seq,
        //                                   map<uint64_t, uint64_t> &deps,
        //                                   uint64_t &replica_id,
        //                                   uint64_t &instance_no,
        //                                   EpaxosTryPreAcceptStatus *conflict_state,
        //                                   uint64_t *conflict_replica_id,
        //                                   uint64_t *conflict_instance_no);

        //     /* RPC handlers */

        // public:
        //     void Start(shared_ptr<Marshallable> &cmd,
        //                string dkey,
        //                const function<void()> &cb);
        //     EpaxosPreAcceptReply OnPreAcceptRequest(shared_ptr<Marshallable> &cmd,
        //                                             string dkey,
        //                                             ballot_t ballot,
        //                                             uint64_t seq,
        //                                             map<uint64_t, uint64_t> deps,
        //                                             uint64_t replica_id,
        //                                             uint64_t instance_no);
        //     EpaxosAcceptReply OnAcceptRequest(shared_ptr<Marshallable> &cmd,
        //                                       string dkey,
        //                                       ballot_t ballot,
        //                                       uint64_t seq,
        //                                       map<uint64_t, uint64_t> deps,
        //                                       uint64_t replica_id,
        //                                       uint64_t instance_no);
        //     void OnCommitRequest(shared_ptr<Marshallable> &cmd,
        //                          string dkey,
        //                          uint64_t seq,
        //                          map<uint64_t, uint64_t> deps,
        //                          uint64_t replica_id,
        //                          uint64_t instance_no);
        //     EpaxosTryPreAcceptReply OnTryPreAcceptRequest(shared_ptr<Marshallable> &cmd,
        //                                                   string dkey,
        //                                                   ballot_t ballot,
        //                                                   uint64_t seq,
        //                                                   map<uint64_t, uint64_t> deps,
        //                                                   uint64_t replica_id,
        //                                                   uint64_t instance_no);
        //     EpaxosPrepareReply OnPrepareRequest(ballot_t ballot,
        //                                         uint64_t replica_id,
        //                                         uint64_t instance_no);

        /* Client request handlers */

    public:
#ifdef SAUCR_TEST_CORO
        bool Start(shared_ptr<Marshallable> &cmd, pair<uint64_t, uint64_t> *zxid);
        void GetState(bool *is_leader, uint64_t *epoch);
#endif

        /* Do not modify this class below here */

    public:
        SaucrServer(Frame *frame);
        ~SaucrServer();

    private:
        bool disconnected_ = false;
        void Setup();

    public:
        void Disconnect(const bool disconnect = true);
        void Reconnect()
        {
            Disconnect(false);
        }
        bool IsDisconnected();

        virtual bool HandleConflicts(Tx &dtxn,
                                     innid_t inn_id,
                                     vector<string> &conflicts)
        {
            verify(0);
        };

        SaucrCommo *commo()
        {
            return (SaucrCommo *)commo_;
        }
    };
} // namespace janus
