#include <err.h>
#include <inttypes.h>
#include <signal.h>
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <nfc/nfc.h>
#include <nfc/nfc-types.h>

#include <string>
#include <plg_api.h>

using namespace std;

static nfc_device *pnd = NULL;
static nfc_context *context;

static void stop_polling(int sig)
{
  (void) sig;
  if (pnd != NULL)
    nfc_abort_command(pnd);
  else {
    nfc_exit(context);
  }
}

string print_nfc_target(const nfc_target *pnt)
{
  nfc_target_info nti = pnt->nti;
  nfc_iso14443a_info nai = nti.nai;
  char buf[10];
  int offset = 0;
  for (int i = 0; i<nai.szUidLen; ++i)
	offset += sprintf(buf + offset, "%02x", nai.abtUid[i]);
  buf[offset]=0;
  string str(buf);
  return str;
}

int poll(nfc_target *nt)
{
  bool verbose = false;

  signal(SIGINT, stop_polling);

  // Display libnfc version
  const char *acLibnfcVersion = nfc_version();

  const uint8_t uiPollNr = 20;
  const uint8_t uiPeriod = 2;
  const nfc_modulation nmModulations[1] = {
    { .nmt = NMT_ISO14443A, .nbr = NBR_106 }
  };
  const size_t szModulations = 1;

  //nfc_target nt;
  int res = 0;

  nfc_init(&context);
  if (context == NULL) {
    return -1;
  }

  pnd = nfc_open(context, NULL);

  if (pnd == NULL) {
    nfc_exit(context);
    return -1;
  }

  if (nfc_initiator_init(pnd) < 0) {
    nfc_close(pnd);
    nfc_exit(context);
    return -1;
  }

  if ((res = nfc_initiator_poll_target(pnd, nmModulations, szModulations, uiPollNr, uiPeriod, nt))  < 0) {
    nfc_close(pnd);
    nfc_exit(context);
    return -1;
  }

 while (0 == nfc_initiator_target_is_present(pnd, NULL)) {}

  nfc_close(pnd);
  nfc_exit(context);
  return res;
}

PlgCustomAuthResponse nfc_login_callback(PlgConf &conf)
{
  PlgCustomAuthResponse resp;
  nfc_target nt;
  int res = poll(&nt);
  if(res <= 0)
  {
    resp.message = "NFC communicarion error";
    resp.result = PlgCustomAuthResponse::kError;
  } else {
    string uid = print_nfc_target(&nt);
    if(conf["nfcid"] == uid)
    {
      resp.message = "Success";
      resp.result = PlgCustomAuthResponse::kSuccess;
    } else {
      resp.message = "Incorrect UID";
      resp.result = PlgCustomAuthResponse::kFail;
    }
  }
  return resp;
}

bool plg_init(PlgInfo *info) {
  info->name = "nfc_auth";
  info->description = "NFC Authentication Plugin";
  info->auth_type = kCustomAuth;
  info->cust_auth_cb = nfc_login_callback;
  freopen("/tmp/nfcLog","w",stderr);
  return true;
}
