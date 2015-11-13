//
// Created by wafer on 11/13/15.
//


#include <plg_api.h>
#include <sstream>
#include <iostream>

void send_SMS(std::string phone_no, std::string content) {
    std::string mPhoneNumber = phone_no;
    std::string mContent = content;
    std::string sendCommand = "adb shell am start -a android.intent.action.SENDTO -d sms:"
                              + mPhoneNumber
                              + " --es sms_body \""
                              + mContent
                              + "\" --ez exit_on_sent true";
    std::string pressCommand = "adb shell input keyevent 22";
    std::string releaseCommand = "adb shell input keyevent 66";

    system(sendCommand.c_str());
    system((pressCommand).c_str());
    system((releaseCommand).c_str());
}

std::string generateAuthCode() {

    long double sysTime = time(0);

    std::stringstream ss;
    ss << sysTime;

    return ss.str();
}

PlgCustomAuthResponse::PlgCustomAuthResult check(std::string generateAuthCode, std::string inputAuthCode) {

    if (generateAuthCode == inputAuthCode) {
        return PlgCustomAuthResponse::kSuccess;
    }
    else if(generateAuthCode != inputAuthCode) {
        return PlgCustomAuthResponse::kFail;
    }
    else {
        return PlgCustomAuthResponse::kError;
    }
}


PlgCustomAuthResponse smsAuth_callback(PlgConf &conf) {
    PlgCustomAuthResponse resp;

    std::string generateAuthCode = generateAuthCode();
    send_SMS(conf["userphone"] ,"Your SMS Auth Code is " + generateAuthCode);

    std::string inputAuthCode;
    std::cout<<"Please input your SMS Auth code"<<std::endl;
    std::cin>> inputAuthCode;

    resp.result = check(generateAuthCode,inputAuthCode);

    switch (resp.result) {
        case PlgCustomAuthResponse::kSuccess :
            resp.message = "Welcome";
            resp.result = PlgCustomAuthResponse::kSuccess;
            break;
        case PlgCustomAuthResponse::kFail :
            resp.message = "Incorrect code";
            resp.result = PlgCustomAuthResponse::kFail;
            break;
        case PlgCustomAuthResponse::kError :
            resp.message = "Unknown error";
            resp.result = PlgCustomAuthResponse::kError;
            break;
    }

    return resp;
}

bool plg_init(PlgInfo *info) {
    info->name = "smsauth";
    info->description = "The SMS Auth Way";
    info->auth_type = kCustomAuth;
    info->cust_auth_cb = smsAuth_callback;
    return true;
}
