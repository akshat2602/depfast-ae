#pragma once

#include "testconf.h"

namespace janus
{

#ifdef SAUCR_TEST_CORO

    class SaucrTest
    {

    private:
        SaucrTestConfig *config_;
        uint64_t init_rpcs_;
        uint64_t cmd = 0;
        pair<uint64_t, uint64_t> zxid_;

    public:
        SaucrTest(SaucrTestConfig *config) : config_(config) {}
        int Run(void);

    private:
        // Election tests
        int testInitialElection(void);
        int testReElection(void);
        // Agreement tests
        int testBasicAgree(void);
        int testFailAgree(void);
        int testFailNoAgree(void);
        int testRejoin(void);
        int testConcurrentStarts(void);
        int testBackup(void);
        int testCount(void);

        void wait(uint64_t microseconds);
    };

#endif

} // namespace janus
