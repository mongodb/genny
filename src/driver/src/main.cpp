#include <experimental/optional>

#include <iostream>
#include <string>

#include <gennylib/version.hpp>

int main() {
    // basically just a test that we're using c++17
    auto x { std::experimental::make_optional(genny::lib::get_version()) };
    std::cout << x.value_or("ERROR") << std::endl;
    return 0;
}
