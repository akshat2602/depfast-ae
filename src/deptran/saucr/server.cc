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
        // Akshat: Send heartbeats to other servers
        return false;
    }

    bool_t SaucrServer::requestVotes()
    {
        auto ev = commo()->SendRequestVote(partition_id_, loc_id_, loc_id_, current_epoch, last_seen_zxid);
        ev->Wait(20000);
        return ev->Yes();
    }

    void SaucrServer::convertToCandidate()
    {
        mtx_.lock();
        state = ZABState::CANDIDATE;
        current_epoch++;
        heartbeat_received = false;
        voted_for = loc_id_;
        mtx_.unlock();
    }

    void SaucrServer::convertToLeader()
    {
        mtx_.lock();
        heartbeat_received = true;
        state = ZABState::LEADER;
        mtx_.unlock();
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