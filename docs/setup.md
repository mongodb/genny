# Build and Install

Here're the steps to get Genny up and running locally:

0.  Ensure you have a suitable platform. Genny supports the following platforms:
    - Ubuntu 20.04, 18.04, 16.04
    - Arch Linux
    - RHEL 8, 7, 6.2
    - Amazon Linux 2
    - macOS

    If you attempt to install Genny on an unsupported platform, compile isn't
    guaranteed to succeed. If you would like to add support for a platform,
    please file a `TIG` ticket.

1.  Install the development tools for your OS.

    -   Ubuntu 18.04/20.04: `sudo apt install build-essential`
    -   Red Hat/CentOS 7/Amazon Linux 2:
        `sudo yum groupinstall "Development Tools"`
    -   Arch: `apk add bash clang gcc musl-dev linux-headers`
    -   macOS: `xcode-select --install`
    -   Windows: <https://visualstudio.microsoft.com/>

2.  Make sure you have a C++17 compatible compiler and Python 3.7 or newer.
    The ones from mongodbtoolchain v3 are safe bets if you're
    unsure. (mongodbtoolchain is internal to MongoDB).

3.  `./run-genny install [ubuntu1804|ubuntu2004|ubuntu1604|archlinux|rhel8|rhel70|rhel62|amazon2|not-linux]`

    This command downloads Genny's toolchain, compiles Genny, creates its
    virtualenv, and installs Genny to `dist/`. You can rerun this command
    at any time to rebuild Genny. If your OS isn't the supported, please let us know in
    `#workload-generation` slack or on GitHub.

    Note that the `--linux-distro` argument is not needed on macOS.

    You can also specify `--build-system make` if you prefer to build
    using `make` rather than `ninja`. Building using `make` may make
    some IDEs happier.

    If you get python errors, ensure you have a modern version of python3.
    On a Mac, run `brew install python3` (assuming you have [homebrew installed](https://brew.sh/))
    and then restart your shell.
    
### Errors Mentioning zstd
There is currently a leak in Genny's toolchain requiring zstd to be installed.
If the `./run-genny install` phase above errors mentioning this, you may need to install it separately.

On macOS, you can `brew install zstd`. On Ubuntu, you can apt-install zstd.

After installing this dependency, re-running the `./run-genny install` phase above should work.

# macOS Unable to get local issuer certificate
If you are on macOS and you see python errors such as when trying `./run-genny install` that contain `ssl.SSLCertVerificationError: [SSL: CERTIFICATE_VERIFY_FAILED] certificate verify failed: unable to get local issuer certificate` this is likely a problem with the python `requests` library. Here is a [StackOverflow answer](https://stackoverflow.com/questions/44649449/brew-installation-of-python-3-6-1-ssl-certificate-verify-failed-certificate/44649450#44649450) which addresses the problem.

## IDEs and Whatnot

We follow CMake and C++17 best-practices, so anything that doesn't work
via "normal means" is probably a bug.

We support using CLion and any conventional editors or IDEs (VSCode,
emacs, vim, etc.). Before doing anything cute (see
[CONTRIBUTING.md](./CONTRIBUTING.md)), please do due-diligence to ensure
it's not going to make common editing environments go wonky.

If you're using CLion, make sure to set `CMake options`
(in settings/preferences) so it can find the toolchain.

The cmake command is printed when `run-genny install` runs, you can
copy and paste the options into Clion. The options
should look something like this:

```bash
-G some-build-system \
-DCMAKE_PREFIX_PATH=/data/mci/gennytoolchain/installed/x64-osx-shared \
-DCMAKE_TOOLCHAIN_FILE=/data/mci/gennytoolchain/scripts/buildsystems/vcpkg.cmake \
-DVCPKG_TARGET_TRIPLET=x64-osx-static
```

See the following images:

### CLion ToolChain Settings

![toolchain](https://user-images.githubusercontent.com/22506/112030965-b9659500-8b32-11eb-9fa4-523640f4c95a.png?raw=true "Toolchains Settings")

### CLion CMake Settings

![CMake](https://user-images.githubusercontent.com/22506/112030931-ac48a600-8b32-11eb-9a09-0f3fd9138c8e.png?raw=true "Cmake Settings")

If you run `./run-genny install -b make` it should set up everything for you.
You just need to set the "Generation Path" to your `build` directory.

### Automatically Running Poplar before genny_core launch in CLion

Create a file called poplar.sh with the following contents:

```bash
#!/bin/bash
pkill -9 curator # Be careful if there are more curator processes that should be kept. 
DATE_SUFFIX=$(date +%s)
mv  build/CedarMetrics  build/CedarMetrics-${DATE_SUFFIX} 2>/dev/null
mv build/WorkloadOutput-${DATE_SUFFIX} 2>/dev/null

# curator is installed by cmake.
build/curator/curator poplar grpc &

sleep 1
```

Next create an external tool for poplar in CLion:

![poplar](https://user-images.githubusercontent.com/22506/112030958-b66aa480-8b32-11eb-9857-593adb3e9832.png?raw=true "Poplar External tool")

*Note*: the Working directory value is required. 

Finally the external poplar tool to the CLion 'Before Launch' list:

![Debug](https://user-images.githubusercontent.com/22506/112030946-b23e8700-8b32-11eb-9c40-a455355969bd.png?raw=true "Debug Before Launch.")
