cc_library(
    name = "bsoncxx",
    srcs = glob(["src/bsoncxx/**/*.cpp"]),
    hdrs = glob(["src/bsoncxx/**/*.hpp"]),
    # includes = ["src/bsoncxx/include/bsoncxx/v_noabi"],
    # includes = ["."],
    strip_include_prefix = "src",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "mongocxx",
    srcs = glob(["src/mongocxx/**/*.cpp"]),
    hdrs = glob(["src/mongocxx/**/*.hpp"]),
    # includes = ["."],
    # includes = ["src/mongocxx/include/mongocxx/v_noabi"],
    strip_include_prefix = "src",
    visibility = ["//visibility:public"],
    deps = [":bsoncxx"],
)
