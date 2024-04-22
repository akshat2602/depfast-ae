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

        RpcHandler(Propose, 6,
                   const uint64_t &, l_id,
                   const uint64_t &, epoch,
                   const uint64_t &, zxid_key,
                   const uint64_t &, zxid_value,
                   const MarshallDeputy &, data,
                   bool_t *, f_ok)
        {
            *f_ok = false;
        };

        RpcHandler(Commit, 5,
                   const uint64_t &, l_id,
                   const uint64_t &, epoch,
                   const uint64_t &, zxid_key,
                   const uint64_t &, zxid_value,
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
