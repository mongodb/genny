#ifndef HEADER_C9EBE412_325B_4614_BD86_F7026AA772C1_INCLUDED
#define HEADER_C9EBE412_325B_4614_BD86_F7026AA772C1_INCLUDED

#include <string>
#include <vector>
#include <functional>
#include <type_traits>
#include <yaml-cpp/yaml.h>

namespace genny {

template <class T>
class ErrorBag_base {

public:

    explicit operator bool() {
        return !this->errors.empty();
    }

    void add(T error) {
        errors.push_back(std::move(error));
    }

    void report(std::ostream& out) {
        for(const auto& error : errors) {
            out << u8"😱 ";
            out << error;
        }
    }

    template<class Is, class Expect>
    void require(Is&& is, Expect&& expect) {
        require("", std::forward<Is>(is), std::forward<Expect>(expect));
    }

    template<class Is, class Expect>
    void require(const std::string& key, const Is& is, const Expect& expect) {
        if (expect != is) {
            add((key.empty() ? "" : "Key " + key + " ") + "expect [" + expect + "] but is [" + is + "]");
        }
    }

    template<class E = std::string>
    void require(const std::string& key, const YAML::Node& node, const E& expect, const std::string& path = "") {
        if (!node[key]) {
            add("Key " + path + key + " not found");
            return;
        }
        require(key, node[key].as<E>(), expect);
    }

private:
    std::vector<T> errors;

};

using ErrorBag = ErrorBag_base<std::string>;

}  // genny

#endif  // HEADER_C9EBE412_325B_4614_BD86_F7026AA772C1_INCLUDED
