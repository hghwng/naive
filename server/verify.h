#ifndef SERVER_VERIFY_H
#define SERVER_VERIFY_H
#include <plg_api.h>

using VerifyResponse = PlgCustomAuthResponse;
VerifyResponse verify_user(const std::string &username, const PlgInfo &plg);
#endif
