#include <dirent.h>
#include <dlfcn.h>
#include <vector>
#include <fstream>

#include <plugin.h>

struct Plugin {
  PlgInfo info;
  PlgConf conf;
};

std::vector<Plugin> g_plgs;
PlgConf g_conf;

void plugin_read_conf(std::string path, PlgConf &conf) {
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

bool plugin_load_all() {
  static const char *kPluginPath = "plugins/";
  static const char *kConfPath = "conf/";
  static const char *kSymbol = "plg_init";

  DIR *dir = opendir(kPluginPath);
  if (dir == NULL) return false;

  struct dirent *ent;
  while ((ent = readdir(dir)) != NULL) {
    std::string so_path = kPluginPath;
    so_path += ent->d_name;

    auto handle = dlopen(so_path.c_str(), RTLD_LAZY);
    if (!handle) continue;

    auto fn = reinterpret_cast<decltype(&plg_init)>(dlsym(handle, kSymbol));
    if (!fn) {
      dlclose(handle);
      continue;
    }

    Plugin plg;
    bool st = fn(&plg.info);
    if (!st) {
      dlclose(handle);
      continue;
    }

    std::string conf_path = kConfPath;
    conf_path += "/";
    conf_path += plg.info.name;
    conf_path += ".conf";

    plugin_read_conf(conf_path, plg.conf);
    g_plgs.push_back(plg);
  }

  closedir(dir);
  return true;
}
