#include "server.h"
#include "frame.h"
#include "coordinator.h"

namespace janus
{

    static int volatile x1 = MarshallDeputy::RegInitializer(MarshallDeputy::CMD_ZAB_COMMIT,
                                                            []() -> Marshallable *
                                                            {
                                                                return new ZABMarshallable;
                                                            });

    SaucrServer::SaucrServer(Frame *frame)
    {
        frame_ = frame;
    }

    SaucrServer::~SaucrServer() {}

    void SaucrServer::Setup() {}

#ifdef SAUCR_TEST_CORO
    bool SaucrServer::Start(shared_ptr<Marshallable> &cmd, pair<uint64_t, uint64_t> *zxid)
    {
        return false;
    }

    void SaucrServer::GetState(bool *is_leader, uint64_t *epoch)
    {
        /* Your code here. This function can be called from another OS thread. */
        *is_leader = 0;
        *epoch = 0;
    }
#endif
    /* Do not modify any code below here */

    void SaucrServer::Disconnect(const bool disconnect)
    {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        verify(disconnected_ != disconnect);
        // global map of rpc_par_proxies_ values accessed by partition then by site
        static map<parid_t, map<siteid_t, map<siteid_t, vector<SiteProxyPair>>>> _proxies{};
        if (_proxies.find(partition_id_) == _proxies.end())
        {
            _proxies[partition_id_] = {};
        }
        SaucrCommo *c = (SaucrCommo *)commo();
        if (disconnect)
        {
            verify(_proxies[partition_id_][loc_id_].size() == 0);
            verify(c->rpc_par_proxies_.size() > 0);
            auto sz = c->rpc_par_proxies_.size();
            _proxies[partition_id_][loc_id_].insert(c->rpc_par_proxies_.begin(), c->rpc_par_proxies_.end());
            c->rpc_par_proxies_ = {};
            verify(_proxies[partition_id_][loc_id_].size() == sz);
            verify(c->rpc_par_proxies_.size() == 0);
        }
        else
        {
            verify(_proxies[partition_id_][loc_id_].size() > 0);
            auto sz = _proxies[partition_id_][loc_id_].size();
            c->rpc_par_proxies_ = {};
            c->rpc_par_proxies_.insert(_proxies[partition_id_][loc_id_].begin(), _proxies[partition_id_][loc_id_].end());
            _proxies[partition_id_][loc_id_] = {};
            verify(_proxies[partition_id_][loc_id_].size() == 0);
            verify(c->rpc_par_proxies_.size() == sz);
        }
        disconnected_ = disconnect;
    }

    bool SaucrServer::IsDisconnected()
    {
        return disconnected_;
    }

}