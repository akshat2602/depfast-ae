#include "testconf.h"
#include "marshallable.h"
#include "zab_command.h"

namespace janus
{

#ifdef SAUCR_TEST_CORO

    int _test_id_g = 0;
    std::chrono::_V2::system_clock::time_point _test_starttime_g;

    string map_to_string(map<uint64_t, uint64_t> m)
    {
        string result = "";
        for (auto itr : m)
        {
            result += to_string(itr.first) + ":" + to_string(itr.second) + ", ";
        }
        result = result.substr(0, result.size() - 2);
        return result;
    }

    string pair_to_string(pair<uint64_t, uint64_t> p)
    {
        return to_string(p.first) + ":" + to_string(p.second);
    }

    string vector_to_string(vector<uint64_t> exec_orders)
    {
        string s = "";
        for (int i = 0; i < exec_orders.size(); i++)
        {
            s += to_string(exec_orders[i]) + ", ";
        }
        return s;
    }

    SaucrFrame **SaucrTestConfig::replicas = nullptr;
    std::vector<int> SaucrTestConfig::committed_cmds[NSERVERS];
    std::vector<unordered_map<string, int>> SaucrTestConfig::committed_zxids;
    pair<uint64_t, uint64_t> SaucrTestConfig::last_committed_zxid;
    uint64_t SaucrTestConfig::rpc_count_last[NSERVERS];

    SaucrTestConfig::SaucrTestConfig(SaucrFrame **replicas_)
    {
        verify(replicas == nullptr);
        replicas = replicas_;
        for (int i = 0; i < NSERVERS; i++)
        {
            replicas[i]->svr_->rep_frame_ = replicas[i]->svr_->frame_;
            SaucrTestConfig::committed_cmds[i].push_back(-1);
            SaucrTestConfig::committed_zxids.push_back(unordered_map<string, int>());
            SaucrTestConfig::rpc_count_last[i] = 0;
            SaucrTestConfig::disconnected_[i] = false;
            SaucrTestConfig::slow_[i] = false;
        }
        th_ = std::thread([this]()
                          { netctlLoop(); });
    }

    void SaucrTestConfig::SetLearnerAction(void)
    {
        for (int i = 0; i < NSERVERS; i++)
        {
            replicas[i]->svr_->RegLearnerAction([i](Marshallable &cmd)
                                                {
                    verify(cmd.kind_ == MarshallDeputy::CMD_ZAB_COMMIT);
                    auto& command = dynamic_cast<ZABMarshallable&>(cmd);
                    Log_debug("server %d committed value %d at zxid.epoch %d and zxid.transaction_id %d", i, command.cmd, command.zxid.first, command.zxid.second);
                    SaucrTestConfig::committed_cmds[i].push_back(command.cmd); 
                    SaucrTestConfig::committed_zxids[i][pair_to_string(command.zxid)] = SaucrTestConfig::committed_cmds[i].size() - 1; 
                    SaucrTestConfig::last_committed_zxid = command.zxid; });
        }
    }

    // void SaucrTestConfig::SetCustomLearnerAction(function<function<void(Marshallable &)>(int)> callback)
    // {
    //     for (int i = 0; i < NSERVERS; i++)
    //     {
    //         replicas[i]->svr_->RegLearnerAction(callback(i));
    //     }
    // }

    // #ifdef SAUCR_PERF_TEST_CORO
    //     void SaucrTestConfig::SetCommittedLearnerAction(function<function<void(Marshallable &)>(int)> callback)
    //     {
    //         for (int i = 0; i < NSERVERS; i++)
    //         {
    //             replicas[i]->svr_->RegCommitLearnerAction(callback(i));
    //         }
    //     }
    // #endif

    bool SaucrTestConfig::Start(int svr, int cmd, pair<uint64_t, uint64_t> *zxid)
    {
        // Construct an empty ZABMarshallableCmd containing cmd as its tx_id_
        auto cmdptr = std::make_shared<ZABMarshallable>();
        cmdptr->cmd = cmd;
        auto cmdptr_m = dynamic_pointer_cast<Marshallable>(cmdptr);
        // call Start()
        Log_debug("Starting agreement on svr %d for cmd id %d", svr, cmdptr->cmd);
        return replicas[svr]->svr_->Start(cmdptr_m, zxid);
    }

    void SaucrTestConfig::GetState(int svr,
                                   bool *is_leader,
                                   uint64_t *epoch)
    {
        replicas[svr]->svr_->GetState(is_leader, epoch);
    }

    vector<int> SaucrTestConfig::GetCommitted(int svr)
    {
        return committed_cmds[svr];
    }

