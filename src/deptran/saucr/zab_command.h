#pragma once

#include "../marshallable.h"

namespace janus
{
    // This will take in a command along with a zxid and convert it into a ZAB_COMMIT command
    class ZABMarshallable : public Marshallable
    {
    public:
        int cmd;
        pair<uint64_t, uint64_t> zxid;

        ZABMarshallable() : Marshallable(MarshallDeputy::CMD_ZAB_COMMIT) {}

        Marshal &ToMarshal(Marshal &m) const override
        {
            m << cmd << zxid;
            return m;
        }

        Marshal &FromMarshal(Marshal &m) override
        {
            m >> cmd >> zxid;
            return m;
        }
    };
}