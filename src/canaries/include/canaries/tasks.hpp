// Copyright 2019-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HEADER_521F5952_C79A_407E_9348_661B514D288D_INCLUDED
#define HEADER_521F5952_C79A_407E_9348_661B514D288D_INCLUDED

#include <chrono>
#include <thread>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>

#include <mongocxx/client.hpp>

#include <gennylib/v1/ConfigNode.hpp>
#include <gennylib/v1/PoolManager.hpp>

namespace genny::canaries {

/**
 * The tasks described in this file are adapted from the canary
 * benchmarks for MongoDB.
 *
 * See the following link for more detail:
 * https://github.com/mongodb/mongo/blob/r4.1.6/src/mongo/unittest/system_resource_canary_bm.cpp
 */
class Task {
public:
    virtual inline void run() = 0;

protected:

    /**
     * Adapted from Google Benchmark's benchmark::DoNotOptimize().
     */
    template <class T>
    void doNotOptimize(T const& value) {
        asm volatile("" : : "r,m"(value) : "memory");
    }
};

class NopTask : public Task {
public:
    void run() override {
        // Ensure memory is flushed and instruct the compiler
        // to not optimize this line out.
        doNotOptimize(_inc());
    }

private:
    int64_t _counter = 0;
    int64_t _inc() {
        // Do a trivial amount of work to (hopefully) trick the compiler
        // into not optimizing everything away.
        return _counter++;
    }
};

class SleepTask : public Task {
public:
    void run() override {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1ms);
    }
};

class CPUTask : public Task {
public:
    void run() override {
        uint64_t limit = 10000;
        uint64_t lresult = 0;
        uint64_t x = 100;
        for (uint64_t i = 0; i < limit; i++) {
            doNotOptimize(x *= 13);
        }
        doNotOptimize(lresult = x);
    }
};

class Strider {
public:
    static const int kStrideBytes = 64;

    // Array of pointers used as a linked list.
    std::unique_ptr<char*[]> data;

    Strider(int arrLength) {
        /*
         * Create a circular list of pointers using a simple striding
         * algorithm.
         */
        int counter = 0;

        data = std::make_unique<char*[]>(arrLength);

        char** arr = data.get();

        /*
         * This access pattern corresponds to many array/matrix algorithms.
         * It should be easily and correctly predicted by any decent hardware
         * prefetch algorithm.
         */
        for (counter = 0; counter < arrLength - kStrideBytes; counter += kStrideBytes) {
            arr[counter] = reinterpret_cast<char*>(&arr[counter + kStrideBytes]);
        }
        arr[counter] = reinterpret_cast<char*>(&arr[0]);
    }
};

// Generate a loop with macros.
#define ONE ptrToNextLinkedListNode = reinterpret_cast<char**>(*ptrToNextLinkedListNode);
#define FIVE ONE ONE ONE ONE ONE
#define TEN FIVE FIVE
#define FIFTY TEN TEN TEN TEN TEN
#define HUNDRED FIFTY FIFTY

class L2Task : public Task {
private:
    Strider _strider;
    static const size_t _arrLength = 256 * 1024;
    static constexpr size_t _counter = _arrLength / (Strider::kStrideBytes * 100) + 1;

public:
    L2Task() : _strider(_arrLength) {}

    inline void run() override {
        char** dummyResult = 0;  // Dummy result to prevent the loop from being optimized out.
        char** ptrToNextLinkedListNode = reinterpret_cast<char**>(_strider.data.get()[0]);

        for (size_t i = 0; i < _counter; ++i) {
            HUNDRED
        }
        doNotOptimize(dummyResult = ptrToNextLinkedListNode);
    }
};

class L3Task : public Task {
private:
    Strider _strider;
    static const size_t _arrLength = 8192 * 1024;
    static constexpr size_t _counter = _arrLength / (Strider::kStrideBytes * 100) + 1;

public:
    L3Task() : _strider(_arrLength) {}

    inline void run() override {
        char** dummyResult = 0;  // Dummy result to prevent the loop from being optimized out.
        char** ptrToNextLinkedListNode = reinterpret_cast<char**>(_strider.data.get()[0]);

        for (size_t i = 0; i < _counter; ++i) {
            HUNDRED
        }
        doNotOptimize(dummyResult = ptrToNextLinkedListNode);
    }
};

// Create a dummy namespace to limit scope of "using".
namespace ping_task {

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

// Create a singleton to reuse the pool.
class Singleton {
private:
    static Singleton* instance;

    genny::v1::PoolManager _poolManager;

    explicit Singleton(std::string mongoUri);

public:
    mongocxx::pool::entry client;
    bsoncxx::document::value pingCmd;

    static Singleton* getInstance(const std::string& mongoUri);
};


class PingTask : public Task {
private:
    Singleton* _shared;

public:
    PingTask() = delete;

    explicit PingTask(std::string mongoUri) : _shared{Singleton::getInstance(mongoUri)} {}

    inline void run() override {
        auto res = _shared->client->database("test").run_command(_shared->pingCmd.view());
    }
};

}  // namespace ping_task

using ping_task::PingTask;

}  // namespace genny::canaries

#endif  // HEADER_521F5952_C79A_407E_9348_661B514D288D_INCLUDED