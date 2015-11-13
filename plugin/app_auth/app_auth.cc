#include <plg_api.h>
#include <stdlib.h>
#include <pthread.h>

static void *thread_func(void *v) {
  system("php -S 0.0.0.0:8000 -t ../../php");
  return v;
}

PlgSystemAuthResponse sysauth_callback(PlgConf &conf) {
  PlgSystemAuthResponse state;

  FILE *f = fopen("php_include.php", "w");
  fprintf(f, "$username = %s;", conf.at("username").c_str());
  fflush(f);
  fclose(f);

  f = fopen("../../php/login", "w");
  fclose(f);

  pthread_t tid;
  pthread_create(&tid, NULL, thread_func, NULL);
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
