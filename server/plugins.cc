#include <dirent.h>
#include <dlfcn.h>

#include "plugins.h"
#include "util.h"

std::vector<PlgInfo> g_plgs;

bool plugin_load_all() {
  static const char *kPluginPath = "/Users/kirito/ClionProjects/naive/build/bin/plugins/";
  static const char *kSymbol = "plg_init";

  DIR *dir = opendir(kPluginPath);
  if (dir == NULL) return false;

  struct dirent *ent;
  while ((ent = readdir(dir)) != NULL) {
    std::string so_path = kPluginPath;
    so_path += ent->d_name;
    printf("%s\n", so_path.c_str());
    auto handle = dlopen(so_path.c_str(), RTLD_LAZY);
    if (!handle) continue;

    auto fn = reinterpret_cast<decltype(&plg_init)>(dlsym(handle, kSymbol));
    if (!fn) {
      dlclose(handle);
      continue;
    }

    PlgInfo info;
    bool st = fn(&info);
    if (!st) {
      dlclose(handle);
      continue;
    }

    g_plgs.push_back(info);
  }

  closedir(dir);
  return true;
}
