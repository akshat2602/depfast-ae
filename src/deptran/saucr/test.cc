#include "test.h"

namespace janus
{

#ifdef SAUCR_TEST_CORO

    int SaucrTest::Run(void)
    {
        Print("START WHOLISTIC TESTS");
        config_->SetLearnerAction();
        uint64_t start_rpc = config_->RpcTotal();
        if (true
            // testBasicAgree()
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

#define Init2(test_id, description)                                                               \
    {                                                                                             \
        Init(test_id, description);                                                               \
        cmd = ((cmd / 100) + 1) * 100;                                                            \
        verify(config_->NDisconnected() == 0 && !config_->IsUnreliable() && !config_->AnySlow()); \
    }

#define InitSub2(sub_test_id, description)                                                        \
    {                                                                                             \
        InitSub(sub_test_id, description);                                                        \
        verify(config_->NDisconnected() == 0 && !config_->IsUnreliable() && !config_->AnySlow()); \
    }

#define Passed2() \
    {             \
        Passed(); \
        return 0; \
    }

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

    // #define AssertNoneExecuted(cmd)                                                   \
//     {                                                                             \
//         auto ne = config_->NExecuted(cmd, NSERVERS);                              \
//         Assert2(ne == 0, "%d servers unexpectedly executed command %d", ne, cmd); \
//     }

    // #define AssertNExecuted(cmd, expected)                                                             \
//     {                                                                                              \
//         auto ne = config_->NExecuted(cmd, expected);                                               \
//         Assert2(ne == expected, "%d servers executed command %d (%d expected)", ne, cmd, expected) \
//     }

    // #define AssertExecutedPairsInOrder(expected_pairs)              \
//     {                                                           \
//         auto r = config_->ExecutedPairsInOrder(expected_pairs); \
//         Assert2(r, "unexpected execution order of commands");   \
//     }

    // #define AssertSameExecutedOrder(dependent_cmds)                                      \
//     {                                                                                \
//         auto r = config_->ExecutedInSameOrder(dependent_cmds);                       \
//         Assert2(r, "execution order of dependent commands is different in servers"); \
//     }

    // #define AssertValidCommitStatus(replica_id, instance_no, r)                                                                                \
//     {                                                                                                                                      \
//         Assert2(r != -1, "failed to reach agreement for replica: %d instance: %d, committed different commands", replica_id, instance_no); \
//         Assert2(r != -2, "failed to reach agreement for replica: %d instance: %d, committed different dkey", replica_id, instance_no);     \
//         Assert2(r != -3, "failed to reach agreement for replica: %d instance: %d, committed different seq", replica_id, instance_no);      \
//         Assert2(r != -4, "failed to reach agreement for replica: %d instance: %d, committed different deps", replica_id, instance_no);     \
//     }

    // #define AssertNCommitted(replica_id, instance_no, n)                                                                            \
//     {                                                                                                                           \
//         auto r = config_->NCommitted(replica_id, instance_no, n);                                                               \
//         AssertValidCommitStatus(replica_id, instance_no, r);                                                                    \
//         Assert2(r >= n, "failed to reach agreement for replica: %d instance: %d among %d servers", replica_id, instance_no, n); \
//     }

    // // Akshat: Is noop needed here for saucr? Check this
    // #define AssertNCommittedAndVerifyNoop(replica_id, instance_no, n, noop)                                                                                 \
//     {                                                                                                                                                   \
//         bool cnoop;                                                                                                                                     \
//         string cdkey;                                                                                                                                   \
//         uint64_t cseq;                                                                                                                                  \
//         map<uint64_t, uint64_t> cdeps;                                                                                                                  \
//         auto r = config_->NCommitted(replica_id, instance_no, n, &cnoop, &cdkey, &cseq, &cdeps);                                                        \
//         AssertValidCommitStatus(replica_id, instance_no, r);                                                                                            \
//         Assert2(r >= n, "failed to reach agreement for replica: %d instance: %d among %d servers", replica_id, instance_no, n);                         \
//         Assert2(cnoop == noop || !noop, "failed to reach agreement for replica: %d instance: %d, expected noop, got command", replica_id, instance_no); \
//         Assert2(cnoop == noop || noop, "failed to reach agreement for replica: %d instance: %d, expected command, got noop", replica_id, instance_no);  \
//     }

    // // Akshat: Check if this is actually needed
    // #define AssertNCommittedAndVerifyAttrs(replica_id, instance_no, n, noop, exp_dkey, exp_seq, exp_deps)                                                                                                                                    \
//     {                                                                                                                                                                                                                                    \
//         bool cnoop;                                                                                                                                                                                                                      \
//         string cdkey;                                                                                                                                                                                                                    \
//         uint64_t cseq;                                                                                                                                                                                                                   \
//         map<uint64_t, uint64_t> cdeps;                                                                                                                                                                                                   \
//         auto r = config_->NCommitted(replica_id, instance_no, n, &cnoop, &cdkey, &cseq, &cdeps);                                                                                                                                         \
//         Assert2(r >= n, "failed to reach agreement for replica: %d instance: %d among %d servers", replica_id, instance_no, n);                                                                                                          \
//         AssertValidCommitStatus(replica_id, instance_no, r);                                                                                                                                                                             \
//         Assert2(cnoop == noop || !noop, "failed to reach agreement for replica: %d instance: %d, expected noop, got command", replica_id, instance_no);                                                                                  \
//         Assert2(cnoop == noop || noop, "failed to reach agreement for replica: %d instance: %d, expected command, got noop", replica_id, instance_no);                                                                                   \
//         Assert2(cdkey == exp_dkey, "failed to reach agreement for replica: %d instance: %d, expected dkey %s, got dkey %s", replica_id, instance_no, exp_dkey.c_str(), cdkey.c_str());                                                   \
//         Assert2(cseq == exp_seq, "failed to reach agreement for replica: %d instance: %d, expected seq %d, got seq %d", replica_id, instance_no, exp_seq, cseq);                                                                         \
//         Assert2(cdeps == exp_deps, "failed to reach agreement for replica: %d instance: %d, expected deps %s different from committed deps %s", replica_id, instance_no, map_to_string(exp_deps).c_str(), map_to_string(cdeps).c_str()); \
//     }

    // // Akshat: Change this to not use dependencies while running the test
    // #define DoAgreeAndAssertNCommittedAndVerifyAttrs(cmd, dkey, n, noop, exp_dkey, exp_seq, exp_deps)                                                                                                      \
//     {                                                                                                                                                                                                  \
//         bool cnoop;                                                                                                                                                                                    \
//         string cdkey;                                                                                                                                                                                  \
//         uint64_t cseq;                                                                                                                                                                                 \
//         map<uint64_t, uint64_t> cdeps;                                                                                                                                                                 \
//         auto r = config_->DoAgreement(cmd, dkey, n, false, &cnoop, &cdkey, &cseq, &cdeps);                                                                                                             \
//         Assert2(r >= 0, "failed to reach agreement for command %d among %d servers", cmd, n);                                                                                                          \
//         Assert2(r != -1, "failed to reach agreement for command %d, committed different commands", cmd);                                                                                               \
//         Assert2(r != -2, "failed to reach agreement for command %d, committed different dkey", cmd);                                                                                                   \
//         Assert2(r != -3, "failed to reach agreement for command %d, committed different seq", cmd);                                                                                                    \
//         Assert2(r != -4, "failed to reach agreement for command %d, committed different deps", cmd);                                                                                                   \
//         Assert2(cnoop == noop || !noop, "failed to reach agreement for command %d, expected noop, got command", cmd);                                                                                  \
//         Assert2(cnoop == noop || noop, "failed to reach agreement for command %d, expected command, got noop", cmd);                                                                                   \
//         Assert2(cdkey == exp_dkey, "failed to reach agreement for command %d, expected dkey %s, got dkey %s", cmd, exp_dkey.c_str(), cdkey.c_str());                                                   \
//         Assert2(cseq == exp_seq, "failed to reach agreement for command %d, expected seq %d, got seq %d", cmd, exp_seq, cseq);                                                                         \
//         Assert2(cdeps == exp_deps, "failed to reach agreement for command %d, expected deps %s different from committed deps %s", cmd, map_to_string(exp_deps).c_str(), map_to_string(cdeps).c_str()); \
//     }

    // #define DoAgreeAndAssertNoneCommitted(cmd, dkey)                                           \
//     {                                                                                      \
//         bool cnoop;                                                                        \
//         string cdkey;                                                                      \
//         uint64_t cseq;                                                                     \
//         map<uint64_t, uint64_t> cdeps;                                                     \
//         auto r = config_->DoAgreement(cmd, dkey, 1, false, &cnoop, &cdkey, &cseq, &cdeps); \
//         Assert2(r == 0, "committed command %d without majority", cmd);                     \
//     }

    // #define DisconnectNServers(n)       \
//     {                               \
//         for (int i = 0; i < n; i++) \
//         {                           \
//             config_->Disconnect(i); \
//         }                           \
//     }

    // #define ReconnectNServers(n)        \
//     {                               \
//         for (int i = 0; i < n; i++) \
//         {                           \
//             config_->Reconnect(i);  \
//         }                           \
//     }

    //     int SaucrTest::testBasicAgree(void)
    //     {
    //         Init2(1, "Basic agreement");
    //         config_->PauseExecution(false);
    //         for (int i = 1; i <= 3; i++)
    //         {
    //             // complete agreement and make sure its attributes are as expected
    //             cmd++;
    //             string dkey = to_string(cmd);
    //             map<uint64_t, uint64_t> deps;
    //             // Akshat: Change this to not use dependencies while running the test
    //             DoAgreeAndAssertNCommittedAndVerifyAttrs(cmd, dkey, NSERVERS, false, dkey, 1, deps);
    //             AssertNExecuted(cmd, NSERVERS);
    //         }
    //         Passed2();
    //     }

#endif

} // namespace janus