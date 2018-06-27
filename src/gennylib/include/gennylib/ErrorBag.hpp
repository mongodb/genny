#ifndef HEADER_C9EBE412_325B_4614_BD86_F7026AA772C1_INCLUDED
#define HEADER_C9EBE412_325B_4614_BD86_F7026AA772C1_INCLUDED

#include <string>
#include <vector>
#include <functional>
#include <type_traits>

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

private:
    std::vector<T> errors;

};

using ErrorBag = ErrorBag_base<std::string>;

}  // genny

#endif  // HEADER_C9EBE412_325B_4614_BD86_F7026AA772C1_INCLUDED
