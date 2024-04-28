#pragma once

#include "../__dep__.h"
#include "../communicator.h"
#include "saucr_rpc.h"

namespace janus
{

#define NSERVERS 5

#define HEARTBEAT_INTERVAL 100000
#define WIDE_AREA_DELAY 40000 + (rand() % 10000)

    class SaucrQuorumEvent : public QuorumEvent
    {
    private:
        // int fast_path_quorum_;
        // int slow_path_quorum_;

    public:
        SaucrQuorumEvent() : QuorumEvent(NSERVERS, ceil(NSERVERS / 2))
        {
            // fast_path_quorum_ = ceil(NSERVERS / 2) + 1;
            // slow_path_quorum_ = ceil(NSERVERS / 2);
        }

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
            return n_voted_yes_ >= quorum_;
        }

        bool No() override
        {
            return n_voted_no_ >= quorum_;
        }
    };

    class TxData;
    class SaucrCommo : public Communicator
    {
    public:
        SaucrCommo() = delete;
        SaucrCommo(PollMgr *);

        shared_ptr<SaucrQuorumEvent> SendRequestVote(parid_t par_id,
                                                     siteid_t site_id,
                                                     uint64_t c_id,
                                                     uint64_t c_epoch,
                                                     pair<uint64_t, uint64_t> last_seen_zxid);

        shared_ptr<SaucrQuorumEvent> SendHeartbeat(parid_t par_id,
                                                   siteid_t site_id,
                                                   uint64_t l_id,
                                                   uint64_t l_epoch);

        shared_ptr<SaucrQuorumEvent> SendProposal(parid_t par_id,
                                                  siteid_t site_id,
                                                  uint64_t l_id,
                                                  uint64_t l_epoch,
                                                  LogEntry &entry);

        shared_ptr<SaucrQuorumEvent> SendCommit(parid_t par_id,
                                                siteid_t site_id,
                                                uint64_t l_id,
                                                uint64_t l_epoch,
                                                uint64_t zxid_commit_epoch,
                                                uint64_t zxid_commit_count);

        /* Do not modify this class below here */

    public:
#if defined(SAUCR_TEST_CORO) || defined(SAUCR_PERF_TEST_CORO)
        std::recursive_mutex rpc_mtx_ = {};
        uint64_t rpc_count_ = 0;
#endif
    };

} // namespace janus
