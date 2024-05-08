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
        rrr::Marshal serialized_saucrstate_;
        std::mutex mtx_{};

    public:
        Persister();
        ~Persister();

        void SaveSaucrState(shared_ptr<Marshallable> &curr_saucrstate);
        shared_ptr<Marshallable> ReadSaucrState();
        size_t SaucrStateSize();
    };

} // namespace janus
