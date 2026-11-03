#include "hame_cloud.h"

#include <cstdio>

namespace esphome::hame_at {

// The four fixed fields after the real endpoint's clock reply.
static const char *const TIME_SUFFIX = "04_0_0_0";
// What the real cloud tells every B2500 about its output limits.
static const char *const DEVICE_FIRE = R"({"code":1,"show":0,"msg":"ok","data":{"id":255,"s":360,"c":30,"i":0,"p":800}})";
// No charge level: the battery keeps its own estimate (the real cloud would
// tell an unknown device 0 %).
static const char *const REALTIME_SOC = R"({"code":1,"show":0,"msg":"ok","data":{"time_no":0}})";
static const char *const OK_JSON = R"({"code":1,"msg":"success","data":[]})";

static std::string host_of(const std::string &url) {
  size_t start = url.find("://");
  start = start == std::string::npos ? 0 : start + 3;
  size_t end = url.find_first_of(":/?", start);
  return url.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

static std::string path_of(const std::string &url) {
  size_t start = url.find("://");
  start = start == std::string::npos ? 0 : start + 3;
  size_t slash = url.find('/', start);
  if (slash == std::string::npos)
    return "/";
  size_t q = url.find('?', slash);
  return url.substr(slash, q == std::string::npos ? std::string::npos : q - slash);
}

static bool ends_with(const std::string &s, const std::string &suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static bool starts_with(const std::string &s, const std::string &prefix) { return s.rfind(prefix, 0) == 0; }

bool is_hame_cloud_url(const std::string &url) {
  std::string host = host_of(url);
  return host == "hamedata.com" || ends_with(host, ".hamedata.com") || host == "marstekcloud.com" ||
         ends_with(host, ".marstekcloud.com");
}

bool is_mcu_firmware_url(const std::string &url) {
  return is_hame_cloud_url(url) && ends_with(path_of(url), ".bin");
}

bool local_hame_answer(const std::string & /*method*/, const std::string &url, const std::string &body,
                       const LocalTime *now, int &status, std::string &response) {
  const std::string path = path_of(url);
  status = 200;
  // getDateInfo*.php: "_YYYY_MM_DD_hh_mm_ss_" in the server's local time plus four fixed fields.
  if (starts_with(path, "/app/neng/getDateInfo") && ends_with(path, ".php")) {
    if (now == nullptr)
      return false;
    char buf[48];
    snprintf(buf, sizeof(buf), "_%04d_%02d_%02d_%02d_%02d_%02d_%s", now->year, now->month, now->day, now->hour,
             now->minute, now->second, TIME_SUFFIX);
    response = buf;
    return true;
  }
  if (path == "/app/Solar/puterrinfo.php") {
    response = body.empty() ? "_2" : "_1";
    return true;
  }
  if (starts_with(path, "/app/Solar/put")) {  // put_tenmins_v2.php
    response = "1";
    return true;
  }
  if (path == "/ems/api/v1/getDeviceFire") {
    response = DEVICE_FIRE;
    return true;
  }
  if (path == "/ems/api/v1/getRealtimeSoc") {
    response = REALTIME_SOC;
    return true;
  }
  if (path == "/prod/api/v1/setB2500Report") {
    response = R"({"code":1,"msg":"ok"})";
    return true;
  }
  if (path == "/prod/api/v1/setSmallReporting") {
    response = R"({"code":1,"msg":"ok","data":{"de":1}})";
    return true;
  }
  // Other reports (/prod|ems/api/v1|v2/set...Report), getters and external services.
  bool report = (starts_with(path, "/prod/api/v") || starts_with(path, "/ems/api/v")) && ends_with(path, "Report");
  if (report || starts_with(path, "/ems/api/v1/get") || starts_with(path, "/external-services/api/v1/") ||
      starts_with(path, "/data-upload/")) {
    response = OK_JSON;
    return true;
  }
  if (ends_with(path, "/getWeatherInfo.php")) {
    response = "{}";
    return true;
  }
  status = 404;
  response.clear();
  return true;
}

}  // namespace esphome::hame_at
