#include <plg_api.h>
#include <stdlib.h>
#include <pthread.h>

PlgSystemAuthResponse sysauth_callback(PlgConf &conf) {
  PlgSystemAuthResponse state;

  FILE *f = fopen("php_include.php", "w");
  fprintf(f, "<?php $username = '%s'; ?>", conf.at("username").c_str());
  fflush(f);
  fclose(f);

  f = fopen("../../php/login", "w");
  fclose(f);

  state.result = true;
  state.message = "wow, such php, best language";
  return state;
}

bool plg_init(PlgInfo *info) {
  info->name = "app_auth";
  info->description = "Authenticate by an Android App";
  info->auth_type = kSystemAuth;
  info->sys_auth_cb = sysauth_callback;
  return true;
}
