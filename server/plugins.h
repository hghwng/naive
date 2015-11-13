#ifndef SERVER_PLUGINS_H
#define SERVER_PLUGINS_H
#include <vector>
#include <string>
#include <plg_api.h>

extern std::vector<PlgInfo> g_plgs;
bool plugin_load_all();

#endif
