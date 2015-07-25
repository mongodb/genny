#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array_context.hpp>
#include <bsoncxx/builder/stream/single_context.hpp>
#include <bsoncxx/json.hpp>
#include <bson.h>
#include <vector>
#include <getopt.h>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

#include "query.hpp"
#include "insert.hpp"
#include "parse_util.hpp"
#include "workload.hpp"
#include "main.h"

using namespace std;
using namespace mwg;

void print_help(const char* process_name);
int main(int argc, char* argv[]);

static struct option poptions[] = {
    {"help",          no_argument,       0, 'h'},
    {0,               0,                 0,  0 }
};

int main(int argc, char *argv[]) {
  string filename = "sample.yml";
  int arg_count = 0;
  int idx = 0;
  while (1) {
      int arg = getopt_long(argc, argv, "h",
                            poptions, &idx);
      arg_count++;
      if (arg == -1) {
          // all arguments have been processed
          break;
      }
              ++arg_count;

        switch(arg) {
        case 'h':
            print_help(argv[0]);
            return EXIT_SUCCESS;
        default:
            fprintf(stderr, "unknown command line option: %s\n", poptions[idx].name);
            print_help(argv[0]);
            return EXIT_FAILURE;
        }
  }

  if (argc > optind)
      filename = argv[optind];
  cout << filename << endl;

  YAML::Node nodes = YAML::LoadFile(filename);

  // Look for main. And start building from there.
  if (auto main = nodes["main"]) {
    //    cout << "Have main here: " << main << endl;

    mongocxx::instance inst{};
    mongocxx::client conn{};

    workload myworkload(main);
    myworkload.execute(conn);
  }

  else
    cout << "There was no main" << endl;
}

void print_help(const char* process_name) {
    fprintf(stderr, "Usage: %s [-h] /path/to/workload \n"
                    "Execution Options:\n"
            "\t--help|-h         Display this help and exit\n\n", process_name);
}
