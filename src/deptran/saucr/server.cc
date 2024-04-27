#include "server.h"
#include "frame.h"
#include "coordinator.h"

namespace janus
{

    static int volatile x1 = MarshallDeputy::RegInitializer(MarshallDeputy::CMD_ZAB_COMMIT,
                                                            []() -> Marshallable *
                                                            {
                                                                return new ZABMarshallable;
                                                            });

    SaucrServer::SaucrServer(Frame *frame)
    {
        frame_ = frame;
    }

    SaucrServer::~SaucrServer() {}

    void SaucrServer::Setup()
    {
        RunSaucrServer();
    }

#ifdef SAUCR_TEST_CORO
    bool SaucrServer::Start(shared_ptr<Marshallable> &cmd, pair<uint64_t, uint64_t> *zxid)
    {
        // Akshat: Start a command on the server, send the command to followers and wait for quorum and then return zxid
        return true;
    }

#endif
    void SaucrServer::GetState(bool *is_leader, uint64_t *epoch)
    {
        mtx_.lock();
        *epoch = current_epoch;
        *is_leader = state == ZABState::LEADER;
        mtx_.unlock();
        return;
    }

    void SaucrServer::RunSaucrServer()
    {
        Coroutine::CreateRun([this]()
                             {
        while (true)
        {
            // Get the state of the server to determine what to do next
            mtx_.lock();
            auto temp_state = state;
            mtx_.unlock();
            switch (temp_state)
            {
            case ZABState::FOLLOWER:
                // If the server is a follower, it should wait for a heartbeat/command from the leader
                // If the server does not receive a heartbeat/command within the timeout, it should transition to a candidate
                Coroutine::Sleep(generate_timeout());
                mtx_.lock();
                if (!heartbeat_received)
                {
                    mtx_.unlock();
                    // Transition to candidate and request for votes
                    convertToCandidate();
                    break;
                }        
                heartbeat_received = false;
                mtx_.unlock();
                break;

            case ZABState::CANDIDATE:
                // If the server is a candidate, it should request for votes from other servers
                // If the server receives a majority of votes, it should transition to a leader
                // If the server does not receive a majority of votes, it should transition back to a follower
                if(requestVotes())
                {
                    convertToLeader();
                    break;
                }
                mtx_.lock();
                state = ZABState::FOLLOWER;
                mtx_.unlock();
                break;
            case ZABState::LEADER:
                // If the server is a leader, it should send heartbeats to other servers
                // If the server does not receive a majority of acknowledgements, it should transition back to a follower
                // If the server receives a majority of acknowledgements, it should proceed forward
                Coroutine::Sleep(HEARTBEAT_INTERVAL);
                // Send heartbeats to other servers
                if(!sendHeartbeats())
                {
                    mtx_.lock();
                    state = ZABState::FOLLOWER;
                    mtx_.unlock();
                    break;
                }
                break;

            default:
                break;
            }
        } });
    }

    bool_t SaucrServer::sendHeartbeats()
    {
        auto ev = commo()->SendHeartbeat(partition_id_, loc_id_, loc_id_, current_epoch);
        ev->Wait(20000);
        return ev->Yes();
    }

    bool_t SaucrServer::requestVotes()
    {
        auto ev = commo()->SendRequestVote(partition_id_, loc_id_, loc_id_, current_epoch, last_seen_zxid);
        ev->Wait(20000);
        return ev->Yes();
    }

    void SaucrServer::convertToCandidate()
    {
        Log_info("Converted to Candidate at epoch: %lu, by server: %lu", current_epoch, loc_id_);
        mtx_.lock();
        state = ZABState::CANDIDATE;
        current_epoch++;
        heartbeat_received = false;
        voted_for = loc_id_;
        mtx_.unlock();
    }

    void SaucrServer::convertToLeader()
    {
        Log_info("Converted to Leader");
        mtx_.lock();
        heartbeat_received = true;
        state = ZABState::LEADER;
        mtx_.unlock();
        // Akshat: Initiate a commit using a noop command so that the leader can start sending heartbeats
    }

