#include <string>
#include <iostream>

#include <pwd.h>
#include <security/pam_appl.h>

#include <plg_api.h>
#include "plugins.h"
#include "verify.h"

extern "C" {

int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc,
                        const char **argv) {
  fflush(stdin);
  std::cout << "called 1" << std::endl;
  if (g_plgs.empty()) {
    std::cout << plugin_load_all() << std::endl;
  }

  const char *username = NULL;
  int pam_ret = pam_get_user(pamh, &username, NULL);//pam_get_item(pamh, PAM_USER, (const void **) username);
  std::cout << pam_ret << username << std::endl;
  struct passwd *pwd = getpwnam(username);
  if (!pwd) {
    return PAM_USER_UNKNOWN;
  }

  int method;
  std::cout << std::endl << "Please choose a way to login: ";
  if (!(std::cin >> method) && (0 < method && method < g_plgs.size())) {
    return PAM_AUTH_ERR;
  }
  VerifyResponse resp;
  resp = verify_user(username, g_plgs[method]);
  switch (resp.result) {
    case VerifyResponse::kSuccess:
      break;
    case VerifyResponse::kFail:
      std::cout << resp.message << std::endl;
      return PAM_AUTH_ERR;
    case VerifyResponse::kError:
      std::cout << resp.message << std::endl;
      return PAM_SERVICE_ERR;
  }

  pam_set_item(pamh, PAM_USER, (const void **) username);
  return PAM_SUCCESS;
}

int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc,
                     const char **argv) {
  std::cout << "called 2" << std::endl;
  return PAM_SUCCESS;
}

int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv) {
  std::cout << "called 3" << std::endl;
  return PAM_SUCCESS;
}

}