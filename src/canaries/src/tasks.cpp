#include <canaries/tasks.hpp>

namespace genny::canaries::ping_task {
Singleton* Singleton::instance = nullptr;

Singleton* Singleton::getInstance(const std::string& mongoUri) {
    if (instance == nullptr) {
        // "new" is OK because the lifetime of this singleton object
        // is the same as the program.
        instance = new Singleton(mongoUri);
    }
    return instance;
}

Singleton::Singleton(std::string mongoUri)
    : _poolManager{{}},
      ns{
        R"(
          Clients:
            PingTask:
              URI: )" + mongoUri, ""
      },
      client{_poolManager.createClient("PingTask", 1, ns.root())},
      pingCmd{make_document(kvp("ping", 1))} {};
}  // namespace genny::canaries::ping_task
