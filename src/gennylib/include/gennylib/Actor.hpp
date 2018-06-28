#ifndef HEADER_00818641_6D7B_4A3D_AFC6_38CC0DBAD99B_INCLUDED
#define HEADER_00818641_6D7B_4A3D_AFC6_38CC0DBAD99B_INCLUDED

#include <gennylib/Orchestrator.hpp>

namespace genny {

class Actor {

public:
    virtual void run() = 0;
    virtual ~Actor() = default;
};


}  // namespace genny

#endif  // HEADER_00818641_6D7B_4A3D_AFC6_38CC0DBAD99B_INCLUDED
