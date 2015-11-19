#include <string>
#include <iostream>

#include <unistd.h>
#include <pwd.h>
#include <signal.h>

#include <plg_api.h>
#include "plugins.h"
#include "verify.h"

int main(int argc, char **argv) {
  // disable C-c and C-z

  // init plugin
  srand(time(0));
  plugin_load_all();

  // user name check and prompt
  std::string username;
  char hostname[64] = {0};
  bool hostname_ready = !gethostname(hostname, 64);
  struct passwd *pwd = NULL;
  VerifyResponse resp;

  // login attempt log
  int attempt = 0;

  while (true) {
    // clear TTY
    if (attempt == 0) {
      fprintf(stdout, "\033c");
    }

    while (true) {
      if (attempt < 3) {
        if (hostname_ready) {
          std::cout << hostname << " ";
        }
        std::cout << "login: ";
        std::cin >> username;
        pwd = getpwnam(username.c_str());
        if (pwd) { // The user exists in the system
          std::cout << std::endl;
          break;
        } else {
          std::cout << "User " << username << " does not exist on system."
          << std::endl
          << std::endl;
        }
      } else {
        attempt = 0;
        goto clear;
      }
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

    resp = verify_user(username, g_plgs[method]);
    if (resp.result == resp.kSuccess) {
      // flush status
      attempt = 0;
      std::cout << std::endl << resp.message << std::endl << std::endl;
      // spawn shell
      system(pwd->pw_shell);
      std::cout << std::endl << "Goodbye!" << std::endl;
      continue;
    } else {
      std::cout << "Error when login: " << resp.message << std::endl;
    }
    attempt++;
    attempt %= 3;
    clear:
    std::cout << std::endl;
  }
  return 0;
}
