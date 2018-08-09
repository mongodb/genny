#ifndef HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
#define HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED

#include <bsoncxx/builder/stream/document.hpp>
#include <optional>
#include <random>
#include <unordered_map>

#include "parse_util.hpp"

namespace genny {

class ValueGenerator;

class document {
public:
  virtual ~document(){};
  virtual bsoncxx::document::view view(bsoncxx::builder::stream::document &doc,
                                       std::mt19937_64 &) {
    return doc.view();
  };
};

class bsonDocument : public document {
public:
  bsonDocument();
  bsonDocument(YAML::Node &);

  void setDoc(bsoncxx::document::value value) { doc = value; }
  virtual bsoncxx::document::view view(bsoncxx::builder::stream::document &,
                                       std::mt19937_64 &) override;

private:
  std::optional<bsoncxx::document::value> doc;
};

class templateDocument : public document {
public:
  templateDocument();
  templateDocument(YAML::Node &);
  virtual bsoncxx::document::view view(bsoncxx::builder::stream::document &,
                                       std::mt19937_64 &) override;

protected:
  // The document to override
  bsonDocument doc;
  unordered_map<string, unique_ptr<ValueGenerator>> override;

private:
  // apply the overides, one level at a time
  void applyOverrideLevel(bsoncxx::builder::stream::document &,
                          bsoncxx::document::view, string, std::mt19937_64 &);
};
} // namespace genny

#endif // HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
