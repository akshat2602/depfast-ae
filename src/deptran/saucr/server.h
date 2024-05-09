#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "zab_command.h"
#include "commo.h"
#include "../scheduler.h"
#include "../classic/tpc_command.h"
#include "persister.h"

namespace janus
{

    class StateMarshallable : public Marshallable
    {
    public:
        StateMarshallable() : Marshallable(MarshallDeputy::CMD_STATE) {}
        uint64_t current_epoch;
        int64_t voted_for;

        vector<LogEntry> commit_log;
        vector<LogEntry> log;

        Marshal &ToMarshal(Marshal &m) const override
        {
            int32_t sz = commit_log.size();
            int32_t sz1 = log.size();
            m << sz;
            for (const LogEntry &entry : commit_log)
            {
                m << entry.cmd_count;
                m << entry.data;
                m << entry.epoch;
            }
            m << sz1;
            for (const LogEntry &entry : log)
            {
                m << entry.cmd_count;
                m << entry.data;
                m << entry.epoch;
            }
            m << voted_for;
            m << current_epoch;
            return m;
        }

        Marshal &FromMarshal(Marshal &m) override
        {
            int32_t sz;
            m >> sz;
            commit_log.resize(sz, LogEntry{});
            for (LogEntry &entry : commit_log)
            {
                m >> entry.cmd_count;
                m >> entry.data;
                m >> entry.epoch;
            }
            int32_t sz1;
            m >> sz1;
            log.resize(sz1, LogEntry{});
            for (LogEntry &entry : log)
            {
                m >> entry.cmd_count;
                m >> entry.data;
                m >> entry.epoch;
            }
            m >> voted_for;
            m >> current_epoch;
            return m;
        }
    };

    enum ZABState
    {
        FOLLOWER = 0,
        CANDIDATE = 1,
        LEADER = 2
    };

    enum SAUCRState
    {
        SLOW_MODE = 0,
        FAST_MODE = 1
    };

    class SaucrServer : public TxLogServer
    {
    private:
        vector<LogEntry> log;
        vector<LogEntry> commit_log;
        uint64_t state = ZABState::FOLLOWER;
        uint64_t current_epoch = 0;
        bool_t heartbeat_received = false;
        uint64_t voted_for = -1;
        map<pair<uint64_t, uint64_t>, uint64_t> zxid_log_index_map;
        map<pair<uint64_t, uint64_t>, uint64_t> zxid_commit_log_index_map;
        uint64_t cmd_count = 1;
        bool_t stop_coroutine = false;
        uint64_t saucr_state = SAUCRState::SLOW_MODE;

        uint64_t heartbeat_timeout = HEARTBEAT_INTERVAL;

        uint64_t generate_timeout()
        {
            return (1000000 + (std::rand() % (2000000 - 1000000 + 1)));
        }
        void RunSaucrServer();
        void ProposalSender();

        /* Helper functions for the state machine */
        void convertToCandidate();
        void convertToLeader();
        vector<vector<LogEntry>> createSyncLogs(shared_ptr<SaucrNewLeaderQuorumEvent> &ev);
        pair<uint64_t, uint64_t> getLastSeenZxid();
        bool_t requestVotes();
        bool_t sendHeartbeats();
        bool_t sendProposal(LogEntry &entry);
        bool_t commitProposal(LogEntry &entry);
        void PersistState();
        void ReadPersistedState();

#ifdef SAUCR_TEST_CORO
        int commit_timeout = 300000;
        int rpc_timeout = 2000000;
#else
        int commit_timeout = 10000000; // 10 seconds
        int rpc_timeout = 5000000;     // 5 seconds
#endif
        // metrics
    public:
        shared_ptr<Persister> persister;
        map<uint64_t, Timer> start_times;
        /* Client request handlers */

        // Handles a request vote RPC from a candidate
        void HandleRequestVote(const uint64_t &c_id,
                               const uint64_t &c_epoch,
                               const uint64_t &last_seen_epoch,
                               const uint64_t &last_seen_cmd_count,
                               const uint64_t &saucr_mode,
                               uint64_t *conflict_epoch,
                               uint64_t *conflict_cmd_count,
                               bool_t *vote_granted,
                               bool_t *f_ok,
                               uint64_t *reply_epoch,
                               rrr::DeferredReply *defer);

        // Handles a heartbeat RPC from a leader, acks the heartbeat if there's no epoch mismatch else rejects it
        void HandleHeartbeat(const uint64_t &l_id,
                             const uint64_t &l_epoch,
                             const uint64_t &saucr_mode,
                             bool_t *f_ok,
                             uint64_t *reply_epoch,
                             rrr::DeferredReply *defer);

        // Handles a proposal from a leader
        void HandlePropose(const uint64_t &l_id,
                           const uint64_t &l_epoch,
                           const LogEntry &entry,
                           const uint64_t &saucr_mode,
                           bool_t *f_ok,
                           uint64_t *reply_epoch,
                           rrr::DeferredReply *defer);

        // Handles a commit from a leader
        void HandleCommit(const uint64_t &l_id,
                          const uint64_t &epoch,
                          const uint64_t &zxid_commit_epoch,
                          const uint64_t &zxid_commit_count,
                          const uint64_t &saucr_mode,
                          bool_t *f_ok,
                          uint64_t *reply_epoch,
                          rrr::DeferredReply *defer);

        // Handles a sync from a leader
        void HandleSync(const uint64_t &l_id,
                        const uint64_t &l_epoch,
                        const vector<LogEntry> &logs,
                        const uint64_t &saucr_mode,
                        bool_t *f_ok,
                        uint64_t *reply_epoch,
                        rrr::DeferredReply *defer);

#ifdef SAUCR_TEST_CORO
        bool Start(shared_ptr<Marshallable> &cmd, pair<uint64_t, uint64_t> *zxid);
        void GetState(bool *is_leader, uint64_t *epoch);
#endif

        /* Do not modify this class below here */

    public:
        SaucrServer(Frame *frame, shared_ptr<Persister> persister_);
        ~SaucrServer();

    private:
        bool disconnected_ = false;
        void Setup();
        void Shutdown();

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
