#include "multiply_generator.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <unordered_map>

#include "threadState.hpp"
#include "workload.hpp"


using bsoncxx::array::value;
using bsoncxx::builder::stream::finalize;

namespace mwg {

value multiplyVar(std::unordered_map<string, bsoncxx::array::value>::iterator var, int64_t factor) {
    auto varview = var->second.view();
    auto elem = varview[0];
    // Make a copy of the value to return
    // Everything after this is about the update. Should be common with workloadNode and
    // operationDocument
    bsoncxx::builder::stream::array myArray{};
    switch (elem.type()) {
        case bsoncxx::type::k_int64: {
            bsoncxx::types::b_int64 value = elem.get_int64();
            value.value *= factor;
            return (myArray << value << bsoncxx::builder::stream::finalize);
            break;
        }
        case bsoncxx::type::k_int32: {
            bsoncxx::types::b_int32 value = elem.get_int32();
            value.value *= factor;
            return (myArray << value << bsoncxx::builder::stream::finalize);
            break;
        }
        case bsoncxx::type::k_double: {
            bsoncxx::types::b_double value = elem.get_double();
            value.value *= factor;
            return (myArray << value << bsoncxx::builder::stream::finalize);
            break;
        }
        case bsoncxx::type::k_utf8:
        case bsoncxx::type::k_document:
        case bsoncxx::type::k_array:
        case bsoncxx::type::k_binary:
        case bsoncxx::type::k_undefined:
        case bsoncxx::type::k_oid:
        case bsoncxx::type::k_bool:
        case bsoncxx::type::k_date:
        case bsoncxx::type::k_null:
        case bsoncxx::type::k_regex:
        case bsoncxx::type::k_dbpointer:
        case bsoncxx::type::k_code:
        case bsoncxx::type::k_symbol:
        case bsoncxx::type::k_timestamp:

            BOOST_LOG_TRIVIAL(fatal) << "multiplyVar with type unsuported type in list";
            exit(EXIT_FAILURE);
            break;
        default:
            BOOST_LOG_TRIVIAL(fatal) << "multiplyVar with type unsuported type not in list";
            exit(EXIT_FAILURE);
    }
    return (myArray << 0 << bsoncxx::builder::stream::finalize);
}


MultiplyGenerator::MultiplyGenerator(YAML::Node& node) : ValueGenerator(node) {
    // add in error checking
    for (auto fnode : node["factors"]) {
        factors.push_back(makeUniqueValueGenerator(fnode));
    }
}

double MultiplyGenerator::generateDouble(threadState& state) {
    double result = 1.0;
    for (auto& factor : factors) {
        result *= factor->generateDouble(state);
    }
    return result;
}

int64_t MultiplyGenerator::generateInt(threadState& state) {
    return (static_cast<int64_t>(generateDouble(state)));
}

bsoncxx::array::value MultiplyGenerator::generate(threadState& state) {
    return (bsoncxx::builder::stream::array{} << generateDouble(state)
                                              << bsoncxx::builder::stream::finalize);
}
std::string MultiplyGenerator::generateString(threadState& state) {
    return (std::to_string(generateDouble(state)));
}
}
