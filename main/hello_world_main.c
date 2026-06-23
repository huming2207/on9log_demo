#define ON9_LOG_LOCAL_LEVEL 5

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "on9log.h"
#include "on9log_esp_vfs.h"
#include "sdkconfig.h"

static const char *TAG = "demo";

typedef struct {
    uint32_t start_count;
    uint32_t end_count;
    uint32_t payload_count;
    uint32_t payload_bytes;
} demo_sink_stats_t;

static demo_sink_stats_t s_demo_sink_stats;

static void demo_sink_start(const uint8_t *header, size_t header_len, void *ctx)
{
    demo_sink_stats_t *stats = (demo_sink_stats_t *)ctx;

    (void)header;
    (void)header_len;
    ++stats->start_count;
}

static void demo_sink_payload(const uint8_t *payload,
                              size_t payload_len,
                              size_t total_arg_cnt,
                              size_t curr_arg_index,
                              void *ctx)
{
    demo_sink_stats_t *stats = (demo_sink_stats_t *)ctx;

    (void)payload;
    (void)total_arg_cnt;
    (void)curr_arg_index;
    ++stats->payload_count;
    stats->payload_bytes += (uint32_t)payload_len;
}

static void demo_sink_end(void *ctx)
{
    demo_sink_stats_t *stats = (demo_sink_stats_t *)ctx;

    ++stats->end_count;
}

static const on9log_sink_t s_demo_sink = {
    .start_cb = demo_sink_start,
    .payload_cb = demo_sink_payload,
    .end_cb = demo_sink_end,
};

static void fill_pattern(uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        buf[i] = (uint8_t)(i * 37u + 11u);
    }
}

static void log_chip_info(void)
{
    esp_chip_info_t chip_info;
    uint32_t flash_size = 0;
    const char *target = CONFIG_IDF_TARGET;

    esp_chip_info(&chip_info);
    esp_err_t err = esp_flash_get_size(NULL, &flash_size);

    printf("plain text: hello from printf after on9log VFS init\n");
    fprintf(stderr, "plain text stderr: target=%s\n", target);
    ESP_LOGI(TAG, "ESP_LOG plain-text frame through esp_log_set_vprintf()");

    ON9_LOGI(TAG,
             "chip=%s cores=%d features wifi=%d bt=%d ble=%d ieee802154=%d",
             target,
             chip_info.cores,
             (chip_info.features & CHIP_FEATURE_WIFI_BGN) != 0,
             (chip_info.features & CHIP_FEATURE_BT) != 0,
             (chip_info.features & CHIP_FEATURE_BLE) != 0,
             (chip_info.features & CHIP_FEATURE_IEEE802154) != 0);

    if (err == ESP_OK) {
        ON9_LOGI(TAG,
                 "flash=%" PRIu32 " bytes min_heap=%" PRIu32 " bytes",
                 flash_size,
                 esp_get_minimum_free_heap_size());
    } else {
        ON9_LOGE(TAG, "esp_flash_get_size failed err=0x%x", (unsigned)err);
    }
}

static void log_format_cases(void)
{
    const char *name = "on9log";
    const char *null_string = NULL;
    const uint8_t pointer_target[] = {0xa5, 0xc0, 0xdb, 0x00};
    const uint8_t *pointer_arg = pointer_target;
    int signed_value = -42;
    unsigned unsigned_value = 0xfeedbeefu;
    long long signed_64 = -0x112233445566ll;
    unsigned long long unsigned_64 = 0x1122334455667788ull;
    int width = 10;
    int negative_width = -8;
    int precision = 5;
    const char *right = "right";
    const char *left = "left";
    const char *truncate = "truncate-me";

    ON9_LOGE(TAG, "level error signed=%d", signed_value);
    ON9_LOGW(TAG, "level warn unsigned=%u hex=%08x", unsigned_value, unsigned_value);
    ON9_LOGI(TAG, "level info string=%s null=%s", name, null_string);
    ON9_LOGD(TAG,
             "level debug ptr=%p bytes=%02x:%02x",
             pointer_arg,
             pointer_target[0],
             pointer_target[1]);
    ON9_LOGV(TAG, "level verbose ll=%lld ull=%llu", signed_64, unsigned_64);

    ON9_LOGI(TAG, "lengths hhd=%hhd hhu=%hhu hd=%hd hu=%hu", -1, 255, -1234, 54321);
    ON9_LOGI(TAG, "widths [%*s] [%-*s] [%.*s]", width, right, negative_width, left, precision, truncate);
    ON9_LOGI(TAG, "percent literal 100%% and char=%c", 'Z');
    ON9_LOGI(TAG, "empty log with no printf args");
}

