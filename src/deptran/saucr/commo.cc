#include "commo.h"
#include "macros.h"

namespace janus
{

    SaucrCommo::SaucrCommo(PollMgr *poll) : Communicator(poll)
    {
    }

    shared_ptr<SaucrNewLeaderQuorumEvent> SaucrCommo::SendRequestVote(parid_t par_id,
                                                                      siteid_t site_id,
                                                                      uint64_t c_id,
                                                                      uint64_t c_epoch,
                                                                      pair<uint64_t, uint64_t> last_seen_zxid)
    {
        Log_info("Request vote sending from %lu", site_id);
        auto ev = Reactor::CreateSpEvent<SaucrNewLeaderQuorumEvent>();
        auto proxies = rpc_par_proxies_[par_id];
        for (auto &p : proxies)
        {
            Log_info("SendRequestVote: %lu", p.first);
            if (p.first == site_id)
            {
                continue;
            }
            SaucrProxy *proxy = (SaucrProxy *)p.second;
            FutureAttr fuattr;
            fuattr.callback = [ev, p, last_seen_zxid](Future *fu)
            {
                bool_t vote_granted;
                bool_t f_ok;
                uint64_t conflict_last_seen_zxid_epoch;
                uint64_t conflict_last_seen_zxid_count;
                fu->get_reply() >> vote_granted >> f_ok >> conflict_last_seen_zxid_epoch >> conflict_last_seen_zxid_count;
                ev->conflict_last_seen_zxid_[p.first] = {conflict_last_seen_zxid_epoch, conflict_last_seen_zxid_count};
                auto conflict_zxid = ev->conflict_last_seen_zxid_[p.first];
                Log_info("RequestVote reply from %lu: vote_granted = %d, f_ok = %d, conflict_last_seen_zxid = %lu, %lu", p.first, vote_granted, f_ok, conflict_zxid.first, conflict_zxid.second);
                if (f_ok && vote_granted)
                {
                    Log_info("Vote granted from %lu", p.first);
                    ev->VoteYes(p.first);
                }
                else if (conflict_zxid.first > last_seen_zxid.first || (conflict_zxid.first == last_seen_zxid.first && conflict_zxid.second > last_seen_zxid.second))
                {
                    // Keep the vote to not granted since the follower has a higher last seen zxid
                    ev->VoteNo();
                }
                else if (conflict_zxid.first < last_seen_zxid.first || (conflict_zxid.first == last_seen_zxid.first && conflict_zxid.second < last_seen_zxid.second))
                {
                    // Mark the vote as conflict
                    ev->VoteConflict(p.first);
                }
                else if (conflict_zxid.first == last_seen_zxid.first && conflict_zxid.second == last_seen_zxid.second)
                {
                    // Mark the vote as conflict
                    ev->VoteYes(p.first);
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

    shared_ptr<SaucrBaseQuorumEvent> SaucrCommo::SendHeartbeat(parid_t par_id,
                                                               siteid_t site_id,
                                                               uint64_t l_id,
                                                               uint64_t l_epoch)
    {
        Log_info("Heartbeat sending from %lu", site_id);
        auto ev = Reactor::CreateSpEvent<SaucrBaseQuorumEvent>(NSERVERS, ceil(NSERVERS / 2));
        auto proxies = rpc_par_proxies_[par_id];
        for (auto &p : proxies)
        {
            Log_info("SendHeartbeat: %lu", p.first);
            if (p.first == site_id)
            {
                continue;
            }
            SaucrProxy *proxy = (SaucrProxy *)p.second;
            FutureAttr fuattr;
            fuattr.callback = [ev](Future *fu)
            {
                bool_t f_ok;
                fu->get_reply() >> f_ok;
                if (f_ok)
                {
                    ev->VoteYes();
                }
                else
                {
                    ev->VoteNo();
                }
            };
            Call_Async(proxy,
                       Heartbeat,
                       l_id,
                       l_epoch,
                       fuattr);
        }
        return ev;
    }

    shared_ptr<SaucrBaseQuorumEvent> SaucrCommo::SendProposal(parid_t par_id,
                                                              siteid_t site_id,
                                                              uint64_t l_id,
                                                              uint64_t l_epoch,
                                                              LogEntry &entry)
    {
        Log_info("Proposal sending from %lu", site_id);
        auto ev = Reactor::CreateSpEvent<SaucrBaseQuorumEvent>(NSERVERS, ceil(NSERVERS / 2));
        auto proxies = rpc_par_proxies_[par_id];
        for (auto &p : proxies)
        {
            if (p.first == site_id)
            {
                continue;
            }
            SaucrProxy *proxy = (SaucrProxy *)p.second;
            FutureAttr fuattr;
            fuattr.callback = [ev](Future *fu)
            {
                bool_t f_ok;
                fu->get_reply() >> f_ok;
                Log_info("Proposal reply f_ok = %d", f_ok);
                if (f_ok)
                {
                    ev->VoteYes();
                }
                else
                {
                    ev->VoteNo();
                }
            };
            Call_Async(proxy,
                       Propose,
                       l_id,
                       l_epoch,
                       entry,
                       fuattr);
        }
        return ev;
    }

    shared_ptr<SaucrBaseQuorumEvent> SaucrCommo::SendCommit(parid_t par_id,
                                                            siteid_t site_id,
                                                            uint64_t l_id,
                                                            uint64_t l_epoch,
                                                            uint64_t zxid_commit_epoch,
                                                            uint64_t zxid_commit_count)
    {
        Log_info("Commit sending from %lu", site_id);
        auto ev = Reactor::CreateSpEvent<SaucrBaseQuorumEvent>(NSERVERS, ceil(NSERVERS / 2));
        auto proxies = rpc_par_proxies_[par_id];
        for (auto &p : proxies)
        {
            if (p.first == site_id)
            {
                continue;
            }
            SaucrProxy *proxy = (SaucrProxy *)p.second;
            FutureAttr fuattr;
            fuattr.callback = [ev](Future *fu)
            {
                bool_t f_ok;
                fu->get_reply() >> f_ok;
                Log_info("Commit reply f_ok = %d", f_ok);
                if (f_ok)
                {
                    ev->VoteYes();
                }
                else
                {
                    ev->VoteNo();
                }
            };
            Call_Async(proxy,
                       Commit,
                       l_id,
                       l_epoch,
                       zxid_commit_epoch,
                       zxid_commit_count,
                       fuattr);
        }
        return ev;
    }

    void SaucrCommo::SendSync(parid_t par_id,
                              siteid_t site_id,
                              uint64_t l_id,
                              uint64_t l_epoch,
                              shared_ptr<SaucrNewLeaderQuorumEvent> ev,
                              vector<vector<LogEntry>> &logs)
    {
        Log_info("Sync sending from %lu", site_id);
        auto proxies = rpc_par_proxies_[par_id];
        for (int i = 0; i < proxies.size(); i++)
        {
            auto p = proxies[i];
            if (p.first == site_id)
            {
                continue;
            }
            if (ev->GetVotes()[i] == SAUCR_VOTE_GRANTED || ev->GetVotes()[i] == SAUCR_VOTE_NOT_GRANTED)
            {
                continue;
            }
            SaucrProxy *proxy = (SaucrProxy *)p.second;
            FutureAttr fuattr;
            fuattr.callback = [ev, p](Future *fu)
            {
                bool_t f_ok;
                fu->get_reply() >> f_ok;
                if (f_ok)
                {
                    ev->VoteYes(p.first);
                }
            };
            Call_Async(proxy,
                       SyncLogs,
                       l_id,
                       l_epoch,
                       logs[i],
                       fuattr);
        }
    }

} // namespace janus