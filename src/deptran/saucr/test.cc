#include "test.h"

namespace janus
{

#ifdef SAUCR_TEST_CORO
#define TEST_EXPAND(x) x

    int SaucrTest::Run(void)
    {
        Print("START WHOLISTIC TESTS");
        config_->SetLearnerAction();
        uint64_t start_rpc = config_->RpcTotal();
        if (testInitialElection() ||
            // TEST_EXPAND(testReElection()) ||
            TEST_EXPAND(testBasicAgree()) || TEST_EXPAND(testFailAgree()) || TEST_EXPAND(testFailNoAgree()))
        // || TEST_EXPAND(testRejoin()) ||
        // TEST_EXPAND(testConcurrentStarts()) || TEST_EXPAND(testBackup())
        {
            Print("TESTS FAILED");
            return 1;
        }
        Print("ALL TESTS PASSED");
        Print("Total RPC count: %ld", config_->RpcTotal() - start_rpc);
        return 0;
    }

#define Init2(test_id, description) \
    Init(test_id, description);     \
    verify(config_->NDisconnected() == 0 && !config_->IsUnreliable())
#define Passed2() \
    Passed();     \
    return 0

#define Assert(expr) \
    if (!(expr))     \
    {                \
        return 1;    \
    }
#define Assert2(expr, msg, ...)     \
    if (!(expr))                    \
    {                               \
        Failed(msg, ##__VA_ARGS__); \
        return 1;                   \
    }
// Akshat: fix the asserts and assert calls from tests
#define AssertOneLeader(ldr) Assert(ldr >= 0)
#define AssertReElection(ldr, old) \
    Assert2(ldr != old, "no reelection despite leader being disconnected")
#define AssertNoneCommitted(zxid)                                                   \
    {                                                                               \
        auto nc = config_->NCommitted(zxid);                                        \
        Assert2(nc == 0,                                                            \
                "%d servers unexpectedly committed zxid.first %ld zxid.second %ld", \
                nc, zxid.first, zxid.second)                                        \
    }
#define AssertNCommitted(zxid, expected)                                         \
    {                                                                            \
        auto nc = config_->NCommitted(zxid);                                     \
        Assert2(nc == expected,                                                  \
                "%d servers committed zxid.first %ld zxid.second (%d expected)", \
                nc, zxid.first, zxid.second, expected)                           \
    }
#define AssertStartOk(ok) Assert2(ok, "unexpected leader change during Start()")
#define AssertWaitNoError(ret, zxid) \
    Assert2(ret != -3, "committed values differ for zxid.first %ld zxid.second %ld", zxid.first, zxid.second)
#define AssertWaitNoTimeout(ret, zxid, n)                                                                                         \
    Assert2(ret != -1, "waited too long for %d server(s) to commit zxid.first %ld, zxid.second %ld", n, zxid.first, zxid.second); \
    Assert2(ret != -2, "term moved on before zxid.first %ld zxid.second %ld committed by %d server(s)", zxid.first, zxid.second, n)
#define DoAgreeAndAssertZxid(cmd, n, zxid)                                                                                                                                                          \
    {                                                                                                                                                                                               \
        auto r = config_->DoAgreement(cmd, n, false);                                                                                                                                               \
        auto ind = zxid;                                                                                                                                                                            \
        Log_debug("r.first %lu r.second %lu", r.first, r.second);                                                                                                                                   \
        Log_debug("ind.first %lu ind.second %lu", ind.first, ind.second);                                                                                                                           \
        Assert2(r.second > 0, "failed to reach agreement for command %d among %d servers, expected commit index>0, got %" PRId64, cmd, n, r.second);                                                \
        Assert2(r.first == ind.first && r.second == ind.second, "agreement zxid incorrect. got .first %ld .second %ld, expected .first %ld .second %ld", r.first, r.second, ind.first, ind.second); \
    }
#define DoAgreeAndAssertWaitSuccess(cmd, n)                                                         \
    {                                                                                               \
        auto r = config_->DoAgreement(cmd, n, true);                                                \
        Assert2(r.second > 0, "failed to reach agreement for command %d among %d servers", cmd, n); \
        zxid_ = r;                                                                                  \
    }

    int SaucrTest::testInitialElection(void)
    {
        Init2(1, "Initial election");
        // Initial election: is there one leader?
        // Initial election does not need extra time
        // Coroutine::Sleep(ELECTIONTIMEOUT);
        int leader = config_->OneLeader();
        AssertOneLeader(leader);
        // calculate RPC count for initial election for later use
        init_rpcs_ = 0;
        for (int i = 0; i < NSERVERS; i++)
        {
            init_rpcs_ += config_->RpcCount(i);
        }
        // Does everyone agree on the epoch?
        uint64_t epoch = config_->OneEpoch();
        Assert2(epoch != -1, "servers disagree on epoch");
        // Sleep for a while
        Coroutine::Sleep(ELECTIONTIMEOUT);
        // Does the epoch stay the same after a while if there's no failures?
        Assert2(config_->OneEpoch() == epoch, "unexpected epoch change");
        // Is the same server still the only leader?
        AssertOneLeader(config_->OneLeader(leader));
        Passed2();
    }

    int SaucrTest::testReElection(void)
    {
        Init2(2, "Re-election after network failure");
        // find current leader
        int leader = config_->OneLeader();
        AssertOneLeader(leader);
        // disconnect leader - make sure a new one is elected
        Log_info("disconnecting old leader");
        config_->Disconnect(leader);
        int oldLeader = leader;
        Coroutine::Sleep(ELECTIONTIMEOUT);
        leader = config_->OneLeader();
        AssertOneLeader(leader);
        AssertReElection(leader, oldLeader);
        // reconnect old leader - should not disturb new leader
        config_->Reconnect(oldLeader);
        Log_info("reconnecting old leader");
        Coroutine::Sleep(ELECTIONTIMEOUT);
        AssertOneLeader(config_->OneLeader(leader));
        // no quorum -> no leader
        Log_info("disconnecting more servers");
        config_->Disconnect((leader + 1) % NSERVERS);
        config_->Disconnect((leader + 2) % NSERVERS);
        config_->Disconnect(leader);
        Assert(config_->NoLeader());
        // quorum restored
        Log_info("reconnecting a server to enable majority");
        config_->Reconnect((leader + 2) % NSERVERS);
        Coroutine::Sleep(ELECTIONTIMEOUT);
        AssertOneLeader(config_->OneLeader());
        // rejoin all servers
        Log_info("rejoining all servers");
        config_->Reconnect((leader + 1) % NSERVERS);
        config_->Reconnect(leader);
        Coroutine::Sleep(ELECTIONTIMEOUT);
        AssertOneLeader(config_->OneLeader());
        Passed2();
    }

    int SaucrTest::testBasicAgree(void)
    {
        Init2(3, "Basic agreement");
        int epoch = config_->OneEpoch();
        zxid_ = make_pair(epoch, 1);
        for (int i = 1; i <= 3; i++)
        {
            // make sure no commits exist before any agreements are started
            AssertNoneCommitted(zxid_);
            Log_debug("NONE COMMITTED");
            // Get the current leader to fetch the latest epoch
            Log_debug("STARTING AGREEMENT");
            // complete 1 agreement and make sure its index is as expected
            DoAgreeAndAssertZxid((int)(i + 300), NSERVERS, make_pair(zxid_.first, zxid_.second++));
        }
        Passed2();
    }

    int SaucrTest::testFailAgree(void)
    {
        Init2(4, "Agreement despite follower disconnection");
        // disconnect 2 followers
        auto leader = config_->OneLeader();
        zxid_ = config_->GetLastCommittedZxid();
        zxid_.second++;
        AssertOneLeader(leader);
        Log_debug("disconnecting two followers leader");
        config_->Disconnect((leader + 1) % NSERVERS);
        config_->Disconnect((leader + 2) % NSERVERS);
        // Agreement despite 2 disconnected servers
        Log_debug("try commit a few commands after disconnect");
        DoAgreeAndAssertZxid(401, NSERVERS - 2, make_pair(zxid_.first, zxid_.second++));
        DoAgreeAndAssertZxid(402, NSERVERS - 2, make_pair(zxid_.first, zxid_.second++));
        Coroutine::Sleep(ELECTIONTIMEOUT);
        DoAgreeAndAssertZxid(403, NSERVERS - 2, make_pair(zxid_.first, zxid_.second++));
        DoAgreeAndAssertZxid(404, NSERVERS - 2, make_pair(zxid_.first, zxid_.second++));
        // reconnect followers
        Log_debug("reconnect servers");
        config_->Reconnect((leader + 1) % NSERVERS);
        config_->Reconnect((leader + 2) % NSERVERS);
        Coroutine::Sleep(ELECTIONTIMEOUT);
        Log_debug("try commit a few commands after reconnect");
        DoAgreeAndAssertWaitSuccess(405, NSERVERS);
        DoAgreeAndAssertWaitSuccess(406, NSERVERS);
        Passed2();
    }

    int SaucrTest::testFailNoAgree(void)
    {
        Init2(5, "No agreement if too many followers disconnect");
        // disconnect 3 followers
        auto leader = config_->OneLeader();
        AssertOneLeader(leader);
        config_->Disconnect((leader + 1) % NSERVERS);
        config_->Disconnect((leader + 2) % NSERVERS);
        config_->Disconnect((leader + 3) % NSERVERS);
        // attempt to do an agreement
        pair<uint64_t, uint64_t> zxid;
        AssertStartOk(config_->Start(leader, 501, &zxid));
        Assert2(zxid.second == ++zxid_.second && zxid.first > 0,
                "Start() returned unexpected index (%ld, expected %ld) and/or term (%ld, expected >0)",
                zxid.second, zxid_.second - 1, zxid.first);
        Coroutine::Sleep(ELECTIONTIMEOUT);
        AssertNoneCommitted(zxid);
        // reconnect followers
        config_->Reconnect((leader + 1) % NSERVERS);
        config_->Reconnect((leader + 2) % NSERVERS);
        config_->Reconnect((leader + 3) % NSERVERS);
        // do agreement in restored quorum
        Coroutine::Sleep(ELECTIONTIMEOUT);
        DoAgreeAndAssertWaitSuccess(502, NSERVERS);
        Passed2();
    }

    // int SaucrTest::testRejoin(void)
    // {
    //     Init2(6, "Rejoin of disconnected leader");
    //     DoAgreeAndAssertZxid(601, NSERVERS, index_++);
    //     // disconnect leader
    //     auto leader1 = config_->OneLeader();
    //     AssertOneLeader(leader1);
    //     config_->Disconnect(leader1);
    //     Coroutine::Sleep(ELECTIONTIMEOUT);
    //     // Make old leader try to agree on some entries (these should not commit)
    //     uint64_t index, term;
    //     AssertStartOk(config_->Start(leader1, 602, &index, &term));
    //     AssertStartOk(config_->Start(leader1, 603, &index, &term));
    //     AssertStartOk(config_->Start(leader1, 604, &index, &term));
    //     // New leader commits, successfully
    //     DoAgreeAndAssertWaitSuccess(605, NSERVERS - 1);
    //     DoAgreeAndAssertWaitSuccess(606, NSERVERS - 1);
    //     // Disconnect new leader
    //     auto leader2 = config_->OneLeader();
    //     AssertOneLeader(leader2);
    //     AssertReElection(leader2, leader1);
    //     config_->Disconnect(leader2);
    //     // reconnect old leader
    //     config_->Reconnect(leader1);
    //     // wait for new election
    //     Coroutine::Sleep(ELECTIONTIMEOUT);
    //     auto leader3 = config_->OneLeader();
    //     AssertOneLeader(leader3);
    //     AssertReElection(leader3, leader2);
    //     // More commits
    //     DoAgreeAndAssertWaitSuccess(607, NSERVERS - 1);
    //     DoAgreeAndAssertWaitSuccess(608, NSERVERS - 1);
    //     // Reconnect all
    //     config_->Reconnect(leader2);
    //     DoAgreeAndAssertWaitSuccess(609, NSERVERS);
    //     Passed2();
    // }

    // class CSArgs
    // {
    // public:
    //     std::vector<uint64_t> *indices;
    //     std::mutex *mtx;
    //     int i;
    //     int leader;
    //     uint64_t term;
    //     SaucrTestConfig *config;
    // };

    // static void *doConcurrentStarts(void *args)
    // {
    //     CSArgs *csargs = (CSArgs *)args;
    //     uint64_t idx, tm;
    //     auto ok = csargs->config->Start(csargs->leader, 701 + csargs->i, &idx, &tm);
    //     if (!ok || tm != csargs->term)
    //     {
    //         return nullptr;
    //     }
    //     {
    //         std::lock_guard<std::mutex> lock(*(csargs->mtx));
    //         csargs->indices->push_back(idx);
    //     }
    //     return nullptr;
    // }

    // int SaucrTest::testConcurrentStarts(void)
    // {
    //     Init2(7, "Concurrently started agreements");
    //     int nconcurrent = 5;
    //     bool success = false;
    //     for (int again = 0; again < 5; again++)
    //     {
    //         if (again > 0)
    //         {
    //             wait(3000000);
    //         }
    //         auto leader = config_->OneLeader();
    //         AssertOneLeader(leader);
    //         uint64_t index, term;
    //         auto ok = config_->Start(leader, 701, &index, &term);
    //         if (!ok)
    //         {
    //             continue; // retry (up to 5 times)
    //         }
    //         // create 5 threads that each Start a command to leader
    //         std::vector<uint64_t> indices{};
    //         std::vector<int> cmds{};
    //         std::mutex mtx{};
    //         pthread_t threads[nconcurrent];
    //         for (int i = 0; i < nconcurrent; i++)
    //         {
    //             CSArgs *args = new CSArgs{};
    //             args->indices = &indices;
    //             args->mtx = &mtx;
    //             args->i = i;
    //             args->leader = leader;
    //             args->term = term;
    //             args->config = config_;
    //             verify(pthread_create(&threads[i], nullptr, doConcurrentStarts, (void *)args) == 0);
    //         }
    //         // join all threads
    //         for (int i = 0; i < nconcurrent; i++)
    //         {
    //             verify(pthread_join(threads[i], nullptr) == 0);
    //         }
    //         if (config_->TermMovedOn(term))
    //         {
    //             goto skip; // if leader's term is expiring, start over
    //         }
    //         // wait for all indices to commit
    //         for (auto index : indices)
    //         {
    //             int cmd = config_->Wait(index, NSERVERS, term);
    //             if (cmd < 0)
    //             {
    //                 AssertWaitNoError(cmd, index);
    //                 goto skip; // on timeout and term changes, try again
    //             }
    //             cmds.push_back(cmd);
    //         }
    //         // make sure all the commits are there with the correct values
    //         for (int i = 0; i < nconcurrent; i++)
    //         {
    //             auto val = 701 + i;
    //             int j;
    //             for (j = 0; j < cmds.size(); j++)
    //             {
    //                 if (cmds[j] == val)
    //                 {
    //                     break;
    //                 }
    //             }
    //             Assert2(j < cmds.size(), "cmd %d missing", val);
    //         }
    //         success = true;
    //         break;
    //     skip:;
    //     }
    //     Assert2(success, "too many term changes and/or delayed responses");
    //     index_ += nconcurrent + 1;
    //     Passed2();
    // }

    // int SaucrTest::testBackup(void)
    // {
    //     Init2(8, "Leader backs up quickly over incorrect follower logs");
    //     // disconnect 3 servers that are not the leader
    //     int leader1 = config_->OneLeader();
    //     AssertOneLeader(leader1);
    //     Log_debug("disconnect 3 followers");
    //     config_->Disconnect((leader1 + 2) % NSERVERS);
    //     config_->Disconnect((leader1 + 3) % NSERVERS);
    //     config_->Disconnect((leader1 + 4) % NSERVERS);
    //     // Start() a bunch of commands that won't be committed
    //     uint64_t index, term;
    //     for (int i = 0; i < 50; i++)
    //     {
    //         AssertStartOk(config_->Start(leader1, 800 + i, &index, &term));
    //     }
    //     Coroutine::Sleep(ELECTIONTIMEOUT);
    //     // disconnect the leader and its 1 follower, then reconnect the 3 servers
    //     Log_debug("disconnect the leader and its 1 follower, reconnect the 3 followers");
    //     config_->Disconnect((leader1 + 1) % NSERVERS);
    //     config_->Disconnect(leader1);
    //     config_->Reconnect((leader1 + 2) % NSERVERS);
    //     config_->Reconnect((leader1 + 3) % NSERVERS);
    //     config_->Reconnect((leader1 + 4) % NSERVERS);
    //     // do a bunch of agreements among the new quorum
    //     Coroutine::Sleep(ELECTIONTIMEOUT);
    //     Log_debug("try to commit a lot of commands");
    //     for (int i = 1; i <= 50; i++)
    //     {
    //         DoAgreeAndAssertZxid(800 + i, NSERVERS - 2, index_++);
    //     }
    //     // reconnect the old leader and its follower
    //     Log_debug("reconnect the old leader and the follower");
    //     config_->Reconnect((leader1 + 1) % NSERVERS);
    //     config_->Reconnect(leader1);
    //     Coroutine::Sleep(ELECTIONTIMEOUT);
    //     // do an agreement all together to check the old leader's incorrect
    //     // entries are replaced in a timely manner
    //     int leader2 = config_->OneLeader();
    //     AssertOneLeader(leader2);
    //     AssertStartOk(config_->Start(leader2, 851, &index, &term));
    //     index_++;
    //     // 10 seconds should be enough to back up 50 incorrect logs
    //     Coroutine::Sleep(2 * ELECTIONTIMEOUT);
    //     Log_debug("check if the old leader has enough committed");
    //     AssertNCommitted(index, NSERVERS);
    //     Passed2();
    // }

#endif

} // namespace janus