    pair<uint64_t, uint64_t> SaucrTestConfig::DoAgreement(int cmd, int n, bool retry)
    {
        Log_debug("Doing 1 round of Saucr agreement");
        auto start = chrono::steady_clock::now();
        while ((chrono::steady_clock::now() - start) < chrono::seconds{10})
        {
            usleep(50000);
            // Coroutine::Sleep(50000);
            // Call Start() to all servers until leader is found
            int ldr = -1;
            pair<uint64_t, uint64_t> zxid;
            for (int i = 0; i < NSERVERS; i++)
            {
                // skip disconnected servers
                if (SaucrTestConfig::IsDisconnected(i))
                    continue;
                if (Start(i, cmd, &zxid))
                {
                    Log_debug("starting cmd at line 130");
                    ldr = i;
                    break;
                }
            }
            if (ldr != -1)
            {
                // If Start() successfully called, wait for agreement
                auto start2 = chrono::steady_clock::now();
                int nc;
                while ((chrono::steady_clock::now() - start2) < chrono::seconds{10})
                {
                    nc = NCommitted(zxid);
                    if (nc < 0)
                    {
                        break;
                    }
                    else if (nc >= n)
                    {
                        Log_debug("reached agreement");
                        for (int i = 0; i < NSERVERS; i++)
                        {
                            Log_debug("in here 157");
                            auto it = committed_zxids[i].find(pair_to_string(zxid));
                            if (it != committed_zxids[i].end())
                            {
                                // Key found
                                Log_debug("found committed log at server %d", i);
                                auto idx = it->second;
                                if (committed_cmds[i][idx] == cmd)
                                {
                                    Log_debug("committed value is the same as given");
                                    return zxid;
                                }
                                break;
                            }
                        }
                        break;
                    }
                    usleep(20000);
                    // Coroutine::Sleep(50000);
                }
                Log_debug("%d committed server at index %d", nc, zxid.second);
                if (!retry)
                {
                    Log_debug("failed to reach agreement");
                    return std::make_pair(0, 0);
                }
            }
            else
            {
                // If no leader found, sleep and retry.
                usleep(50000);
                // Coroutine::Sleep(50000);
            }
        }
        Log_debug("Failed to reach agreement end");
        return std::make_pair(0, 0);
    }

    int SaucrTestConfig::NCommitted(pair<uint64_t, uint64_t> zxid)
    {
        int count = 0;
        string searchString = to_string(zxid.first) + ":" + to_string(zxid.second);
        for (int i = 0; i < NSERVERS; i++)
        {
            if (committed_zxids[i].find(searchString) != committed_zxids[i].end())
            {
                Log_debug("committed_zxid %d", committed_zxids[i][searchString]);
                Log_debug("found committed log at server %d", i);
                count++;
            }
        }
        return count;
    }

    int SaucrTestConfig::Wait(pair<uint64_t, uint64_t> zxid, int n, uint64_t epoch)
    {
        int nc = 0, i;
        auto to = 10000; // 10 milliseconds
        for (i = 0; i < 30; i++)
        {
            nc = NCommitted(zxid);
            if (nc < 0)
            {
                return -3; // values differ
            }
            else if (nc >= n)
            {
                break;
            }
            Reactor::CreateSpEvent<TimeoutEvent>(to)->Wait();
            if (to < 1000000)
            {
                to *= 2;
            }
            if (EpochMovedOn(epoch))
            {
                return -2; // term changed
            }
        }
        if (i == 30)
        {
            return -1; // timeout
        }
        for (int i = 0; i < NSERVERS; i++)
        {
            // Return the committed value for the given zxid
            auto it = committed_zxids[i].find(pair_to_string(zxid));
            if (it != committed_zxids[i].end())
            {
                // Key found
                auto idx = it->second;
                return committed_cmds[i][idx];
            }
        }
        verify(0);
    }

    int SaucrTestConfig::OneLeader(int expected)
    {
        return waitOneLeader(true, expected);
    }

    bool SaucrTestConfig::NoLeader(void)
    {
        int r = waitOneLeader(false, -1);
        return r == -1;
    }