    /* CLIENT HANDLERS */
    void SaucrServer::HandleRequestVote(const uint64_t &c_id,
                                        const uint64_t &c_epoch,
                                        const uint64_t &last_seen_epoch,
                                        const uint64_t &last_seen_cmd_count,
                                        bool_t *vote_granted,
                                        bool_t *f_ok,
                                        rrr::DeferredReply *defer)
    {
        *f_ok = true;
        *vote_granted = false;
        Log_info("Received RequestVote from: %lu, with epoch: %lu, zxid: %lu, %lu, at: %lu", c_id, c_epoch, last_seen_epoch, last_seen_cmd_count, loc_id_);
        mtx_.lock();

        // If the current epoch is less than the epoch of the candidate, do not vote
        if (c_epoch < current_epoch)
        {
            *vote_granted = false;
            mtx_.unlock();
            Log_info("Did not vote for: %lu, because c_epoch is less", c_id);
            defer->reply();
            return;
        }

        // If the current epoch is less than the epoch of the candidate, update the current epoch and transition to follower
        if (c_epoch > current_epoch)
        {
            current_epoch = c_epoch;
            voted_for = -1;
            state = ZABState::FOLLOWER;
        }

        // If the server has already voted for another candidate, do not vote
        if (voted_for != -1 && voted_for != c_id)
        {
            *vote_granted = false;
            mtx_.unlock();
            Log_info("Did not vote for: %lu, because already voted for: %lu at server: %lu", c_id, voted_for, loc_id_);
            defer->reply();
            return;
        }

        // If the server has not seen a command with a higher epoch, vote for the candidate
        if (last_seen_epoch > last_seen_zxid.first)
        {
            voted_for = c_id;
            current_epoch = c_epoch;
            heartbeat_received = true;
            *vote_granted = true;
            mtx_.unlock();
            Log_info("Voted for: %lu, because last_seen_epoch is greater", c_id);
            defer->reply();
            return;
        }
        // If the server has not seend a command with a higher command count, vote for the candidate
        else if (last_seen_epoch == last_seen_zxid.first && last_seen_cmd_count >= last_seen_zxid.second)
        {
            voted_for = c_id;
            heartbeat_received = true;
            current_epoch = c_epoch;
            *vote_granted = true;
            mtx_.unlock();
            Log_info("Voted for: %lu, because last_seen_cmd_count is greater", c_id);
            defer->reply();
            return;
        }
        Log_info("Did not vote for: %lu", c_id);
        mtx_.unlock();
        defer->reply();
        return;
    }

    void SaucrServer::HandleHeartbeat(const uint64_t &l_id,
                                      const uint64_t &l_epoch,
                                      bool_t *f_ok,
                                      rrr::DeferredReply *defer)
    {
        *f_ok = true;
        Log_info("Received Heartbeat from: %lu, with epoch: %lu at: %lu", l_id, l_epoch, loc_id_);
        mtx_.lock();
        if (l_epoch < current_epoch)
        {
            mtx_.unlock();
            *f_ok = false;
            defer->reply();
            return;
        }
        if (l_epoch > current_epoch)
        {
            current_epoch = l_epoch;
            state = ZABState::FOLLOWER;
            voted_for = -1;
        }
        heartbeat_received = true;
        mtx_.unlock();
        defer->reply();
        return;
    }

    /* Do not modify any code below here */

    void SaucrServer::Disconnect(const bool disconnect)
    {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        verify(disconnected_ != disconnect);
        // global map of rpc_par_proxies_ values accessed by partition then by site
        static map<parid_t, map<siteid_t, map<siteid_t, vector<SiteProxyPair>>>> _proxies{};
        if (_proxies.find(partition_id_) == _proxies.end())
        {
            _proxies[partition_id_] = {};
        }
        SaucrCommo *c = (SaucrCommo *)commo();
        if (disconnect)
        {
            verify(_proxies[partition_id_][loc_id_].size() == 0);
            verify(c->rpc_par_proxies_.size() > 0);
            auto sz = c->rpc_par_proxies_.size();
            _proxies[partition_id_][loc_id_].insert(c->rpc_par_proxies_.begin(), c->rpc_par_proxies_.end());
            c->rpc_par_proxies_ = {};
            verify(_proxies[partition_id_][loc_id_].size() == sz);
            verify(c->rpc_par_proxies_.size() == 0);
        }
        else
        {
            verify(_proxies[partition_id_][loc_id_].size() > 0);
            auto sz = _proxies[partition_id_][loc_id_].size();
            c->rpc_par_proxies_ = {};
            c->rpc_par_proxies_.insert(_proxies[partition_id_][loc_id_].begin(), _proxies[partition_id_][loc_id_].end());
            _proxies[partition_id_][loc_id_] = {};
            verify(_proxies[partition_id_][loc_id_].size() == 0);
            verify(c->rpc_par_proxies_.size() == sz);
        }
        disconnected_ = disconnect;
    }

    bool SaucrServer::IsDisconnected()
    {
        return disconnected_;
    }

}