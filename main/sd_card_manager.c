#include "sd_card_manager.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"

#include "esp_http_client.h"

static const char *TAG = "SD_CARD";

// SD 卡配置
#define MOUNT_POINT "/sdcard"

// SD 卡引腳配置（方案 A：自訂 GPIO）
#define PIN_NUM_MISO  8   // 自訂 ⭐
#define PIN_NUM_MOSI  2   // 自訂 ⭐
#define PIN_NUM_CLK   3   // 自訂 ⭐
#define PIN_NUM_CS    1   // 自訂 ⭐

static sdmmc_card_t *card = NULL;
static bool is_mounted = false;

// WAV 文件頭結構
typedef struct {
    char riff[4];           // "RIFF"
    uint32_t chunk_size;    // 文件大小 - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmt_size;      // fmt chunk 大小 (16)
    uint16_t audio_format;  // PCM = 1
    uint16_t num_channels;  // 單聲道 = 1
    uint32_t sample_rate;   // 採樣率
    uint32_t byte_rate;     // 字節率
    uint16_t block_align;   // 塊對齊
    uint16_t bits_per_sample; // 位深度
    char data[4];           // "data"
    uint32_t data_size;     // 數據大小
} __attribute__((packed)) wav_header_t;

esp_err_t sd_card_init(void) {
    ESP_LOGI(TAG, "初始化 SD 卡 (SPI 模式)...");
    
    // VFS 掛載配置
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,  // 不要自動格式化（避免意外）
        .max_files = 5,
        .allocation_unit_size = 0  // 使用預設值
    };
    
    // SPI 總線配置
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    // 初始化 SPI 總線
    ESP_LOGI(TAG, "初始化 SPI 總線 (SPI2_HOST)...");
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "⚠️ SPI 總線已初始化，繼續...");
        } else {
            ESP_LOGE(TAG, "❌ SPI 總線初始化失敗: %s", esp_err_to_name(ret));
            return ret;
        }
    } else {
        ESP_LOGI(TAG, "✅ SPI 總線初始化成功");
    }
    
    // SDSPI 設備配置
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = SPI2_HOST;
    
    // SDSPI 主機配置
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 10000;  // 降低到 10 MHz（更穩定，避免 I/O 錯誤）
    
    // 掛載 SD 卡
    ESP_LOGI(TAG, "掛載文件系統...");
    ESP_LOGI(TAG, "配置: CS=GPIO%d, MOSI=GPIO%d, MISO=GPIO%d, CLK=GPIO%d", 
             PIN_NUM_CS, PIN_NUM_MOSI, PIN_NUM_MISO, PIN_NUM_CLK);
    
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ SD 卡掛載失敗: %s (0x%x)", esp_err_to_name(ret), ret);
        
        if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "原因: 通訊超時 - 檢查接線和 SD 卡");
        } else if (ret == ESP_ERR_INVALID_RESPONSE) {
            ESP_LOGE(TAG, "原因: SD 卡無響應 - SD 卡可能損壞或未插好");
        } else if (ret == ESP_ERR_INVALID_CRC) {
            ESP_LOGE(TAG, "原因: CRC 錯誤 - 接線問題或 SD 卡損壞");
        } else if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "原因: 掛載失敗 - SD 卡未格式化為 FAT32");
        }
        
        ESP_LOGE(TAG, "建議:");
        ESP_LOGE(TAG, "1. 檢查 SD 卡是否插好");
        ESP_LOGE(TAG, "2. 檢查接線（特別是 MISO/MOSI）");
        ESP_LOGE(TAG, "3. 用電腦格式化 SD 卡為 FAT32");
        ESP_LOGE(TAG, "4. 嘗試更換 SD 卡");
        
        spi_bus_free(SPI2_HOST);
        return ret;
    }
    
    is_mounted = true;
    
    // 打印 SD 卡信息
    sdmmc_card_print_info(stdout, card);
    ESP_LOGI(TAG, "✅ SD 卡掛載成功: %s", MOUNT_POINT);
    
    // 顯示容量
    uint64_t card_size = ((uint64_t) card->csd.capacity) * card->csd.sector_size;
    ESP_LOGI(TAG, "SD 卡容量: %llu MB", card_size / (1024 * 1024));
    
    // 測試掛載點
    struct stat st;
    if (stat(MOUNT_POINT, &st) == 0) {
        ESP_LOGI(TAG, "掛載點狀態: 存在, 權限: %o", st.st_mode);
    } else {
        ESP_LOGE(TAG, "❌ 掛載點不可訪問");
        is_mounted = false;
        return ESP_FAIL;
    }
    
    // 立即測試寫入
    ESP_LOGI(TAG, "🧪 測試寫入功能...");
    
    // 嘗試多種路徑格式
    const char* test_paths[] = {
        "/sdcard/test.txt",
        "0:/test.txt",  // FAT 驅動器格式
        "/sdcard/TEST.TXT"  // 大寫（FAT 相容）
    };
    
    FILE* test_f = NULL;
    for (int i = 0; i < 3; i++) {
        ESP_LOGI(TAG, "嘗試路徑 %d: %s", i+1, test_paths[i]);
        test_f = fopen(test_paths[i], "w");
        if (test_f != NULL) {
            ESP_LOGI(TAG, "✓ 路徑 %d 成功！", i+1);
            break;
        }
        ESP_LOGW(TAG, "✗ 路徑 %d 失敗 (errno: %d - %s)", i+1, errno, strerror(errno));
    }
    if (test_f == NULL) {
        ESP_LOGE(TAG, "❌ 初始化測試寫入失敗 (errno: %d - %s)", errno, strerror(errno));
        ESP_LOGE(TAG, "SD 卡可能是唯讀或未正確掛載");
        
        // 嘗試其他診斷
        ESP_LOGI(TAG, "🔍 進行額外診斷...");
        
        // 檢查是否可以讀取根目錄
        DIR* test_dir = opendir(MOUNT_POINT);
        if (test_dir) {
            ESP_LOGI(TAG, "✓ 可以打開根目錄");
            closedir(test_dir);
        } else {
            ESP_LOGE(TAG, "✗ 無法打開根目錄");
        }
        
        // 檢查卡片信息
        if (card) {
            ESP_LOGI(TAG, "SD 卡信息:");
            ESP_LOGI(TAG, "  容量: %llu MB", 
                     ((uint64_t)card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
            ESP_LOGI(TAG, "  扇區大小: %d bytes", card->csd.sector_size);
        }
        
        is_mounted = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✓ 文件打開成功");
    int write_ret = fprintf(test_f, "SD card init test\n");
    ESP_LOGI(TAG, "✓ 寫入返回: %d bytes", write_ret);
    fclose(test_f);
    
    // 驗證文件是否真的存在
    struct stat file_stat;
    if (stat("/sdcard/init_test.txt", &file_stat) == 0) {
        ESP_LOGI(TAG, "✓ 文件已創建，大小: %ld bytes", file_stat.st_size);
    } else {
        ESP_LOGW(TAG, "⚠️ 文件創建但無法 stat");
    }
    
    remove("/sdcard/init_test.txt");
    ESP_LOGI(TAG, "✅ 寫入測試成功");
    
    return ESP_OK;
}

void sd_card_deinit(void) {
    if (is_mounted) {
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
        spi_bus_free(SPI2_HOST);
        ESP_LOGI(TAG, "SD 卡已卸載");
        is_mounted = false;
    }
}

bool sd_is_mounted(void) {
    return is_mounted;
}

esp_err_t sd_save_audio_wav(const char* filename, 
                            const int16_t* audio_data, 
                            size_t audio_len,
                            uint32_t sample_rate) {
    if (!is_mounted) {
        ESP_LOGE(TAG, "❌ SD 卡未掛載");
        return ESP_ERR_INVALID_STATE;
    }
    
    char filepath[300];  // 增加緩衝區大小
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, filename);
    
    ESP_LOGI(TAG, "💾 保存音頻到: %s", filepath);
    
    // 創建 WAV 頭
    wav_header_t header;
    uint32_t data_size = audio_len * sizeof(int16_t);
    
    memcpy(header.riff, "RIFF", 4);
    header.chunk_size = 36 + data_size;
    memcpy(header.wave, "WAVE", 4);
    memcpy(header.fmt, "fmt ", 4);
    header.fmt_size = 16;
    header.audio_format = 1;  // PCM
    header.num_channels = 1;  // 單聲道
    header.sample_rate = sample_rate;
    header.byte_rate = sample_rate * 1 * 2;
    header.block_align = 1 * 2;
    header.bits_per_sample = 16;
    memcpy(header.data, "data", 4);
    header.data_size = data_size;
    
    // 打開文件
    FILE* f = fopen(filepath, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "❌ 無法創建文件: %s", filepath);
        return ESP_FAIL;
    }
    
    // 寫入 WAV 頭
    size_t written = fwrite(&header, 1, sizeof(header), f);
    if (written != sizeof(header)) {
        ESP_LOGE(TAG, "❌ 寫入 WAV 頭失敗");
        fclose(f);
        return ESP_FAIL;
    }
    
    // 寫入音頻數據
    written = fwrite(audio_data, 1, data_size, f);
    if (written != data_size) {
        ESP_LOGE(TAG, "❌ 寫入音頻數據失敗");
        fclose(f);
        return ESP_FAIL;
    }
    
    fclose(f);
    
    ESP_LOGI(TAG, "✅ 音頻保存成功: %s (%u bytes)", filename, data_size + sizeof(header));
    return ESP_OK;
}

