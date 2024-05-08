#pragma once

#include "frame.h"

namespace janus
{

#ifdef SAUCR_TEST_CORO

// slow network connections have latency up to 26 milliseconds
#define MAXSLOW 27
// servers have 1/10 chance of being disconnected to the network N = Numerator, D = Denominator
#define DOWNRATE_N 1
#define DOWNRATE_D 10
// Give a generous 5 seconds for elections
#define ELECTIONTIMEOUT 5000000

#define Print(format, ...) fprintf(stderr, format "\n", ##__VA_ARGS__)

    extern int _test_id_g;
    extern std::chrono::_V2::system_clock::time_point _test_starttime_g;
#define Init(test_id, description)           \
    Print("TEST %d: " description, test_id); \
    _test_id_g = test_id;                    \
    _test_starttime_g = std::chrono::system_clock::now();

#define InitSub(sub_test_id, description) \
    Print("TEST %d.%d: " description, _test_id_g, sub_test_id);

#define Failed(msg, ...) Print("TEST %d Failed: " msg, _test_id_g, ##__VA_ARGS__);

#define Passed() \
    Print("TEST %d Passed (time taken: %d s)", _test_id_g, (std::chrono::system_clock::now() - _test_starttime_g) / 1000000000);

    extern string map_to_string(map<uint64_t, uint64_t> m);
    extern string pair_to_string(pair<uint64_t, uint64_t> m);

    class SaucrTestConfig
    {

    private:
        static SaucrFrame **replicas;
        static std::vector<int> committed_cmds[NSERVERS];
        static std::vector<unordered_map<std::string, int>> committed_zxids;
        static pair<uint64_t, uint64_t> last_committed_zxid;
        static uint64_t rpc_count_last[NSERVERS];

        // disconnected_[svr] true if svr is disconnected by Disconnect()/Reconnect()
        bool disconnected_[NSERVERS];
        // slow_[svr] true if svr is set as slow by SetSlow()
        bool slow_[NSERVERS];
        // guards disconnected_ between Disconnect()/Reconnect() and netctlLoop
        std::mutex disconnect_mtx_;

    public:
        SaucrTestConfig(SaucrFrame **replicas);

        // sets up learner action functions for the servers
        // so that each committed command and its associated zxid on each server is
        // logged to this test's data structures.
        void SetLearnerAction(void);

        // Akshat: Setup perf test coro
#ifdef SAUCR_PERF_TEST_CORO
        // sets up commit learner action functions for the servers to the passed callback function
        void SetCommittedLearnerAction(function<function<void(Marshallable &)>(int)> callback);
#endif

        // Calls Start() to specified server
        // Returns true on success, false on error.
        // Also returns the zxid of the command that was started.
        bool Start(int svr, int cmd, pair<uint64_t, uint64_t> *zxid);

        // Waits for at least n servers to commit index
        // If commit takes too long, gives up after a while.
        // If term has moved on since the given start term, also gives up.
        // Returns the committed value on success.
        // -1 if it took too long for enough servers to commit
        // -2 if term changed
        // -3 if committed values for index differ
        int Wait(pair<uint64_t, uint64_t> zxid, int n, uint64_t epoch);

        // Calls GetState() to specified server
        void GetState(int svr, bool *is_leader, uint64_t *epoch);

        // Returns committed values for server
        vector<int> GetCommitted(int svr);

        // Returns the last committed zxid of all servers
        pair<uint64_t, uint64_t> GetLastCommittedZxid(void);

        // Returns index of leader on success, < 0 on error.
        // If expected is specified, only returns success if the leader == expected
        // Only looks at servers that are not disconnected
        int OneLeader(int expected = -1);

        bool NoLeader(void);

        // Returns true if at least 1 server has a currentTerm
        // number higher than term.
        bool EpochMovedOn(uint64_t term);

        // Checks if all servers agree on a term
        // Returns agreed upon term on success
        // -1 if there's disagreement
        uint64_t OneEpoch(void);

        // Returns number of servers that think zxid is committed.
        // Checks if the committed value for zxid is the same across servers.
        int NCommitted(pair<uint64_t, uint64_t> zxid);

        // Does one agreement.
        // Submits a command with value cmd to the leader
        // Waits at most 2 seconds until n servers commit the command.
        // Makes sure the value of the commits is the same as what was given.
        // If retry == true, Retries the agreement until at most 10 seconds pass.
        // Returns index of committed agreement on success, 0 on error.
        pair<uint64_t, uint64_t> DoAgreement(int cmd, int n, bool retry);

        // Disconnects server from rest of servers
        void Disconnect(int svr);

        // Reconnects disconnected server
        void Reconnect(int svr);

        void Restart(int svr)
        {
            return replicas[svr]->Restart();
        }

        // Returns number of disconnected servers
        int NDisconnected(void);

        // Checks if server was disconnected from rest of servers
        bool IsDisconnected(int svr);

        // Sets/unsets network unreliable
        // Blocks until network successfully set to unreliable/reliable
        // If unreliable == true, previous call to SetUnreliable must have been false
        // and vice versa
        void SetUnreliable(bool unreliable = true);

        bool IsUnreliable(void);

        // Reconnects all disconnected servers
        // Waits on unreliable thread
        void Shutdown(void);

        // Resets RPC counts to zero
        void RpcCountReset(void);

        // Returns total RPC count for a server
        // if reset, the next time RpcCount called for
        // svr, the count will exclude all RPCs before this call
        uint64_t RpcCount(int svr, bool reset = true);

        // Returns total RPC count across all servers
        // since server setup.
        uint64_t RpcTotal(void);

    private:
        // vars & subroutine for unreliable network setting
        std::thread th_;
        std::mutex cv_m_; // guards cv_, unreliable_, finished_
        std::condition_variable cv_;
        bool unreliable_ = false;
        bool finished_ = false;
        void netctlLoop(void);

        // internal disconnect/reconnect/slow functions
        std::recursive_mutex connection_m_;
        bool isDisconnected(int svr);
        void disconnect(int svr, bool ignore = false);
        void reconnect(int svr, bool ignore = false);
        void slow(int svr, uint32_t msec);

        // other internal helpers
        int waitOneLeader(bool want_leader, int expected);
    };

#endif

}
