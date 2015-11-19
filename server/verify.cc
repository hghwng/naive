#include <string>
#include <unistd.h>
#include "verify.h"
#include "plugins.h"
#include "util.h"

bool verify_username(const std::string &username) {
  for (auto i : username) {
    if (!isalpha(i) && !isdigit(i) && i != '_') return false;
  }
  return true;
}

PlgConf verify_load_user(const std::string &username, PlgConf &conf) {
  static const char *kConfPath = "/opt/naive/user/";

  std::string path = kConfPath;
  path += username;
  path += ".conf";

  util_read_conf(path, conf);
  return conf;
}

VerifyResponse verify_user_sysauth(std::string username,
    const PlgInfo &plg, PlgConf &conf) {
  VerifyResponse resp;

  PlgSystemAuthResponse sys_auth_resp = plg.sys_auth_cb(conf);
  if (!sys_auth_resp.result) {
    resp.message = sys_auth_resp.message;
    resp.result = VerifyResponse::kFail;
    return resp;
  }

  static const char *kBasePath = "/opt/naive/states/";
  const int kMaxTry = 15;
  std::string userst_path = kBasePath;
  userst_path += username;

  for (int i = 0; i < kMaxTry; ++i) {
    if (unlink(userst_path.c_str()) == 0) {
      resp.message = "Success";
      resp.result = VerifyResponse::kSuccess;
      return resp;
    }
    usleep(200);
  }

  resp.message = "Operation timeout";
  resp.result = VerifyResponse::kFail;
  return resp;
}

VerifyResponse verify_user(const std::string &username, const PlgInfo &plg) {
  if (!verify_username(username)) {
    VerifyResponse resp;
    resp.message = "Invalid username";
    resp.result = VerifyResponse::kError;
    return resp;
  }

  PlgConf conf;
  verify_load_user(username, conf);
  conf["username"] = username;

  switch (plg.auth_type) {
    case kSystemAuth:
      return verify_user_sysauth(username, plg, conf);
    case kCustomAuth:
      return plg.cust_auth_cb(conf);
  }
}
