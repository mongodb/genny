#include <gennylib/Cast.hpp>

#include <sstream>

namespace genny {
void Cast::add(const std::string_view& castName, std::shared_ptr<ActorProducer> entry) {
    const auto& [it, success] = _producers.emplace(castName, entry);

    if (!success) {
        std::ostringstream stream;
        auto previousActorProducer = it->second;
        stream << "Failed to add '" << entry->name() << "' as '" << castName
               << "', a producer named '" << previousActorProducer->name()
               << "' was already added instead.";
        throw std::domain_error(stream.str());
    }
}

std::ostream& Cast::streamProducersTo(std::ostream& out) const {
    for (const auto& [castName, producer] : globalCast().getProducers()) {
        out << castName << " is " << producer->name() << std::endl;
    }
    return out;
}
}  // namespace genny
