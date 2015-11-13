#ifndef INCLUDE_PLG_API_H
#define INCLUDE_PLG_API_H
#include <string>
#include <map>

struct PlgSystemAuthResponse {
  bool result;
  std::string message;
};

struct PlgCustomAuthResponse {
  enum PlgCustomAuthResult {
    kSuccess,
    kFail,
    kError
  } result;
  std::string message;
};

enum PlgAuthType {
  kSystemAuth,
  kCustomAuth,
};

using PlgConf = std::map<std::string, std::string>;
using PlgSystemAuthCb = PlgSystemAuthResponse (*)(const PlgConf &sysconf, const PlgConf &plgconf);
using PlgCustomAuthCb = PlgCustomAuthResponse (*)(const PlgConf &sysconf, const PlgConf &plgconf);

struct PlgInfo {
  std::string name;
  PlgAuthType auth_type;

  PlgSystemAuthCb sys_auth_cb;
  PlgCustomAuthCb cust_auth_cb;
};

extern "C" bool plg_init(PlgInfo *info);

#endif
