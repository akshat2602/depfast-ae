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

    void SaucrServer::GetState(bool *is_leader, uint64_t *epoch)
    {
        mtx_.lock();
        *epoch = current_epoch;
        *is_leader = state == ZABState::LEADER;
        mtx_.unlock();
        return;
    }
#endif

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
                Coroutine::Sleep(HEARTBEAT_INTERVAL);
                // If the server is a leader, it should send heartbeats to other servers
                // If the server does not receive a majority of acknowledgements, it should transition back to a follower
                // If the server receives a majority of acknowledgements, it should proceed forward
                if(!sendHeartbeats())
                {
                    Log_info("Stepping down as leader");
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

    /* HELPER FUNCTIONS */

    vector<vector<LogEntry>> SaucrServer::createSyncLogs(shared_ptr<SaucrNewLeaderQuorumEvent> &ev)
    {
        vector<vector<LogEntry>> syncLogs;
        auto votes = ev->GetVotes();
        auto conflict_zxids = ev->GetConflictLastSeenZxid();
        for (int i = 0; i < NSERVERS; i++)
        {
            vector<LogEntry> syncLog = {};
            if (votes[i] == SAUCR_VOTE_NOT_GRANTED || votes[i] == SAUCR_VOTE_GRANTED)
            {
                syncLogs.push_back(syncLog);
                continue;
            }
            auto p = conflict_zxids[i];
            for (int i = zxid_log_index_map[p]; i < log.size(); i++)
            {
                syncLog.push_back(commit_log[i]);
            }
            syncLogs.push_back(syncLog);
        }
        return syncLogs;
    }

    pair<uint64_t, uint64_t> SaucrServer::getLastSeenZxid()
    {
        if (commit_log.size() > 0)
        {
            auto entry = commit_log.back();
            Log_info("Last seen zxid: %lu, %lu at server: %lu with epoch %lu", entry.epoch, entry.cmd_count, loc_id_, current_epoch);
            return {entry.epoch, entry.cmd_count};
        }
        return {0, 0};
    }

    bool_t SaucrServer::sendHeartbeats()
    {
        auto ev = commo()->SendHeartbeat(partition_id_, loc_id_, loc_id_, current_epoch);
        ev->Wait(20000);
        return ev->Yes();
    }

    bool_t SaucrServer::requestVotes()
    {
        auto last_seen_zxid = getLastSeenZxid();
        auto ev = commo()->SendRequestVote(partition_id_, loc_id_, loc_id_, current_epoch, last_seen_zxid);
        ev->Wait(20000);
        if (ev->n_voted_conflict_ > 0)
        {
            Log_info("Received conflict votes");
            auto syncLog = createSyncLogs(ev);
            commo()->SendSync(partition_id_, loc_id_, loc_id_, current_epoch, ev, syncLog);
            ev->Wait(20000);
            return ev->Yes();
        }
        return ev->Yes();
    }

    bool_t SaucrServer::sendProposal(LogEntry &entry)
    {
        auto ev = commo()->SendProposal(partition_id_, loc_id_, loc_id_, current_epoch, entry);
        ev->Wait(20000);
        return ev->Yes();
    }

    bool_t SaucrServer::commitProposal(LogEntry &entry)
    {
        commit_log.push_back(entry);
        zxid_commit_log_index_map[{entry.epoch, entry.cmd_count}] = commit_log.size() - 1;
        auto ev = commo()->SendCommit(partition_id_, loc_id_, loc_id_, current_epoch, entry.epoch, entry.cmd_count);
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
        Log_info("Converted to Candidate at epoch: %lu, by server: %lu", current_epoch, loc_id_);
    }

    void SaucrServer::convertToLeader()
    {
        Log_info("Converted to Leader");
        mtx_.lock();
        heartbeat_received = true;
        state = ZABState::LEADER;
        mtx_.unlock();
        LogEntry entry;
        int cmd = 0;
        auto cmdptr = std::make_shared<TpcCommitCommand>();
        auto vpd_p = std::make_shared<VecPieceData>();
        vpd_p->sp_vec_piece_data_ = std::make_shared<vector<shared_ptr<SimpleCommand>>>();
        cmdptr->tx_id_ = cmd;
        cmdptr->cmd_ = vpd_p;
        auto cmdptr_m = dynamic_pointer_cast<Marshallable>(cmdptr);
        MarshallDeputy data(cmdptr);
        entry.data = data;
        entry.epoch = current_epoch;
        entry.cmd_count = 0;
        log.push_back(entry);
        zxid_log_index_map[{current_epoch, 0}] = log.size() - 1;
        if (sendProposal(entry))
        {
            if (commitProposal(entry))
            {
                Log_info("Committed Noop Command");
            }
            else
            {
                Log_info("Failed to commit Noop Command");
                // Step down as leader
                mtx_.lock();
                state = ZABState::FOLLOWER;
                voted_for = -1;
                mtx_.unlock();
            }
        }
    }

    /* CLIENT HANDLERS */
    void SaucrServer::HandleRequestVote(const uint64_t &c_id,
                                        const uint64_t &c_epoch,
                                        const uint64_t &last_seen_epoch,
                                        const uint64_t &last_seen_cmd_count,
                                        uint64_t *conflict_epoch,
                                        uint64_t *conflict_cmd_count,
                                        bool_t *vote_granted,
                                        bool_t *f_ok,
                                        rrr::DeferredReply *defer)
    {
        *f_ok = true;
        *vote_granted = false;
        Log_info("Received RequestVote from: %lu, with epoch: %lu, zxid: %lu, %lu, at: %lu", c_id, c_epoch, last_seen_epoch, last_seen_cmd_count, loc_id_);
        mtx_.lock();

        auto last_seen_zxid = getLastSeenZxid();
        *conflict_epoch = last_seen_zxid.first;
        *conflict_cmd_count = last_seen_zxid.second;
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

        // If the server is in sync with the candidate, vote for the candidate
        if (last_seen_epoch == last_seen_zxid.first && last_seen_cmd_count == last_seen_zxid.second)
        {
            voted_for = c_id;
            current_epoch = c_epoch;
            *vote_granted = true;
            mtx_.unlock();
            Log_info("Voted for: %lu, because last_seen_epoch and last_seen_cmd_count are equal", c_id);
            defer->reply();
            return;
        }

        // If the server has not seend a command with a higher command count, don't vote, let the server catch up using sync
        else if (last_seen_epoch == last_seen_zxid.first && last_seen_cmd_count > last_seen_zxid.second)
        {
            voted_for = c_id;
            current_epoch = c_epoch;
            heartbeat_received = true;
            *vote_granted = false;
            mtx_.unlock();
            Log_info("Voted for: %lu, because last_seen_cmd_count is greater", c_id);
            defer->reply();
            return;
        }
        // If the server has not seen a command with a higher epoch, don't vote, let the server catch up using sync
        else if (last_seen_epoch > last_seen_zxid.first)
        {
            voted_for = c_id;
            current_epoch = c_epoch;
            heartbeat_received = true;
            *vote_granted = false;
            mtx_.unlock();
            Log_info("Voted for: %lu, because last_seen_epoch is greater", c_id);
            defer->reply();
            return;
        }
        Log_info("Did not vote for: %lu", c_id);
        mtx_.unlock();
        defer->reply();
        return;
    }

    void SaucrServer::HandleSync(const uint64_t &l_id,
                                 const uint64_t &l_epoch,
                                 const vector<LogEntry> &logs,
                                 bool_t *f_ok,
                                 rrr::DeferredReply *defer)
    {
        *f_ok = true;
        Log_info("Received Sync from: %lu, with epoch: %lu at: %lu", l_id, l_epoch, loc_id_);
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
            voted_for = l_id;
        }
        // Akshat: Reason about this
        for (auto entry : logs)
        {
            auto zxid = make_pair(entry.epoch, entry.cmd_count);
            if (zxid_log_index_map.find(zxid) != zxid_log_index_map.end())
            {
                mtx_.unlock();
                *f_ok = false;
                defer->reply();
                return;
            }
            commit_log.push_back(entry);
            zxid_commit_log_index_map[zxid] = log.size() - 1;
        }
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
            // voted_for = -1;
            voted_for = l_id;
        }
        heartbeat_received = true;
        mtx_.unlock();
        defer->reply();
        return;
    }

    void SaucrServer::HandlePropose(const uint64_t &l_id,
                                    const uint64_t &l_epoch,
                                    const LogEntry &entry,
                                    bool_t *f_ok,
                                    rrr::DeferredReply *defer)
    {
        *f_ok = true;
        Log_info("Received Proposal from: %lu, with epoch: %lu at: %lu", l_id, l_epoch, loc_id_);
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
            // Akshat: Reason about whether to set voted_for to -1 here
            voted_for = l_id;
        }
        auto zxid = make_pair(entry.epoch, entry.cmd_count);
        Log_info("Accepted Proposal from: %lu, with epoch: %lu at: %lu", l_id, l_epoch, loc_id_);
        log.push_back(entry);
        zxid_log_index_map[zxid] = log.size() - 1;
        mtx_.unlock();
        defer->reply();
        return;
    }

    void SaucrServer::HandleCommit(const uint64_t &l_id,
                                   const uint64_t &l_epoch,
                                   const uint64_t &zxid_commit_epoch,
                                   const uint64_t &zxid_commit_count,
                                   bool_t *f_ok,
                                   rrr::DeferredReply *defer)
    {
        *f_ok = true;
        Log_info("Received Commit from: %lu, with epoch: %lu at: %lu", l_id, l_epoch, loc_id_);
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
            voted_for = l_id;
        }
        Log_info("Accepted Commit from: %lu, with epoch: %lu at: %lu", l_id, l_epoch, loc_id_);
        // Akshat: Reason about this
        auto idx = zxid_log_index_map[{zxid_commit_epoch, zxid_commit_count}];
        auto entry = log[idx];
        commit_log.push_back(entry);
        zxid_commit_log_index_map[{zxid_commit_epoch, zxid_commit_count}] = commit_log.size() - 1;
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