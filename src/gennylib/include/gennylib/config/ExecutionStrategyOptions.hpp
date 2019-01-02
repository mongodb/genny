#ifndef HEADER_4E5CB3A4_FE6D_49B3_A31A_4237238C5A31
#define HEADER_4E5CB3A4_FE6D_49B3_A31A_4237238C5A31

namespace genny::config {
struct ExecutionStrategyOptions {
    static constexpr auto kDefaultMaxRetries = size_t{0};
    static constexpr auto kDefaultThrowOnFailure = false;

    size_t maxRetries = kDefaultMaxRetries;
    bool throwOnFailure = kDefaultThrowOnFailure;
};
} // namespace genny::config

#endif // HEADER_4E5CB3A4_FE6D_49B3_A31A_4237238C5A31
