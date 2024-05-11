#include "persister.h"

namespace janus
{

    Persister::Persister() {}

    Persister::~Persister() {}

    void Persister::SaveSaucrState(shared_ptr<Marshallable> &curr_saucrstate)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        saucrstate_kind_ = curr_saucrstate->kind_;
        if (saucrstate_size_ != -1)
        {
            auto initializer = MarshallDeputy::GetInitializer(saucrstate_kind_)();
            shared_ptr<Marshallable> old_raftstate(initializer);
            old_raftstate->FromMarshal(serialized_saucrstate_);
        }
        curr_saucrstate->ToMarshal(serialized_saucrstate_);
        saucrstate_size_ = serialized_saucrstate_.content_size();
        auto ev = Reactor::CreateSpEvent<TimeoutEvent>(10000);
        ev->Wait();
        if (ev->IsReady())
        {
            return;
        }
    }

    shared_ptr<Marshallable> Persister::ReadSaucrState()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto initializer = MarshallDeputy::GetInitializer(saucrstate_kind_)();
        shared_ptr<Marshallable> saucrstate(initializer);
        saucrstate->FromMarshal(serialized_saucrstate_);
        saucrstate->ToMarshal(serialized_saucrstate_);
        auto ev = Reactor::CreateSpEvent<TimeoutEvent>(10000);
        ev->Wait();
        if (ev->IsReady())
        {
            return saucrstate;
        }
    }

    shared_ptr<Marshallable> Persister::ReadFastSwitchEntry()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto initializer = MarshallDeputy::GetInitializer(MarshallDeputy::CMD_FAST_SWITCH_ENTRY)();
        shared_ptr<Marshallable> fast_switch_entry(initializer);
        fast_switch_entry->FromMarshal(serialized_fast_switch_entry_);
        auto ev = Reactor::CreateSpEvent<TimeoutEvent>(10000);
        ev->Wait();
        if (ev->IsReady())
        {
            return fast_switch_entry;
        }
    }

    void Persister::SaveFastSwitchEntry(shared_ptr<Marshallable> &curr_fast_switch_entry)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto initializer = MarshallDeputy::GetInitializer(MarshallDeputy::CMD_FAST_SWITCH_ENTRY)();
        shared_ptr<Marshallable> fast_switch_entry(initializer);
        fast_switch_entry->ToMarshal(serialized_fast_switch_entry_);
        fast_switch_entry_size_ = serialized_fast_switch_entry_.content_size();
        auto ev = Reactor::CreateSpEvent<TimeoutEvent>(10000);
        ev->Wait();
        if (ev->IsReady())
        {
            return;
        }
    }

    size_t Persister::SaucrStateSize()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return saucrstate_size_;
    }

    size_t Persister::FastSwitchEntrySize()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return fast_switch_entry_size_;
    }

} // namespace janus
