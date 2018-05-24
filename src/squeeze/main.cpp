#include <iostream>
#include <optional>
#include <string>

#include "version.h"

int main() {
    // basically just a test that we're using c++17
    auto x { std::make_optional(squeeze::lib::get_version()) };
    std::cout << x.value() << std::endl;
    return 0;
}
