#include <string>
#include <iostream>
#include <climits>

#include <unistd.h>
#include <pwd.h>
#include <signal.h>

#include <plg_api.h>
#include "plugins.h"
#include "verify.h"

bool check_login(struct passwd *pwd) {
  if (!pwd) {
    std::cout << "Invalid username" << std::endl;
    return false;
  }

  std::cout << "You can log on to the system using these ways: " << std::endl;
  for (int i = 0; i < g_plgs.size(); i++) {
    std::cout << i << ". "
      << g_plgs[i].name << "\t"
      << g_plgs[i].description << std::endl;
  }
  int method;

  while (true) {
    std::cout << std::endl << "Please choose a way to login: ";
    if (!(std::cin >> method) && (0 < method && method < g_plgs.size())) {
      std::cout << "Error input. " << std::endl;
      std::cin.clear();
      std::cin.ignore(INT_MAX, '\n');
    } else {
      break;
    }
  }

  VerifyResponse resp = verify_user(pwd->pw_name, g_plgs[method]);
  if (resp.result == resp.kSuccess) return true;
  std::cout << "Error when login: " << resp.message << std::endl;
  return false;
}