static void log_buffer_cases(void)
{
    uint8_t small[] = {
        0x00, 0x01, 0x02, 0x03, 0x7f, 0x80, 0xa5, 0xc0,
        0xdb, 0xde, 0xdd, 0xdc, 0xf0, 0x55, 0xaa, 0xff,
        'O',  'N',  '9',  '\n',
    };
    static uint8_t large[3200];

    ON9_LOG_BUFI(TAG, small, sizeof(small));
    ON9_LOG_BUFI(TAG, NULL, 0);

    fill_pattern(large, sizeof(large));
    ON9_LOGI(TAG, "next buffer intentionally exceeds 3072-byte VFS transport cap");
    ON9_LOG_BUFI(TAG, large, sizeof(large));
    ON9_LOGI(TAG, "if host is in sync, this line follows the intentional oversized-frame drop");

    on9log_write_buffer(ON9_LOG_LEVEL_INFO, TAG, NULL, 1);
    ON9_LOGI(TAG, "invalid NULL buffer produced dropped_count=%" PRIu32, on9log_get_dropped_count());
}

static void log_sink_cases(void)
{
    memset(&s_demo_sink_stats, 0, sizeof(s_demo_sink_stats));
    const char *buffer_text = "sink-buffer";
    on9log_err_t add_err = on9log_add_sink(&s_demo_sink, &s_demo_sink_stats);
    ON9_LOGI(TAG, "secondary sink add err=%d", (int)add_err);

    ON9_LOGI(TAG, "secondary sink sees this formatted packet value=%d", 1234);
    ON9_LOG_BUFI(TAG, buffer_text, strlen(buffer_text));

    on9log_err_t remove_err = on9log_remove_sink(&s_demo_sink, &s_demo_sink_stats);
    ON9_LOGI(
        TAG,
        "secondary sink remove err=%d starts=%" PRIu32 " ends=%" PRIu32 " payload_cb=%" PRIu32 " payload_bytes=%" PRIu32,
        (int)remove_err,
        s_demo_sink_stats.start_count,
        s_demo_sink_stats.end_count,
        s_demo_sink_stats.payload_count,
        s_demo_sink_stats.payload_bytes);
}

static void worker_task(void *arg)
{
    const char *task_name = (const char *)arg;

    for (int i = 0; i < 5; ++i) {
        printf("plain text worker %s iteration %d\n", task_name, i);
        ON9_LOGI("worker",
                 "task=%s iteration=%d tick=%" PRIu32,
                 task_name,
                 i,
                 (uint32_t)xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(150));
    }

    ON9_LOGW("worker", "task=%s finished", task_name);
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t err = on9log_esp_vfs_init();
    if (err != ESP_OK) {
        printf("on9log_esp_vfs_init failed: 0x%x\n", (unsigned)err);
        return;
    }

    ON9_LOGI(TAG, "on9log demo start local_level=%d", ON9_LOG_LOCAL_LEVEL);
    log_chip_info();
    log_format_cases();
    log_buffer_cases();
    log_sink_cases();

    BaseType_t task_ok = xTaskCreate(worker_task, "on9log_demo_worker", 4096, "A", tskIDLE_PRIORITY + 1, NULL);
    ON9_LOGI(TAG, "worker task create result=%d", (int)task_ok);

    for (uint32_t heartbeat = 0; heartbeat < 6; ++heartbeat) {
        ON9_LOGI(TAG,
                 "main heartbeat=%" PRIu32 " free_heap=%" PRIu32,
                 heartbeat,
                 esp_get_free_heap_size());
        printf("plain text main heartbeat %" PRIu32 "\n", heartbeat);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ON9_LOGI(TAG, "on9log demo complete; continuing slow heartbeat");
    while (true) {
        ON9_LOGI(TAG, "slow heartbeat tick=%" PRIu32, (uint32_t)xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
