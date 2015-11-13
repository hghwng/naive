#include <fstream>
#include <plg_api.h>

void util_read_conf(std::string path, PlgConf &conf) {
  std::ifstream stream;
  stream.open(path);

  std::string line;
  while (getline(stream, line)) {
    std::string key;
    std::string val;

    auto i = line.begin();
    for (; i != line.end() && isspace(*i); ++i);
    for (; i != line.end() && !isspace(*i); ++i) key.push_back(*i);
    for (; i != line.end() && isspace(*i); ++i);
    for (; i != line.end(); ++i) val.push_back(*i);
    conf[key] = val;
  }
}

