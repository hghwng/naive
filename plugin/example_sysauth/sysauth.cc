#include <plugin.h>

PlgSystemAuthResponse sysauth_callback(
    const PlgConf &sysconf, const PlgConf &plgconf) {
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
  info->auth_type = kSystemAuth;
  info->sys_auth_cb = sysauth_callback;
  return true;
}
