#include <plg_api.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <security/pam_appl.h>

static char *g_password = NULL;

static int conv(int n, const struct pam_message **msg, struct pam_response **resp, void *data) {
  struct pam_response *addr = (struct pam_response *)calloc((size_t) n, sizeof(pam_response));
  resp[0] = addr;
  for (int i = 0; i < n; i++) {
    if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF) {
      resp[i]->resp = g_password;
      resp[i]->resp_retcode = 0;
    }
  }
  return 0;
}

static PlgCustomAuthResponse::PlgCustomAuthResult check(std::string username, std::string password) {
  pam_handle_t *pamh;
  struct pam_conv pamc;
  pamc.conv = conv;
  pamc.appdata_ptr = NULL;
  int pam_err;
  const char *user = username.c_str();
  if (g_password) {
    free(g_password);
  }
  g_password = (char *) malloc(sizeof(char) * (password.length() + 1));
  strcpy(g_password, password.c_str());

  pam_err = pam_start("login", user, &pamc, &pamh);
  if (pam_err) {
    return PlgCustomAuthResponse::kError;
  }
  pam_err = pam_set_item(pamh, PAM_RUSER, user);
  if (pam_err) {
    pam_end(pamh, pam_err);
    return PlgCustomAuthResponse::kError;
  }
  pam_err = pam_authenticate(pamh, 0);
  if (pam_err) {
    pam_end(pamh, pam_err);
    return PlgCustomAuthResponse::kFail;
  }
  return PlgCustomAuthResponse::kSuccess;
}

PlgCustomAuthResponse pam_callback(PlgConf &conf) {
  PlgCustomAuthResponse resp;
  std::string password;
  std::cout << "Password: ";
  std::cin >> password;
  resp.result = check(conf["username"], password);

  switch (resp.result) {
    case PlgCustomAuthResponse::kSuccess:
      resp.message = "Welcome";
      break;
    case PlgCustomAuthResponse::kFail:
      resp.message = "Error username or password";
      break;
    case PlgCustomAuthResponse::kError:
      resp.message = "Unknown error";
      break;
  }

  return resp;
}

bool plg_init(PlgInfo *info) {
  info->name = "pam_login";
  info->description = "PAM native login plugin";
  info->auth_type = kCustomAuth;
  info->cust_auth_cb = pam_callback;
  return true;
}
