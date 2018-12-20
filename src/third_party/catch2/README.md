## Some Notes on Genny's vendored version of Catch2

#### Source Code Versioning
Genny's vendors the single header version of [Catch2](https://github.com/catchorg/Catch2)
for its testing needs. Catch2's version number is listed at the top of `include/catch.hpp` in a
comment.

#### Cmake Integration Versioning
Genny also vendors Catch2's cmake integration to allow cmake to natively pick up Catch2's
test cases. The integration code, `ParseAndAddCatchTests.cmake`, is not coupled to the version
of Catch2; this allows bug fixes to be picked up without re-vendoring a newer version of Catch2.

When upgrading either Catch2 or its cmake integration, manually inspect the test results to
ensure everything still works.

#### Local Changes to Cmake Integration
Additionally, as of December 2018, there's a local change to `ParseAndAddCatchTests.cmake`. On the
line that `add_test` is called, there's an additional parameter `--out ${CTestName}.junit.xml`
appended to the `add_test` invocation. This allows uniquely named reports to be created for
each test, whereas the default behavior is to have the same report file created, causing
earlier ones to be get clobbered. See [Catch2#1316](https://github.com/catchorg/Catch2/issues/1316) for more detail.
