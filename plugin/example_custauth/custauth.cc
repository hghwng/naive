#include <plg_api.h>
#include <stdlib.h>

PlgCustomAuthResponse pam_callback(const PlgConf &conf) {
  PlgCustomAuthResponse resp;

  switch (rand() % 3) {
    case 0:
      resp.message = "Welcome";
      resp.result = PlgCustomAuthResponse::kSuccess;
      break;
    case 1:
      resp.message = "Incorrect code";
      resp.result = PlgCustomAuthResponse::kFail;
      break;
    case 2:
      resp.message = "Unknown error";
      resp.result = PlgCustomAuthResponse::kError;
      break;
  }

  return resp;
}

bool plg_init(PlgInfo *info) {
  info->name = "custauth";
  info->description = "An example for customized authentication";
  info->auth_type = kCustomAuth;
  info->cust_auth_cb = pam_callback;
  return true;
}
