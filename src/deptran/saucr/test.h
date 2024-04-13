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

    public:
        SaucrTest(SaucrTestConfig *config) : config_(config) {}
        int Run(void);

    private:
        // Accept/Commit tests
        int testBasicAgree(void);
        int testFastPathIndependentAgree(void);
        int testFastPathDependentAgree(void);
        int testSlowPathIndependentAgree(void);
        int testSlowPathDependentAgree(void);
        int testNonIdenticalAttrsAgree(void);
        int testFailNoQuorum(void);
        // Prepare tests
        int testConcurrentAgree(void);
        int testConcurrentUnreliableAgree(void);
    };

#endif

} // namespace janus
