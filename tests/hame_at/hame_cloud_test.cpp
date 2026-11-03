// Local answers for the Hame cloud endpoints (Marsrelay-compatible).

#include "hame_cloud.h"
#include "test_util.h"

using namespace esphome::hame_at;

static std::string answer(const std::string &url, const std::string &body = "", const LocalTime *t = nullptr,
                          int *status_out = nullptr) {
  int status = 0;
  std::string resp;
  bool ok = local_hame_answer(body.empty() ? "GET" : "POST", url, body, t, status, resp);
  if (status_out)
    *status_out = ok ? status : -1;
  return ok ? resp : "<none>";
}

int main() {
  EXPECT(is_hame_cloud_url("http://eu.hamedata.com/app/neng/getDateInfoeu.php?uid=x"));
  EXPECT(is_hame_cloud_url("https://marstekcloud.com/data-upload/x"));
  EXPECT(!is_hame_cloud_url("http://192.168.1.20/rpc/EM.GetStatus"));
  EXPECT(!is_hame_cloud_url("http://evilhamedata.com/x"));
  EXPECT(is_mcu_firmware_url("https://eu.hamedata.com/app/neng/admin/upload/B2500V23_All_V224.3.bin"));
  EXPECT(!is_mcu_firmware_url("http://eu.hamedata.com/app/Solar/put_tenmins_v2.php?x=a.bin"));

  // Clock reply: local time plus the real endpoint's four fixed fields.
  LocalTime t{2026, 11, 3, 9, 5, 7};
  EXPECT_STR(answer("http://eu.hamedata.com/app/neng/getDateInfoeu.php?uid=a&fcv=b", "", &t),
             "_2026_11_03_09_05_07_04_0_0_0");
  EXPECT_STR(answer("http://eu.hamedata.com/app/neng/getDateInfoeu.php?uid=a"), "<none>");  // no time yet

  EXPECT_STR(answer("http://eu.hamedata.com/app/Solar/puterrinfo.php", "err=1"), "_1");
  EXPECT_STR(answer("http://eu.hamedata.com/app/Solar/puterrinfo.php"), "_2");
  EXPECT_STR(answer("http://eu.hamedata.com/app/Solar/put_tenmins_v2.php?devid=x&soc=1"), "1");
  EXPECT_STR(answer("http://eu.hamedata.com/ems/api/v1/getDeviceFire?devid=x"),
             R"({"code":1,"show":0,"msg":"ok","data":{"id":255,"s":360,"c":30,"i":0,"p":800}})");
  EXPECT_STR(answer("http://eu.hamedata.com/ems/api/v1/getRealtimeSoc?devid=x&type=HMA-1"),
             R"({"code":1,"show":0,"msg":"ok","data":{"time_no":0}})");
  EXPECT_STR(answer("http://eu.hamedata.com/prod/api/v1/setB2500Report?v=abc"), R"({"code":1,"msg":"ok"})");
  EXPECT_STR(answer("http://eu.hamedata.com/prod/api/v1/setSmallReporting?v=abc"),
             R"({"code":1,"msg":"ok","data":{"de":1}})");
  EXPECT_STR(answer("http://eu.hamedata.com/prod/api/v1/setCtReporting?v=abc"), R"({"code":1,"msg":"success"})");
  EXPECT_STR(answer("http://eu.hamedata.com/ems/api/v2/setAcCoupleReporting?v=1"),
             R"({"code":1,"msg":"success","data":[]})");
  EXPECT_STR(answer("http://eu.hamedata.com/ems/api/v1/getDevicesApply?devid=x"),
             R"({"code":1,"msg":"success","data":[]})");
  int status = 0;
  answer("http://eu.hamedata.com/something/else", "", nullptr, &status);
  EXPECT_EQ(status, 404);
  return finish();
}
