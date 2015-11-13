#include <string>
#include <plg_api.h>
#include <iostream>
#include "plugins.h"
#include "verify.h"

int main(int argc, char **argv) {
  srand(time(0));
  plugin_load_all();
  std::string username = "abc";
  VerifyResponse resp = verify_user(username, g_plgs[0]);
  std::cout << resp.result << std::endl;
  std::cout << resp.message << std::endl;
  return 0;
}
