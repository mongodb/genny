#ifndef HEADER_81A374DA_8E23_4E4D_96D2_619F27016F2A_INCLUDED
#define HEADER_81A374DA_8E23_4E4D_96D2_619F27016F2A_INCLUDED

#include <optional>
#include <string>
#include <vector>

#include <gennylib/ActorProducer.hpp>
#include <gennylib/ActorVector.hpp>

namespace genny::driver {

struct ProgramOptions {
    explicit ProgramOptions() = default;

    /**
     * @param argc c-style argc
     * @param argv c-style argv
     */
    ProgramOptions(int argc, char** argv);

    enum class YamlSource { FILE, STRING };
    YamlSource sourceType = YamlSource::FILE;
    std::string workloadSource;  // either file name or yaml

    std::string metricsFormat;
    std::string metricsOutputFileName;
    std::string mongoUri;
    std::string description;
    bool isHelp = false;

    // Mainly only useful for testing. Allow specifying additional
    // ActorProducers which will be *added* to the default list.
    std::vector<genny::ActorProducer> otherProducers;
};

/**
 * Basic workload driver that spins up one thread per actor.
 */
class DefaultDriver {

public:
    enum class OutcomeCode {
        kSuccess = 0,
        kStandardException = 1,
        kBoostException = 2,
        kInternalException = 3,
        kUnknownException = 10,
    };

    /**
     * @return c-style exit code
     */
    OutcomeCode run(const ProgramOptions& options) const;
};

}  // namespace genny::driver

#endif  // HEADER_81A374DA_8E23_4E4D_96D2_619F27016F2A_INCLUDED
