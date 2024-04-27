#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "zab_command.h"
#include "commo.h"
#include "../scheduler.h"

namespace janus
{

    enum ZABState
    {
        FOLLOWER = 0,
        CANDIDATE = 1,
        LEADER = 2
    };

    class SaucrServer : public TxLogServer
    {
    private:
        vector<LogEntry> log;
        uint64_t state = ZABState::FOLLOWER;
        uint64_t current_epoch = 0;
        bool_t heartbeat_received = false;
        uint64_t voted_for = -1;
        pair<uint64_t, uint64_t> last_seen_zxid = {0, 0};

        uint64_t heartbeat_timeout = HEARTBEAT_INTERVAL;
        uint64_t generate_timeout()
        {
            return (700000 + (std::rand() % (1300000 - 700000 + 1)));
        }

        void convertToCandidate();
        void convertToLeader();
        bool_t requestVotes();
        bool_t sendHeartbeats();

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
        /* Client request handlers */

        void HandleRequestVote(const uint64_t &c_id,
                               const uint64_t &c_epoch,
                               const uint64_t &last_seen_epoch,
                               const uint64_t &last_seen_cmd_count,
                               bool_t *vote_granted,
                               bool_t *f_ok,
                               rrr::DeferredReply *defer);

        void HandleHeartbeat(const uint64_t &l_id,
                             const uint64_t &l_epoch,
                             bool_t *f_ok,
                             rrr::DeferredReply *defer);

#ifdef SAUCR_TEST_CORO
        bool Start(shared_ptr<Marshallable> &cmd, pair<uint64_t, uint64_t> *zxid);
#endif
        void GetState(bool *is_leader, uint64_t *epoch);

        /* Do not modify this class below here */

    public:
        SaucrServer(Frame *frame);
        ~SaucrServer();
        void RunSaucrServer();

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
