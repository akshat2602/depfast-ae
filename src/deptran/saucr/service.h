#pragma once

#include "__dep__.h"
#include "saucr_rpc.h"
#include "server.h"
#include "macros.h"

namespace janus
{

    class TxLogServer;
    class SaucrServer;
    class SaucrServiceImpl : public SaucrService
    {
    public:
        SaucrServer *svr_;
        SaucrServiceImpl(TxLogServer *sched);

        RpcHandler(RequestVote, 6,
                   const uint64_t &, c_id,
                   const uint64_t &, c_epoch,
                   const uint64_t &, last_seen_epoch,
                   const uint64_t &, last_seen_cmd_count,
                   bool_t *, vote_granted,
                   bool_t *, f_ok)
        {
            *f_ok = false;
            *vote_granted = false;
        }

        RpcHandler(Propose, 4,
                   const uint64_t &, l_id,
                   const uint64_t &, epoch,
                   const MarshallDeputy &, data,
                   bool_t *, f_ok)
        {
            *f_ok = false;
        };

        RpcHandler(Commit, 3,
                   const uint64_t &, l_id,
                   const uint64_t &, epoch,
                   bool_t *, f_ok)
        {
            *f_ok = false;
        };

        RpcHandler(Heartbeat, 3,
                   const uint64_t &, l_id,
                   const uint64_t &, epoch,
                   bool_t *, f_ok)
        {
            *f_ok = false;
        };
    };
} // namespace janus
