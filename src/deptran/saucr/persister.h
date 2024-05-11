#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../deptran/marshallable.h"

namespace janus
{

    class Persister
    {
    private:
        int saucrstate_kind_;
        size_t saucrstate_size_ = -1;
        size_t fast_switch_entry_size_ = -1;
        rrr::Marshal serialized_saucrstate_;
        rrr::Marshal serialized_fast_switch_entry_;
        std::mutex mtx_{};

    public:
        Persister();
        ~Persister();

        void SaveSaucrState(shared_ptr<Marshallable> &curr_saucrstate);
        void SaveFastSwitchEntry(shared_ptr<Marshallable> &curr_fast_switch_entry);
        shared_ptr<Marshallable> ReadSaucrState();
        shared_ptr<Marshallable> ReadFastSwitchEntry();
        size_t SaucrStateSize();
        size_t FastSwitchEntrySize();
    };

} // namespace janus
