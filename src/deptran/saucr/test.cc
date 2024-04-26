#include "test.h"

namespace janus
{

#ifdef SAUCR_TEST_CORO

    int SaucrTest::Run(void)
    {
        Print("START WHOLISTIC TESTS");
        config_->SetLearnerAction();
        uint64_t start_rpc = config_->RpcTotal();
        if (testInitialElection()
            // || testFastPathIndependentAgree() || testFastPathDependentAgree() || testSlowPathIndependentAgree() || testSlowPathDependentAgree() || testFailNoQuorum()
            // || testNonIdenticalAttrsAgree()
            // || testPrepareCommittedCommandAgree() || testPrepareAcceptedCommandAgree() || testPreparePreAcceptedCommandAgree() || testPrepareNoopCommandAgree() || testConcurrentAgree() || testConcurrentUnreliableAgree())
        )
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

#define AssertOneLeader(ldr) Assert(ldr >= 0)
#define AssertReElection(ldr, old) \
    Assert2(ldr != old, "no reelection despite leader being disconnected")
#define AssertNoneCommitted(index)                             \
    {                                                          \
        auto nc = config_->NCommitted(index);                  \
        Assert2(nc == 0,                                       \
                "%d servers unexpectedly committed index %ld", \
                nc, index)                                     \
    }
#define AssertNCommitted(index, expected)                       \
    {                                                           \
        auto nc = config_->NCommitted(index);                   \
        Assert2(nc == expected,                                 \
                "%d servers committed index %ld (%d expected)", \
                nc, index, expected)                            \
    }
#define AssertStartOk(ok) Assert2(ok, "unexpected leader change during Start()")
#define AssertWaitNoError(ret, index) \
    Assert2(ret != -3, "committed values differ for index %ld", index)
#define AssertWaitNoTimeout(ret, index, n)                                                \
    Assert2(ret != -1, "waited too long for %d server(s) to commit index %ld", n, index); \
    Assert2(ret != -2, "term moved on before index %ld committed by %d server(s)", index, n)
#define DoAgreeAndAssertIndex(cmd, n, index)                                                                                           \
    {                                                                                                                                  \
        auto r = config_->DoAgreement(cmd, n, false);                                                                                  \
        auto ind = index;                                                                                                              \
        Assert2(r > 0, "failed to reach agreement for command %d among %d servers, expected commit index>0, got %" PRId64, cmd, n, r); \
        Assert2(r == ind, "agreement index incorrect. got %ld, expected %ld", r, ind);                                                 \
    }
#define DoAgreeAndAssertWaitSuccess(cmd, n)                                                  \
    {                                                                                        \
        auto r = config_->DoAgreement(cmd, n, true);                                         \
        Assert2(r > 0, "failed to reach agreement for command %d among %d servers", cmd, n); \
        index_ = r + 1;                                                                      \
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
#endif

} // namespace janus