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

        RpcHandler(RequestVote, 9,
                   const uint64_t &, c_id,
                   const uint64_t &, c_epoch,
                   const uint64_t &, last_seen_epoch,
                   const uint64_t &, last_seen_cmd_count,
                   bool_t *, vote_granted,
                   bool_t *, f_ok,
                   uint64_t *, conflict_epoch,
                   uint64_t *, conflict_cmd_count,
                   uint64_t *, reply_epoch)
        {
            *f_ok = false;
            *vote_granted = false;
            *conflict_epoch = 0;
            *conflict_cmd_count = 0;
            *reply_epoch = 0;
        }

        RpcHandler(Propose, 5,
                   const uint64_t &, l_id,
                   const uint64_t &, l_epoch,
                   const LogEntry &, entry,
                   bool_t *, f_ok,
                   uint64_t *, reply_epoch)
        {
            *f_ok = false;
            *reply_epoch = 0;
        };

        RpcHandler(Commit, 6,
                   const uint64_t &, l_id,
                   const uint64_t &, l_epoch,
                   const uint64_t &, zxid_commit_epoch,
                   const uint64_t &, zxid_commit_count,
                   bool_t *, f_ok,
                   uint64_t *, reply_epoch)
        {
            *f_ok = false;
            *reply_epoch = 0;
        };

        RpcHandler(Heartbeat, 4,
                   const uint64_t &, l_id,
                   const uint64_t &, l_epoch,
                   bool_t *, f_ok,
                   uint64_t *, reply_epoch)
        {
            *f_ok = false;
            *reply_epoch = 0;
        };

        RpcHandler(SyncLogs, 5,
                   const uint64_t &, l_id,
                   const uint64_t &, l_epoch,
                   const vector<LogEntry> &, logs,
                   bool_t *, f_ok,
                   uint64_t *, reply_epoch)
        {
            *f_ok = false;
            *reply_epoch = 0;
        };
    };
} // namespace janus
