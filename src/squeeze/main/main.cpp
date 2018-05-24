#include <iostream>
#include <optional>
#include <string>

int main() {
    // basically just a test that we're using c++17
    auto x { std::make_optional("Hello, World!") };
    std::cout << x.value() << std::endl;
    return 0;
}
