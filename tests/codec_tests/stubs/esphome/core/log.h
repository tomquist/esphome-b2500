#pragma once

#include <cstdio>

#define B2500_TEST_LOG(level, tag, fmt, ...) printf("[" level "][%s] " fmt "\n", tag, ##__VA_ARGS__)

#define ESP_LOGE(tag, fmt, ...) B2500_TEST_LOG("E", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) B2500_TEST_LOG("W", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) B2500_TEST_LOG("I", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) B2500_TEST_LOG("D", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) B2500_TEST_LOG("V", tag, fmt, ##__VA_ARGS__)
