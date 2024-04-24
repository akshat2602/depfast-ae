#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "zab_command.h"
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
        //     /* Helpers */

        //     shared_ptr<EpaxosCommand> GetCommand(uint64_t replica_id, uint64_t instance_no);
        //     void Execute(shared_ptr<EpaxosCommand> &ecmd); 

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
