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

    void SaucrServiceImpl::HandlePropose(const uint64_t &l_id,
                                         const uint64_t &epoch,
                                         const uint64_t &zxid_key,
                                         const uint64_t &zxid_value,
                                         const MarshallDeputy &data,
                                         bool_t *f_ok,
                                         rrr::DeferredReply *defer)
    {
        *f_ok = false;
        // svr_->HandlePropose(l_id, epoch, zxid_key, zxid_value, data, ack_received);
        defer->reply();
    }

    void SaucrServiceImpl::HandleCommit(const uint64_t &l_id,
                                        const uint64_t &epoch,
                                        const uint64_t &zxid_key,
                                        const uint64_t &zxid_value,
                                        bool_t *f_ok,
                                        rrr::DeferredReply *defer)
    {
        *f_ok = false;
        // svr_->HandleCommit(l_id, epoch, zxid_key, zxid_value, f_ok);
        defer->reply();
    }

    void SaucrServiceImpl::HandleHeartbeat(const uint64_t &l_id,
                                           const uint64_t &epoch,
                                           bool_t *f_ok,
                                           rrr::DeferredReply *defer)
    {
        *f_ok = false;
        // svr_->HandleHeartbeat(l_id, epoch, f_ok);
        defer->reply();
    }

} // namespace janus;
