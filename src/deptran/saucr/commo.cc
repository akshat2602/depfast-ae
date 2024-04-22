#include "commo.h"
#include "macros.h"

namespace janus
{

    SaucrCommo::SaucrCommo(PollMgr *poll) : Communicator(poll)
    {
    }

    // void
    // SaucrCommo::SendStart(const siteid_t &site_id,
    //                        const parid_t &par_id,
    //                        const shared_ptr<Marshallable> &cmd,
    //                        const string &dkey,
    //                        const function<void(void)> &callback)
    // {
    //     auto proxies = rpc_par_proxies_[par_id];
    //     for (auto &p : proxies)
    //     {
    //         if (p.first != site_id)
    //             continue;
    //         EpaxosProxy *proxy = (EpaxosProxy *)p.second;
    //         FutureAttr fuattr;
    //         fuattr.callback = [callback](Future *fu)
    //         {
    //             callback();
    //         };
    //         MarshallDeputy md_cmd(cmd);
    //         Call_Async(proxy,
    //                    Start,
    //                    md_cmd,
    //                    dkey,
    //                    fuattr);
    //     }
    // }

} // namespace janus