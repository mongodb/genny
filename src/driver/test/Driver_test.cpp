#include <helpers.hpp>
#include <DefaultDriver.hpp>
#include <boost/log/trivial.hpp>


using namespace genny::driver;


TEST_CASE("Normal Execution") {
    DefaultDriver driver;

    BOOST_LOG_TRIVIAL(info) << cwd();

    ProgramOptions opts;
    opts.workloadFileName = "./test/BigUpdate.yml";
    driver.run(opts);
}