    int SaucrTestConfig::waitOneLeader(bool want_leader, int expected)
    {
        Log_debug("expecting leader %d", expected);
        uint64_t mostRecentEpoch = 0, epoch;
        int leader = -1, i, retry;
        bool isleader;
        for (retry = 0; retry < 10; retry++)
        {
            // Reactor::CreateSpEvent<TimeoutEvent>(ELECTIONTIMEOUT / 10)->Wait();
            // Coroutine::Sleep(ELECTIONTIMEOUT/10);
            usleep(ELECTIONTIMEOUT / 10);
            leader = -1;
            mostRecentEpoch = 0;
            for (i = 0; i < NSERVERS; i++)
            {
                // ignore disconnected servers
                if (SaucrTestConfig::replicas[i]->svr_->IsDisconnected())
                    continue;
                SaucrTestConfig::replicas[i]->svr_->GetState(&isleader, &epoch);
                if (isleader)
                {
                    if (epoch == mostRecentEpoch)
                    {
                        Failed("multiple leaders elected in term %ld", epoch);
                        return -2;
                    }
                    else if (epoch > mostRecentEpoch)
                    {
                        leader = i;
                        mostRecentEpoch = epoch;
                        Log_debug("found leader %d with term %d", leader, epoch);
                    }
                }
            }
            if (leader != -1)
            {
                if (!want_leader)
                {
                    Failed("leader elected despite lack of quorum");
                }
                else if (expected >= 0 && leader != expected)
                {
                    Failed("unexpected leader change, expecting %d, got %d", expected, leader);
                    return -3;
                }
                return leader;
            }
        }
        if (want_leader)
        {
            Log_debug("failing, timeout?");
            Failed("waited too long for leader election");
        }
        return -1;
    }

    bool SaucrTestConfig::EpochMovedOn(uint64_t epoch)
    {
        for (int i = 0; i < NSERVERS; i++)
        {
            uint64_t curEpoch;
            bool isLeader;
            SaucrTestConfig::replicas[i]->svr_->GetState(&isLeader, &curEpoch);
            if (curEpoch > epoch)
            {
                return true;
            }
        }
        return false;
    }

    uint64_t SaucrTestConfig::OneEpoch(void)
    {
        // Do not consider disconnected servers
        uint64_t epoch, curEpoch;
        bool isLeader;
        // Get the epoch of the first connected server
        for (int i = 0; i < NSERVERS; i++)
        {
            if (!SaucrTestConfig::replicas[i]->svr_->IsDisconnected())
            {
                SaucrTestConfig::replicas[i]->svr_->GetState(&isLeader, &epoch);
                break;
            }
        }
        for (int i = 1; i < NSERVERS; i++)
        {
            if (SaucrTestConfig::replicas[i]->svr_->IsDisconnected())
                continue;
            SaucrTestConfig::replicas[i]->svr_->GetState(&isLeader, &curEpoch);
            if (curEpoch != epoch)
            {
                return -1;
            }
        }
        return epoch;
    }

    pair<uint64_t, uint64_t> SaucrTestConfig::GetLastCommittedZxid(void)
    {
        return last_committed_zxid;
    }

    void SaucrTestConfig::Disconnect(int svr)
    {
        verify(svr >= 0 && svr < NSERVERS);
        std::lock_guard<std::mutex> lk(disconnect_mtx_);
        verify(!disconnected_[svr]);
        disconnect(svr, true);
        disconnected_[svr] = true;
    }

    void SaucrTestConfig::Reconnect(int svr)
    {
        verify(svr >= 0 && svr < NSERVERS);
        std::lock_guard<std::mutex> lk(disconnect_mtx_);
        verify(disconnected_[svr]);
        reconnect(svr);
        disconnected_[svr] = false;
    }

    int SaucrTestConfig::NDisconnected(void)
    {
        int count = 0;
        for (int i = 0; i < NSERVERS; i++)
        {
            if (disconnected_[i])
                count++;
        }
        return count;
    }

    bool SaucrTestConfig::IsDisconnected(int svr)
    {
        return isDisconnected(svr);
    }

    void SaucrTestConfig::SetUnreliable(bool unreliable)
    {
        std::unique_lock<std::mutex> lk(cv_m_);
        verify(!finished_);
        if (unreliable)
        {
            verify(!unreliable_);
            // lk acquired cv_m_ in state 1 or 0
            unreliable_ = true;
            // if cv_m_ was in state 1, must signal cv_ to wake up netctlLoop
            lk.unlock();
            cv_.notify_one();
        }
        else
        {
            verify(unreliable_);
            // lk acquired cv_m_ in state 2 or 0
            unreliable_ = false;
            // wait until netctlLoop moves cv_m_ from state 2 (or 0) to state 1,
            // restoring the network to reliable state in the process.
            lk.unlock();
            lk.lock();
        }
    }

    bool SaucrTestConfig::IsUnreliable(void)
    {
        return unreliable_;
    }

