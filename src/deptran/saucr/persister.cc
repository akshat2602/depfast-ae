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
    }

    shared_ptr<Marshallable> Persister::ReadSaucrState()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto initializer = MarshallDeputy::GetInitializer(saucrstate_kind_)();
        shared_ptr<Marshallable> saucrstate(initializer);
        saucrstate->FromMarshal(serialized_saucrstate_);
        saucrstate->ToMarshal(serialized_saucrstate_);
        return saucrstate;
    }

    size_t Persister::SaucrStateSize()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return saucrstate_size_;
    }


} // namespace janus
