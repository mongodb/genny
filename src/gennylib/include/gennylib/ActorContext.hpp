#ifndef HEADER_2459295A_4C5E_4F47_8B70_60D8B8BC2CD6_INCLUDED
#define HEADER_2459295A_4C5E_4F47_8B70_60D8B8BC2CD6_INCLUDED

#include <mongocxx/client_session.hpp>

#include <gennylib/PhaseLoop.hpp>

namespace genny::actor {

struct BaseCounter {
    BaseCounter() = default;
    ~BaseCounter() = default;

    std::atomic_int nextId = 0;

    BaseCounter& operator++() {
        nextId++;
        return *this;
    }

    friend std::ostream& operator<<(std::ostream& os, const BaseCounter& lc) {
        return os << lc.nextId;
    }
};

}  // namespace genny::actor

#endif  // HEADER_2459295A_4C5E_4F47_8B70_60D8B8BC2CD6_INCLUDED