    void SaucrTestConfig::Shutdown(void)
    {
        // trigger netctlLoop shutdown
        {
            std::unique_lock<std::mutex> lk(cv_m_);
            verify(!finished_);
            // lk acquired cv_m_ in state 0, 1, or 2
            finished_ = true;
            // if cv_m_ was in state 1, must signal cv_ to wake up netctlLoop
            lk.unlock();
            cv_.notify_one();
        }
        // wait for netctlLoop thread to exit
        th_.join();
        // Reconnect() all Deconnect()ed servers
        for (int i = 0; i < NSERVERS; i++)
        {
            if (disconnected_[i])
            {
                Reconnect(i);
            }
        }
    }

    uint64_t SaucrTestConfig::RpcCount(int svr, bool reset)
    {
        std::lock_guard<std::recursive_mutex> lk(replicas[svr]->commo_->rpc_mtx_);
        uint64_t count = replicas[svr]->commo_->rpc_count_;
        uint64_t count_last = rpc_count_last[svr];
        if (reset)
        {
            rpc_count_last[svr] = count;
        }
        verify(count >= count_last);
        return count - count_last;
    }

    uint64_t SaucrTestConfig::RpcTotal(void)
    {
        uint64_t total = 0;
        for (int i = 0; i < NSERVERS; i++)
        {
            total += replicas[i]->commo_->rpc_count_;
        }
        return total;
    }

    void SaucrTestConfig::netctlLoop(void)
    {
        int i;
        bool isdown;
        // cv_m_ unlocked state 0 (finished_ == false)
        std::unique_lock<std::mutex> lk(cv_m_);
        while (!finished_)
        {
            if (!unreliable_)
            {
                {
                    std::lock_guard<std::mutex> prlk(disconnect_mtx_);
                    // unset all unreliable-related disconnects and slows
                    for (i = 0; i < NSERVERS; i++)
                    {
                        if (!disconnected_[i])
                        {
                            reconnect(i, true);
                            slow(i, 0);
                        }
                    }
                }
                // sleep until unreliable_ or finished_ is set
                // cv_m_ unlocked state 1 (unreliable_ == false && finished_ == false)
                cv_.wait(lk, [this]()
                         { return unreliable_ || finished_; });
                continue;
            }
            {
                std::lock_guard<std::mutex> prlk(disconnect_mtx_);
                for (i = 0; i < NSERVERS; i++)
                {
                    // skip server if it was disconnected using Disconnect()
                    if (disconnected_[i])
                    {
                        continue;
                    }
                    // server has DOWNRATE_N / DOWNRATE_D chance of being down
                    if ((rand() % DOWNRATE_D) < DOWNRATE_N)
                    {
                        // disconnect server if not already disconnected in the previous period
                        disconnect(i, true);
                    }
                    else
                    {
                        // Server not down: random slow timeout
                        // Reconnect server if it was disconnected in the previous period
                        reconnect(i, true);
                        // server's slow timeout should be btwn 0-(MAXSLOW-1) ms
                        slow(i, rand() % MAXSLOW);
                    }
                }
            }
            // change unreliable state every 0.1s
            lk.unlock();
            usleep(100000);
            // cv_m_ unlocked state 2 (unreliable_ == true && finished_ == false)
            lk.lock();
        }
        // If network is still unreliable, unset it
        if (unreliable_)
        {
            unreliable_ = false;
            {
                std::lock_guard<std::mutex> prlk(disconnect_mtx_);
                // unset all unreliable-related disconnects and slows
                for (i = 0; i < NSERVERS; i++)
                {
                    if (!disconnected_[i])
                    {
                        reconnect(i, true);
                        slow(i, 0);
                    }
                }
            }
        }
        // cv_m_ unlocked state 3 (unreliable_ == false && finished_ == true)
    }

    bool SaucrTestConfig::isDisconnected(int svr)
    {
        std::lock_guard<std::recursive_mutex> lk(connection_m_);
        return replicas[svr]->svr_->IsDisconnected();
    }

    void SaucrTestConfig::disconnect(int svr, bool ignore)
    {
        std::lock_guard<std::recursive_mutex> lk(connection_m_);
        if (!isDisconnected(svr))
        {
            // simulate disconnected server
            replicas[svr]->svr_->Disconnect();
        }
        else if (!ignore)
        {
            verify(0);
        }
    }

    void SaucrTestConfig::reconnect(int svr, bool ignore)
    {
        std::lock_guard<std::recursive_mutex> lk(connection_m_);
        if (isDisconnected(svr))
        {
            // simulate reconnected server
            replicas[svr]->svr_->Reconnect();
        }
        else if (!ignore)
        {
            verify(0);
        }
    }

    void SaucrTestConfig::slow(int svr, uint32_t msec)
    {
        std::lock_guard<std::recursive_mutex> lk(connection_m_);
        verify(!isDisconnected(svr));
        replicas[svr]->commo_->rpc_poll_->slow(msec * 1000);
    }

#endif
}
