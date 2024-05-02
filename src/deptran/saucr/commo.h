#pragma once

#include "../__dep__.h"
#include "../communicator.h"
#include "saucr_rpc.h"

namespace janus
{

#define NSERVERS 5

#define HEARTBEAT_INTERVAL 100000
#define WIDE_AREA_DELAY 40000 + (rand() % 10000)

    enum SaucrVoteResult
    {
        SAUCR_VOTE_GRANTED = 0,
        SAUCR_VOTE_NOT_GRANTED = 1,
        SAUCR_VOTE_CONFLICT = 2,
    };

    // Base class quorum events for heartbeats and proposals/commits
    class SaucrBaseQuorumEvent : public QuorumEvent
    {
    private:
        // int fast_path_quorum_;
        // int slow_path_quorum_;

    public:
        // SaucrBaseQuorumEvent() : QuorumEvent(NSERVERS, ceil(NSERVERS / 2)) {}
        SaucrBaseQuorumEvent(int n_total, int quorum) : QuorumEvent(n_total, quorum) {}

        void VoteYes()
        {
            this->QuorumEvent::VoteYes();
        }
        void VoteNo()
        {
            this->QuorumEvent::VoteNo();
        }

        // bool FastPath()
        // {
        //     return !is_recovery && ((!thrifty && n_voted_identical_ >= fast_path_quorum_) || (thrifty && all_equal && (n_voted_nonidentical_ + n_voted_identical_ >= fast_path_quorum_)));
        // }

        // bool SlowPath()
        // {
        //     return (n_voted_yes_ >= slow_path_quorum_) && (is_recovery || (!thrifty && (n_voted_nonidentical_ + n_voted_no_) > (n_total_ - fast_path_quorum_)) || (thrifty && (!all_equal || n_voted_no_ > 0)));
        // }

        bool Yes() override
        {
            return QuorumEvent::Yes();
        }

        bool No() override
        {
            return QuorumEvent::No();
        }
    };

    // This quorum event will be used for request vote specifically where syncing might be needed between the leader and the followers
    class SaucrNewLeaderQuorumEvent : public SaucrBaseQuorumEvent
    {
    public:
        SaucrNewLeaderQuorumEvent() : SaucrBaseQuorumEvent(NSERVERS, ceil(NSERVERS / 2)) {}

        int n_voted_conflict_{0};
        vector<pair<uint64_t, uint64_t>> conflict_last_seen_zxid_ = vector<pair<uint64_t, uint64_t>>(NSERVERS, make_pair(0, 0));
        vector<int> vote_granted_ = vector<int>(NSERVERS, SAUCR_VOTE_NOT_GRANTED);

        void VoteYes(int idx)
        {
            vote_granted_[idx] = SAUCR_VOTE_GRANTED;
            return SaucrBaseQuorumEvent::VoteYes();
        }
        void VoteNo()
        {
            return SaucrBaseQuorumEvent::VoteNo();
        }

        void VoteConflict(int idx)
        {
            vote_granted_[idx] = SAUCR_VOTE_CONFLICT;
            n_voted_conflict_++;
        }

        bool Yes() override
        {
            Log_info("SaucrNewLeaderQuorumEvent::Yes() n_yes_voted_ = %d", n_voted_yes_);
            return SaucrBaseQuorumEvent::Yes();
        }

        vector<pair<uint64_t, uint64_t>> GetConflictLastSeenZxid()
        {
            return conflict_last_seen_zxid_;
        }

        vector<int> GetVotes()
        {
            return vote_granted_;
        }

        bool No() override
        {
            return SaucrBaseQuorumEvent::No();
        }
    };

    class TxData;
    class SaucrCommo : public Communicator
    {
    public:
        SaucrCommo() = delete;
        SaucrCommo(PollMgr *);

        shared_ptr<SaucrNewLeaderQuorumEvent> SendRequestVote(parid_t par_id,
                                                              siteid_t site_id,
                                                              uint64_t c_id,
                                                              uint64_t c_epoch,
                                                              pair<uint64_t, uint64_t> last_seen_zxid);

        shared_ptr<SaucrBaseQuorumEvent> SendHeartbeat(parid_t par_id,
                                                       siteid_t site_id,
                                                       uint64_t l_id,
                                                       uint64_t l_epoch);

        shared_ptr<SaucrBaseQuorumEvent> SendProposal(parid_t par_id,
                                                      siteid_t site_id,
                                                      uint64_t l_id,
                                                      uint64_t l_epoch,
                                                      LogEntry &entry);

        shared_ptr<SaucrBaseQuorumEvent> SendCommit(parid_t par_id,
                                                    siteid_t site_id,
                                                    uint64_t l_id,
                                                    uint64_t l_epoch,
                                                    uint64_t zxid_commit_epoch,
                                                    uint64_t zxid_commit_count);

        void SendSync(parid_t par_id,
                      siteid_t site_id,
                      uint64_t l_id,
                      uint64_t l_epoch,
                      shared_ptr<SaucrNewLeaderQuorumEvent> ev,
                      vector<vector<LogEntry>> &logs);

        /* Do not modify this class below here */

    public:
#if defined(SAUCR_TEST_CORO) || defined(SAUCR_PERF_TEST_CORO)
        std::recursive_mutex rpc_mtx_ = {};
        uint64_t rpc_count_ = 0;
#endif
    };

} // namespace janus
