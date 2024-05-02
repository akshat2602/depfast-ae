#include "../marshallable.h"
#include "service.h"
#include "server.h"

namespace janus
{

    SaucrServiceImpl::SaucrServiceImpl(TxLogServer *sched)
        : svr_((SaucrServer *)sched)
    {
        struct timespec curr_time;
        clock_gettime(CLOCK_MONOTONIC_RAW, &curr_time);
        srand(curr_time.tv_nsec);
    }

    void SaucrServiceImpl::HandleRequestVote(const uint64_t &c_id,
                                             const uint64_t &c_epoch,
                                             const uint64_t &last_seen_epoch,
                                             const uint64_t &last_seen_cmd_count,
                                             bool_t *vote_granted,
                                             bool_t *f_ok,
                                             uint64_t *conflict_epoch,
                                             uint64_t *conflict_cmd_count,
                                             uint64_t *reply_epoch,
                                             rrr::DeferredReply *defer)
    {
        svr_->HandleRequestVote(c_id, c_epoch, last_seen_epoch, last_seen_cmd_count, conflict_epoch, conflict_cmd_count, vote_granted, f_ok, reply_epoch, defer);
    }

    void SaucrServiceImpl::HandlePropose(const uint64_t &l_id,
                                         const uint64_t &l_epoch,
                                         const LogEntry &entry,
                                         bool_t *f_ok,
                                         uint64_t *reply_epoch,
                                         rrr::DeferredReply *defer)
    {
        svr_->HandlePropose(l_id, l_epoch, entry, f_ok, reply_epoch, defer);
    }

    void SaucrServiceImpl::HandleCommit(const uint64_t &l_id,
                                        const uint64_t &epoch,
                                        const uint64_t &zxid_commit_epoch,
                                        const uint64_t &zxid_commit_count,
                                        bool_t *f_ok,
                                        uint64_t *reply_epoch,
                                        rrr::DeferredReply *defer)
    {
        svr_->HandleCommit(l_id, epoch, zxid_commit_epoch, zxid_commit_count, f_ok, reply_epoch, defer);
    }

    void SaucrServiceImpl::HandleHeartbeat(const uint64_t &l_id,
                                           const uint64_t &l_epoch,
                                           bool_t *f_ok,
                                           uint64_t *reply_epoch,
                                           rrr::DeferredReply *defer)
    {
        svr_->HandleHeartbeat(l_id, l_epoch, f_ok, reply_epoch, defer);
    }

    void SaucrServiceImpl::HandleSyncLogs(const uint64_t &l_id,
                                          const uint64_t &l_epoch,
                                          const vector<LogEntry> &logs,
                                          bool_t *f_ok,
                                          uint64_t *reply_epoch,
                                          rrr::DeferredReply *defer)
    {
        svr_->HandleSync(l_id, l_epoch, logs, f_ok, reply_epoch, defer);
    }

} // namespace janus;