void sd_list_files(void) {
    if (!is_mounted) {
        ESP_LOGW(TAG, "SD 卡未掛載");
        return;
    }
    
    ESP_LOGI(TAG, "📁 SD 卡文件列表:");
    
    struct dirent *entry;
    DIR *dir = opendir(MOUNT_POINT);
    
    if (dir == NULL) {
        ESP_LOGE(TAG, "無法打開目錄");
        return;
    }
    
    int file_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        char filepath[300];  // 增加緩衝區大小以容納長檔名
        snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, entry->d_name);
        
        struct stat st;
        if (stat(filepath, &st) == 0) {
            ESP_LOGI(TAG, "  %s (%ld bytes)", entry->d_name, st.st_size);
            file_count++;
        }
    }
    
    closedir(dir);
    ESP_LOGI(TAG, "總共 %d 個文件", file_count);
}

esp_err_t sd_delete_file(const char* filename) {
    if (!is_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    
    char filepath[300];  // 增加緩衝區大小
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, filename);
    
    if (unlink(filepath) == 0) {
        ESP_LOGI(TAG, "✅ 文件已刪除: %s", filename);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "❌ 刪除文件失敗: %s", filename);
        return ESP_FAIL;
    }
}

// HTTP 事件處理器（用於下載）
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            // 數據會在主函數中處理
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t sd_download_wav(const char* url, const char* filename) {
    if (!is_mounted) {
        ESP_LOGE(TAG, "❌ SD 卡未掛載");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "📥 開始下載: %s", url);
    ESP_LOGI(TAG, "💾 保存到: %s", filename);
    
    char filepath[300];  // 增加緩衝區大小
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, filename);
    
    // 打開文件準備寫入（使用二進位模式）
    ESP_LOGI(TAG, "嘗試創建文件: %s", filepath);
    FILE* f = fopen(filepath, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "❌ 無法創建文件: %s (errno: %d - %s)", filepath, errno, strerror(errno));
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ 文件創建成功");
    
    // 配置 HTTP 客戶端
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = 30000,
        .buffer_size = 4096,
        .skip_cert_common_name_check = true,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "❌ HTTP 客戶端初始化失敗");
        fclose(f);
        return ESP_FAIL;
    }
    
    // 開始 HTTP GET 請求
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ HTTP 連線失敗: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        fclose(f);
        return err;
    }
    
    // 獲取內容長度
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    ESP_LOGI(TAG, "📊 HTTP 狀態: %d, 檔案大小: %d bytes (%.1f KB)", 
             status_code, content_length, (float)content_length / 1024);
    
    if (status_code != 200) {
        ESP_LOGE(TAG, "❌ HTTP 錯誤: %d", status_code);
        esp_http_client_cleanup(client);
        fclose(f);
        return ESP_FAIL;
    }
    
    // 分塊下載並寫入 SD 卡
    char *buffer = malloc(4096);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "❌ 記憶體分配失敗");
        esp_http_client_cleanup(client);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    
    int total_read = 0;
    int last_progress = -1;
    
    while (1) {
        int read_len = esp_http_client_read(client, buffer, 4096);
        if (read_len < 0) {
            ESP_LOGE(TAG, "❌ 讀取數據失敗");
            free(buffer);
            esp_http_client_cleanup(client);
            fclose(f);
            return ESP_FAIL;
        }
        
        if (read_len == 0) {
            break;  // 下載完成
        }
        
        // 寫入 SD 卡
        size_t written = fwrite(buffer, 1, read_len, f);
        if (written != read_len) {
            ESP_LOGE(TAG, "❌ 寫入 SD 卡失敗");
            free(buffer);
            esp_http_client_cleanup(client);
            fclose(f);
            return ESP_FAIL;
        }
        
        total_read += read_len;
        
        // 顯示進度（每 10% 顯示一次）
        if (content_length > 0) {
            int progress = (total_read * 100) / content_length;
            if (progress / 10 != last_progress / 10) {
                ESP_LOGI(TAG, "📥 下載進度: %d%% (%d/%d bytes)", 
                         progress, total_read, content_length);
                last_progress = progress;
            }
        }
    }
    
    free(buffer);
    esp_http_client_cleanup(client);
    fclose(f);
    
    ESP_LOGI(TAG, "✅ 下載完成: %s (%d bytes)", filename, total_read);
    
    return ESP_OK;
}

// 格式化 SD 卡為 FAT32
esp_err_t sd_format_card(void) {
    if (!is_mounted) {
        ESP_LOGE(TAG, "❌ SD 卡未掛載，無法格式化");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGW(TAG, "⚠️ 警告：即將格式化 SD 卡，所有數據將被刪除！");
    ESP_LOGI(TAG, "等待 3 秒...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "🔄 開始格式化 SD 卡...");
    
    // 先卸載
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
    is_mounted = false;
    
    // 重新掛載並格式化
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,  // 啟用格式化
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    
    // SDSPI 設備配置
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = SPI2_HOST;
    
    // SDSPI 主機配置
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    
    // 掛載（會自動格式化）
    esp_err_t ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 格式化失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    
    is_mounted = true;
    ESP_LOGI(TAG, "✅ SD 卡格式化成功！");
    
    return ESP_OK;
}
