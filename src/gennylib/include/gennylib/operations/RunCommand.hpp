#ifndef HEADER_E15AF094_36D4_40ED_AC66_1960B554EFE4_INCLUDED
#define HEADER_E15AF094_36D4_40ED_AC66_1960B554EFE4_INCLUDED

#include <gennylib/Operation.hpp>
#include <gennylib/context.hpp>

#include <gennylib/value_generators.hpp>

namespace genny::operation {
class RunCommand : public genny::Operation {
public:
    explicit RunCommand(const OperationContext& operationContext,
                        mongocxx::client& client,
                        std::mt19937_64& rng);

    ~RunCommand() = default;

    void run() override;

private:
    std::unique_ptr<value_generators::DocumentGenerator> _documentTemplate;
};
}  // namespace genny::operation

#endif  // HEADER_E15AF094_36D4_40ED_AC66_1960B554EFE4_INCLUDED