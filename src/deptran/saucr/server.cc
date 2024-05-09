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
    static int volatile x2 = MarshallDeputy::RegInitializer(MarshallDeputy::CMD_STATE,
                                                            []() -> Marshallable *
                                                            {
                                                                return new StateMarshallable;
                                                            });

    SaucrServer::SaucrServer(Frame *frame, shared_ptr<Persister> persister_)
    {
        persister = persister_;
        frame_ = frame;
    }

    SaucrServer::~SaucrServer() {}

    void SaucrServer::Shutdown()
    {
        this->mtx_.lock();
        this->stop_coroutine = true;
        this->mtx_.unlock();
        Log_debug("Shutting down");
        auto ev = Reactor::CreateSpEvent<TimeoutEvent>(2500000);
        ev->Wait();
        this->PersistState();
        return;
    }

    void SaucrServer::Setup()
    {
        size_t temp = this->persister->SaucrStateSize();
        if (temp != -1)
        {
            this->ReadPersistedState();
        }
        RunSaucrServer();
        ProposalSender();
    }

#ifdef SAUCR_TEST_CORO
    bool SaucrServer::Start(shared_ptr<Marshallable> &cmd, pair<uint64_t, uint64_t> *zxid)
    {
        bool is_leader;
        uint64_t temp_epoch;
        this->GetState(&is_leader, &temp_epoch);
        if (!is_leader)
        {
            return false;
        }
        // Log_info("Locking mutex");
        this->mtx_.lock();
        LogEntry entry;
        auto zabmarshal = dynamic_pointer_cast<ZABMarshallable>(cmd);
        zabmarshal->zxid = make_pair(current_epoch, cmd_count);
        auto cmdptr_m = dynamic_pointer_cast<Marshallable>(zabmarshal);
        MarshallDeputy cmd_deputy(cmdptr_m);
        entry.epoch = current_epoch;
        entry.cmd_count = cmd_count;
        entry.data = cmd_deputy;
        *zxid = make_pair(entry.epoch, entry.cmd_count);
        log.push_back(entry);
        zxid_log_index_map[{entry.epoch, entry.cmd_count}] = log.size() - 1;
        cmd_count++;
        // Log_info("Unlocking mutex");
        this->mtx_.unlock();
        this->PersistState();
        Log_info("Starting command at epoch: %lu, cmd_count: %lu", entry.epoch, entry.cmd_count);
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

    void SaucrServer::PersistState()
    {
        auto curr_saucrstate = make_shared<StateMarshallable>();
        // Log_debug("Locking mutex");
        this->mtx_.lock();
        curr_saucrstate->log = this->log;
        curr_saucrstate->commit_log = this->commit_log;
        curr_saucrstate->voted_for = this->voted_for;
        curr_saucrstate->current_epoch = this->current_epoch;
        // Log_debug("Unlocking mutex");
        this->mtx_.unlock();
        shared_ptr<Marshallable> m = curr_saucrstate;
        this->persister->SaveSaucrState(m);
        return;
    }

    void SaucrServer::ReadPersistedState()
    {
        auto curr_saucrstate = make_shared<StateMarshallable>();
        shared_ptr<Marshallable> m = curr_saucrstate;
        m = this->persister->ReadSaucrState();
        curr_saucrstate = dynamic_pointer_cast<StateMarshallable>(m);
        // Log_debug("Locking mutex");
        this->mtx_.lock();
        this->log = curr_saucrstate->log;
        this->commit_log = curr_saucrstate->commit_log;
        this->voted_for = curr_saucrstate->voted_for;
        this->current_epoch = curr_saucrstate->current_epoch;
        // Log_debug("Unlocking mutex");
        this->mtx_.unlock();
        return;
    }

    // Check if there are any new proposals to propose
    // To do this, get the last seen zxid and check if there are any new commands to propose,
    // if there are, propose them only if their epoch is the same as the current epoch
    // If the epoch is different, don't propose.
    void SaucrServer::ProposalSender()
    {
        Coroutine::CreateRun([this]()
                             {
            while(!this->stop_coroutine)
            {
                bool is_leader;
                uint64_t temp_epoch;
                this->GetState(&is_leader, &temp_epoch);
                if (!is_leader)
                {
                    Coroutine::Sleep(HEARTBEAT_INTERVAL);
                    continue;
                }
                auto last_seen_zxid = getLastSeenZxid();
                auto idx = zxid_log_index_map[last_seen_zxid];
                for (int i = idx + 1; i < log.size(); i++)
                {
                    auto entry = log[i];
                    if (entry.epoch == current_epoch && !dynamic_pointer_cast<ZABMarshallable>(entry.data.sp_data_)->is_noop)
                    {
                        if (sendProposal(entry))
                        {
                            Log_info("Sent Proposal");
                            if (commitProposal(entry))
                            {
                                std::shared_ptr<Marshallable> cmd = const_cast<MarshallDeputy &>(entry.data).sp_data_;
                                app_next_(*cmd);
                                Log_info("Committed Proposal");
                            }
                        }
                        this->PersistState();
                    }
                }

                Coroutine::Sleep(HEARTBEAT_INTERVAL);
            } });
    }

    void SaucrServer::RunSaucrServer()
    {
        Coroutine::CreateRun([this]()
                             {
        while (!this->stop_coroutine)
        {
            // Get the state of the server to determine what to do next
            mtx_.lock();
            auto temp_state = state;
            mtx_.unlock();
            switch (temp_state)
            {
            case ZABState::FOLLOWER:
                Log_info("Follower at server: %lu", loc_id_);
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
                Log_info("Candidate at server: %lu", loc_id_);
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
                Log_info("Leader at server: %lu", loc_id_);
                Coroutine::Sleep(HEARTBEAT_INTERVAL);
                // If the server is a leader, it should send heartbeats to other servers
                // If the server receives a majority of acknowledgements, it should proceed forward
                if(!sendHeartbeats())
                {
                    Log_info("SUSSY SUSSY BAKA, PROBABLY DISCONNECTED BUT TEST IS SUSSY SUSSY");
                    // Log_info("Stepping down as leader at server: %lu", loc_id_);
                    // mtx_.lock();
                    // state = ZABState::FOLLOWER;
                    // mtx_.unlock();
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
            for (int i = zxid_commit_log_index_map[p] + 1; i < commit_log.size(); i++)
            {
                syncLog.push_back(commit_log[i]);
            }
            syncLogs.push_back(syncLog);
        }
        return syncLogs;
    }

    pair<uint64_t, uint64_t> SaucrServer::getLastSeenZxid()
    {
        pair<uint64_t, uint64_t> last_seen_zxid = {0, 0};
        mtx_.lock();
        if (commit_log.size() > 0)
        {
            auto entry = commit_log.back();
            Log_info("Last seen zxid: %lu, %lu at server: %lu with epoch %lu", entry.epoch, entry.cmd_count, loc_id_, current_epoch);
            last_seen_zxid = {entry.epoch, entry.cmd_count};
        }
        mtx_.unlock();
        return last_seen_zxid;
    }

    bool_t SaucrServer::sendHeartbeats()
    {
        auto ev = commo()->SendHeartbeat(partition_id_, loc_id_, loc_id_, current_epoch, saucr_state);
        ev->Wait(20000);
        for (auto reply_epoch : ev->reply_epochs_)
        {
            if (reply_epoch > current_epoch)
            {
                Log_info("Received higher epoch %lu", reply_epoch);
                mtx_.lock();
                state = ZABState::FOLLOWER;
                voted_for = -1;
                Log_info("Changing epoch from %lu to %lu", current_epoch, reply_epoch);
                current_epoch = reply_epoch;
                heartbeat_received = false;
                mtx_.unlock();
                this->PersistState();
                return ev->No();
            }
        }
        return ev->Yes();
    }

    bool_t SaucrServer::requestVotes()
    {
        auto last_seen_zxid = getLastSeenZxid();
        auto ev = commo()->SendRequestVote(partition_id_, loc_id_, loc_id_, current_epoch, last_seen_zxid, saucr_state);
        ev->Wait(20000);
        if (ev->n_voted_conflict_ > 0)
        {
            Log_info("Received conflict votes");
            auto syncLog = createSyncLogs(ev);
            commo()->SendSync(partition_id_, loc_id_, loc_id_, current_epoch, ev, syncLog, saucr_state);
            ev->Wait(20000);
            for (auto reply_epoch : ev->reply_epochs_)
            {
                if (reply_epoch > current_epoch)
                {
                    mtx_.lock();
                    state = ZABState::FOLLOWER;
                    voted_for = -1;
                    Log_info("Changing epoch from %lu to %lu", current_epoch, reply_epoch);
                    current_epoch = reply_epoch;
                    mtx_.unlock();
                    this->PersistState();
                    return ev->No();
                }
            }
            return ev->Yes();
        }
        for (auto reply_epoch : ev->reply_epochs_)
        {
            if (reply_epoch > current_epoch)
            {
                mtx_.lock();
                state = ZABState::FOLLOWER;
                voted_for = -1;
                Log_info("Changing epoch from %lu to %lu", current_epoch, reply_epoch);
                current_epoch = reply_epoch;
                mtx_.unlock();
                this->PersistState();
                return ev->No();
            }
        }
        return ev->Yes();
    }

    bool_t SaucrServer::sendProposal(LogEntry &entry)
    {
        auto ev = commo()->SendProposal(partition_id_, loc_id_, loc_id_, current_epoch, entry, saucr_state);
        ev->Wait(20000);
        for (auto reply_epoch : ev->reply_epochs_)
        {
            if (reply_epoch > current_epoch)
            {
                mtx_.lock();
                state = ZABState::FOLLOWER;
                voted_for = -1;
                Log_info("Changing epoch from %lu to %lu", current_epoch, reply_epoch);
                current_epoch = reply_epoch;
                mtx_.unlock();
                this->PersistState();
                return ev->No();
            }
        }
        Log_info("Sent Proposal from: %lu, with epoch: %lu", loc_id_, current_epoch);
        return ev->Yes();
    }

    bool_t SaucrServer::commitProposal(LogEntry &entry)
    {
        commit_log.push_back(entry);
        zxid_commit_log_index_map[{entry.epoch, entry.cmd_count}] = commit_log.size() - 1;
        auto ev = commo()->SendCommit(partition_id_, loc_id_, loc_id_, current_epoch, entry.epoch, entry.cmd_count, saucr_state);
        ev->Wait(20000);
        // Check if any server has rejected the commit due to a epoch conflict
        for (auto reply_epoch : ev->reply_epochs_)
        {
            if (reply_epoch > current_epoch)
            {
                mtx_.lock();
                state = ZABState::FOLLOWER;
                voted_for = -1;
                Log_info("Changing epoch from %lu to %lu", current_epoch, reply_epoch);
                current_epoch = reply_epoch;
                mtx_.unlock();
                this->PersistState();
                return ev->No();
            }
        }
        Log_info("Sent Commit from: %lu, with epoch: %lu", loc_id_, current_epoch);
        return ev->Yes();
    }

    void SaucrServer::convertToCandidate()
    {
        mtx_.lock();
        state = ZABState::CANDIDATE;
        current_epoch = current_epoch + 1;
        heartbeat_received = true;
        voted_for = loc_id_;
        mtx_.unlock();
        this->PersistState();
        Log_info("Converted to Candidate at epoch: %lu, by server: %lu", current_epoch, loc_id_);
    }

    void SaucrServer::convertToLeader()
    {
        Log_info("Converted to Leader");
        mtx_.lock();
        heartbeat_received = true;
        state = ZABState::LEADER;
        cmd_count = 1;
        mtx_.unlock();
        LogEntry entry;
        int cmd = 0;
        auto cmdptr = std::make_shared<ZABMarshallable>();
        cmdptr->cmd = cmd;
        cmdptr->is_noop = true;
        cmdptr->zxid = make_pair(current_epoch, cmd_count);
        auto cmdptr_m = dynamic_pointer_cast<Marshallable>(cmdptr);
        MarshallDeputy data(cmdptr);
        entry.data = data;
        entry.epoch = current_epoch;
        entry.cmd_count = 0;
        log.push_back(entry);
        zxid_log_index_map[{entry.epoch, entry.cmd_count}] = log.size() - 1;
        if (sendProposal(entry))
        {
            Log_info("Sent Noop Command");
            if (commitProposal(entry))
            {
                Log_info("Committed Noop Command");
            }
            this->PersistState();
        }
    }

    /* CLIENT HANDLERS */
    void SaucrServer::HandleRequestVote(const uint64_t &c_id,
                                        const uint64_t &c_epoch,
                                        const uint64_t &last_seen_epoch,
                                        const uint64_t &last_seen_cmd_count,
                                        const uint64_t &saucr_mode,
                                        uint64_t *conflict_epoch,
                                        uint64_t *conflict_cmd_count,
                                        bool_t *vote_granted,
                                        bool_t *f_ok,
                                        uint64_t *reply_epoch,
                                        rrr::DeferredReply *defer)
    {
        if (this->stop_coroutine)
        {
            Log_info("REPLY SHUTTING DOWN");
            *f_ok = false;
            *vote_granted = false;
            *conflict_epoch = 0;
            *conflict_cmd_count = 0;
            *reply_epoch = 0;
            defer->reply();
            return;
        }
        *f_ok = true;
        *vote_granted = false;
        Log_info("Received RequestVote from: %lu, with epoch: %lu, zxid: %lu, %lu, at: %lu", c_id, c_epoch, last_seen_epoch, last_seen_cmd_count, loc_id_);
        auto last_seen_zxid = getLastSeenZxid();
        mtx_.lock();
        *conflict_epoch = last_seen_zxid.first;
        *conflict_cmd_count = last_seen_zxid.second;
        // If the current epoch is less than the epoch of the candidate, do not vote
        if (c_epoch < current_epoch)
        {
            *vote_granted = false;
            *reply_epoch = current_epoch;
            mtx_.unlock();
            Log_info("Did not vote for: %lu, because c_epoch is less", c_id);
            defer->reply();
            return;
        }

        // If the current epoch is less than the epoch of the candidate, update the current epoch and transition to follower
        if (c_epoch > current_epoch)
        {
            Log_info("Changing epoch from %lu to %lu", current_epoch, c_epoch);
            current_epoch = c_epoch;
            voted_for = -1;
            state = ZABState::FOLLOWER;
        }
        *reply_epoch = current_epoch;
        // If the server has already voted for another candidate, do not vote
        if (voted_for != -1 && voted_for != c_id)
        {
            *vote_granted = false;
            mtx_.unlock();
            Log_info("Did not vote for: %lu, because already voted for: %lu at server: %lu", c_id, voted_for, loc_id_);
            this->PersistState();
            defer->reply();
            return;
        }

        // If the server is in sync with the candidate, vote for the candidate
        if (last_seen_epoch == last_seen_zxid.first && last_seen_cmd_count == last_seen_zxid.second)
        {
            voted_for = c_id;
            Log_info("Changing epoch from %lu to %lu", current_epoch, c_epoch);
            current_epoch = c_epoch;
            *vote_granted = true;
            mtx_.unlock();
            Log_info("Voted for: %lu, because last_seen_epoch and last_seen_cmd_count are equal", c_id);
            this->PersistState();
            defer->reply();
            return;
        }

        // If the server has not seend a command with a higher command count, don't vote, let the server catch up using sync
        else if (last_seen_epoch == last_seen_zxid.first && last_seen_cmd_count > last_seen_zxid.second)
        {
            voted_for = c_id;
            current_epoch = c_epoch;
            Log_info("Changing epoch from %lu to %lu", current_epoch, c_epoch);
            heartbeat_received = true;
            *vote_granted = false;
            mtx_.unlock();
            Log_info("Voted for: %lu, because last_seen_cmd_count is greater", c_id);
            this->PersistState();
            defer->reply();
            return;
        }
        // If the server has not seen a command with a higher epoch, don't vote, let the server catch up using sync
        else if (last_seen_epoch > last_seen_zxid.first)
        {
            voted_for = c_id;
            current_epoch = c_epoch;
            Log_info("Changing epoch from %lu to %lu", current_epoch, c_epoch);
            heartbeat_received = true;
            *vote_granted = false;
            mtx_.unlock();
            Log_info("Voted for: %lu, because last_seen_epoch is greater", c_id);
            this->PersistState();
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
                                 const uint64_t &saucr_mode,
                                 bool_t *f_ok,
                                 uint64_t *reply_epoch,
                                 rrr::DeferredReply *defer)
    {
        if (this->stop_coroutine)
        {
            Log_info("REPLY SHUTTING DOWN");
            *reply_epoch = 0;
            *f_ok = false;
            defer->reply();
            return;
        }
        *f_ok = true;
        Log_info("Received Sync from: %lu, with epoch: %lu at: %lu", l_id, l_epoch, loc_id_);
        mtx_.lock();
        if (l_epoch < current_epoch)
        {
            *reply_epoch = current_epoch;
            mtx_.unlock();
            *f_ok = false;
            defer->reply();
            return;
        }
        if (l_epoch > current_epoch)
        {
            Log_info("Changing epoch from %lu to %lu", current_epoch, l_epoch);
            current_epoch = l_epoch;
            state = ZABState::FOLLOWER;
            voted_for = l_id;
        }
        for (auto entry : logs)
        {
            auto zxid = make_pair(entry.epoch, entry.cmd_count);
            commit_log.push_back(entry);
            zxid_commit_log_index_map[zxid] = log.size() - 1;
        }
        *reply_epoch = current_epoch;
        mtx_.unlock();
        this->PersistState();
        defer->reply();
        return;
    }

    void SaucrServer::HandleHeartbeat(const uint64_t &l_id,
                                      const uint64_t &l_epoch,
                                      const uint64_t &saucr_mode,
                                      bool_t *f_ok,
                                      uint64_t *reply_epoch,
                                      rrr::DeferredReply *defer)
    {
        if (this->stop_coroutine)
        {
            Log_info("REPLY SHUTTING DOWN");
            *reply_epoch = 0;
            *f_ok = false;
            defer->reply();
            return;
        }
        *f_ok = true;
        // Log_info("Received Heartbeat from: %lu, with epoch: %lu at: %lu", l_id, l_epoch, loc_id_);
        mtx_.lock();
        if (l_epoch < current_epoch)
        {
            *reply_epoch = current_epoch;
            mtx_.unlock();
            *f_ok = false;
            defer->reply();
            return;
        }
        if (l_epoch > current_epoch)
        {
            Log_info("Changing epoch from %lu to %lu", current_epoch, l_epoch);
            current_epoch = l_epoch;
            state = ZABState::FOLLOWER;
            // voted_for = -1;
            voted_for = l_id;
        }
        *reply_epoch = current_epoch;
        heartbeat_received = true;
        mtx_.unlock();
        this->PersistState();
        defer->reply();
        return;
    }

    void SaucrServer::HandlePropose(const uint64_t &l_id,
                                    const uint64_t &l_epoch,
                                    const LogEntry &entry,
                                    const uint64_t &saucr_mode,
                                    bool_t *f_ok,
                                    uint64_t *reply_epoch,
                                    rrr::DeferredReply *defer)
    {
        if (this->stop_coroutine)
        {
            Log_info("REPLY SHUTTING DOWN");
            *reply_epoch = 0;
            *f_ok = false;
            defer->reply();
            return;
        }
        *f_ok = true;
        Log_info("Received Proposal from: %lu, with epoch: %lu at: %lu", l_id, l_epoch, loc_id_);
        mtx_.lock();
        if (l_epoch < current_epoch)
        {
            *reply_epoch = current_epoch;
            mtx_.unlock();
            *f_ok = false;
            defer->reply();
            return;
        }
        if (l_epoch > current_epoch)
        {
            Log_info("Changing epoch from %lu to %lu", current_epoch, l_epoch);
            current_epoch = l_epoch;
            state = ZABState::FOLLOWER;
            // Akshat: Reason about whether to set voted_for to -1 here
            voted_for = l_id;
        }
        *reply_epoch = current_epoch;
        auto zxid = make_pair(entry.epoch, entry.cmd_count);
        Log_info("Accepted Proposal from: %lu, with epoch: %lu at: %lu", l_id, l_epoch, loc_id_);
        log.push_back(entry);
        zxid_log_index_map[zxid] = log.size() - 1;
        mtx_.unlock();
        this->PersistState();
        defer->reply();
        return;
    }

    void SaucrServer::HandleCommit(const uint64_t &l_id,
                                   const uint64_t &l_epoch,
                                   const uint64_t &zxid_commit_epoch,
                                   const uint64_t &zxid_commit_count,
                                   const uint64_t &saucr_mode,
                                   bool_t *f_ok,
                                   uint64_t *reply_epoch,
                                   rrr::DeferredReply *defer)
    {
        if (this->stop_coroutine)
        {
            Log_info("REPLY SHUTTING DOWN");
            *reply_epoch = 0;
            *f_ok = false;
            defer->reply();
            return;
        }
        *f_ok = true;
        Log_info("Received Commit from: %lu, with epoch: %lu at: %lu", l_id, l_epoch, loc_id_);
        mtx_.lock();
        if (l_epoch < current_epoch)
        {
            *reply_epoch = current_epoch;
            mtx_.unlock();
            *f_ok = false;
            defer->reply();
            return;
        }
        if (l_epoch > current_epoch)
        {
            Log_info("Changing epoch from %lu to %lu", current_epoch, l_epoch);
            current_epoch = l_epoch;
            state = ZABState::FOLLOWER;
            voted_for = l_id;
        }
        *reply_epoch = current_epoch;
        Log_info("Accepted Commit from: %lu, with epoch: %lu at: %lu", l_id, l_epoch, loc_id_);
        // Akshat: Handle the case where the commit is not in the log
        auto idx = zxid_log_index_map[{zxid_commit_epoch, zxid_commit_count}];
        auto entry = log[idx];
        commit_log.push_back(entry);
        zxid_commit_log_index_map[{zxid_commit_epoch, zxid_commit_count}] = commit_log.size() - 1;
        mtx_.unlock();
        shared_ptr<Marshallable> cmd = const_cast<MarshallDeputy &>(entry.data).sp_data_;
        // Only execute the command if the command is not a noop
        Log_info("Command noop: %d", dynamic_pointer_cast<ZABMarshallable>(cmd)->is_noop);
        if (!dynamic_pointer_cast<ZABMarshallable>(cmd)->is_noop)
        {
            Log_info("Executing command");
            app_next_(*cmd);
        }
        this->PersistState();
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