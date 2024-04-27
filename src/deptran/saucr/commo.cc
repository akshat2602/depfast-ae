#include "commo.h"
#include "macros.h"

namespace janus
{

    SaucrCommo::SaucrCommo(PollMgr *poll) : Communicator(poll)
    {
    }

    shared_ptr<SaucrQuorumEvent> SaucrCommo::SendRequestVote(parid_t par_id,
                                                             siteid_t site_id,
                                                             uint64_t c_id,
                                                             uint64_t c_epoch,
                                                             pair<uint64_t, uint64_t> last_seen_zxid)
    {
        auto ev = Reactor::CreateSpEvent<SaucrQuorumEvent>();
        auto proxies = rpc_par_proxies_[par_id];
        for (auto &p : proxies)
        {
            if (p.first != site_id)
                continue;
            SaucrProxy *proxy = (SaucrProxy *)p.second;
            FutureAttr fuattr;
            fuattr.callback = [ev](Future *fu)
            {
                bool_t vote_granted;
                bool_t f_ok;
                fu->get_reply() >> vote_granted >> f_ok;
                if (f_ok)
                {
                    if (vote_granted)
                        ev->VoteYes();
                    else
                        ev->VoteNo();
                }
                else
                {
                    ev->VoteNo();
                }
            };
            Call_Async(proxy,
                       RequestVote,
                       c_id,
                       c_epoch,
                       last_seen_zxid.first,
                       last_seen_zxid.second,
                       fuattr);
        }
        return ev;
    }

} // namespace janus