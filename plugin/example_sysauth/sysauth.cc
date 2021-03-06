#include <plg_api.h>
#include <stdlib.h>

PlgSystemAuthResponse sysauth_callback(const PlgConf &conf) {
  PlgSystemAuthResponse state;

  switch (rand() % 2) {
    case 0:
      state.message = "SMS sent";
      state.result = true;
      break;
    case 1:
      state.message = "SMS send error";
      state.result = false;
      break;
  }

  return state;
}

bool plg_init(PlgInfo *info) {
  info->name = "sysauth";
  info->description = "Authenticate by system";
  info->auth_type = kSystemAuth;
  info->sys_auth_cb = sysauth_callback;
  return true;
}
