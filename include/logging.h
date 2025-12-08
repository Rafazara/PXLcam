#pragma once

#include <esp_log.h>

#ifndef PXLCAM_LOG_TAG
#define PXLCAM_LOG_TAG "pxlcam"
#endif

#define PXLCAM_LOGI(fmt, ...) ESP_LOGI(PXLCAM_LOG_TAG, fmt, ##__VA_ARGS__)
#define PXLCAM_LOGW(fmt, ...) ESP_LOGW(PXLCAM_LOG_TAG, fmt, ##__VA_ARGS__)
#define PXLCAM_LOGE(fmt, ...) ESP_LOGE(PXLCAM_LOG_TAG, fmt, ##__VA_ARGS__)

#define PXLCAM_LOGI_TAG(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define PXLCAM_LOGW_TAG(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define PXLCAM_LOGE_TAG(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
