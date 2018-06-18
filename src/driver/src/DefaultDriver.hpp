#ifndef HEADER_81A374DA_8E23_4E4D_96D2_619F27016F2A_INCLUDED
#define HEADER_81A374DA_8E23_4E4D_96D2_619F27016F2A_INCLUDED

namespace genny::driver {

/**
 * Basic workload driver that spins up one thread per actor.
 */
class DefaultDriver {

public:
    /**
     * @param argc c-style argc
     * @param argv c-style argv
     * @return c-style exit code
     */
    int run(int argc, char** argv) const;
};

}  // namespace genny::driver

#endif  // HEADER_81A374DA_8E23_4E4D_96D2_619F27016F2A_INCLUDED
