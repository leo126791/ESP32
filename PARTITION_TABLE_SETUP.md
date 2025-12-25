# 分區表配置指南

## 問題

編譯錯誤：應用程式大小 (1.3 MB) 超過分區大小 (1 MB)

```
Error: app partition is too small for binary ESP32.bin size 0x1534d0:
- Part 'factory' 0/0 @ 0x10000 size 0x100000 (overflow 0x534d0)
```

## 解決方案：使用 menuconfig 配置自定義分區表

### 步驟 1: 打開 menuconfig

```bash
# 在專案根目錄執行
idf.py menuconfig
```

或者使用完整路徑：
```bash
C:\Espressif\frameworks\esp-idf-v5.5.1\export.bat
idf.py menuconfig
```

### 步驟 2: 導航到分區表設置

1. 使用 **方向鍵** 導航
2. 找到並進入 **`Partition Table`**
3. 按 **Enter** 進入

### 步驟 3: 選擇自定義分區表

1. 使用方向鍵選擇 **`Partition Table (Custom partition table CSV)`**
2. 按 **Enter** 展開選項
3. 選擇 **`Custom partition table CSV`**
4. 按 **Space** 或 **Enter** 選中

### 步驟 4: 設置自定義分區表文件名

1. 找到 **`Custom partition CSV file`**
2. 按 **Enter** 編輯
3. 輸入：`partitions_16mb.csv`
4. 按 **Enter** 確認

### 步驟 5: 保存並退出

1. 按 **S** (Save) 保存配置
2. 按 **Enter** 確認保存到 `sdkconfig`
3. 按 **Q** (Quit) 退出 menuconfig
4. 按 **Enter** 確認退出

### 步驟 6: 重新編譯

```bash
idf.py build
```

## 詳細的 menuconfig 操作

### 鍵盤快捷鍵

- **方向鍵 ↑↓**: 上下移動
- **方向鍵 ←→**: 左右切換選項
- **Enter**: 進入子菜單或編輯選項
- **Space**: 選中/取消選中
- **/** (斜線): 搜索功能
- **?**: 查看幫助
- **S**: 保存配置
- **Q**: 退出

### 搜索功能

如果找不到 Partition Table 選項：

1. 按 **/** 打開搜索
2. 輸入：`PARTITION_TABLE`
3. 按 **Enter** 搜索
4. 選擇搜索結果，按 **Enter** 跳轉

## 分區表配置詳情

### 當前配置 (partitions_16mb.csv)

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x400000,    # 4 MB 應用程式
storage,  data, fat,     0x410000,0xBF0000,    # 12 MB 存儲空間
```

### 分區說明

- **nvs** (24 KB): 非易失性存儲，用於 WiFi 配置等
- **phy_init** (4 KB): PHY 初始化數據
- **factory** (4 MB): 主應用程式分區
- **storage** (12 MB): FAT 文件系統，用於存儲音頻等

### 為什麼需要 4 MB？

- **Edge Impulse 模型**: ~500 KB
- **TensorFlow Lite**: ~300 KB
- **ESP-DSP 庫**: ~200 KB
- **主程式碼**: ~300 KB
- **其他庫**: ~100 KB
- **總計**: ~1.4 MB (需要 4 MB 分區以留有餘裕)

## 驗證配置

編譯成功後，你會看到：

```
ESP32.bin binary size 0x1534d0 bytes. Smallest app partition is 0x400000 bytes. 0x2acb30 bytes (67%) free.
```

這表示：
- 應用程式大小: 1.3 MB
- 分區大小: 4 MB
- 剩餘空間: 2.7 MB (67%)

## 常見問題

### Q: menuconfig 無法打開？

**A**: 確保已經設置 ESP-IDF 環境：
```bash
C:\Espressif\frameworks\esp-idf-v5.5.1\export.bat
```

### Q: 找不到 Partition Table 選項？

**A**: 使用搜索功能 (按 `/`)，輸入 `PARTITION_TABLE`

### Q: 保存後還是使用舊的分區表？

**A**: 執行完整清理：
```bash
idf.py fullclean
idf.py build
```

### Q: 編譯後還是報錯？

**A**: 檢查 `sdkconfig` 文件：
```bash
# 應該看到這些行
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_16mb.csv"
CONFIG_PARTITION_TABLE_FILENAME="partitions_16mb.csv"
```

### Q: 可以直接編輯 sdkconfig 嗎？

**A**: 可以，但不推薦。使用 menuconfig 更安全，會自動處理依賴關係。

如果要手動編輯，修改這些行：
```
CONFIG_PARTITION_TABLE_SINGLE_APP=n
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_16mb.csv"
CONFIG_PARTITION_TABLE_FILENAME="partitions_16mb.csv"
```

然後執行：
```bash
idf.py reconfigure
idf.py build
```

## 燒錄到設備

配置完成並編譯成功後：

```bash
# 燒錄（替換 COM3 為你的端口）
idf.py -p COM3 flash monitor
```

**注意**: 更改分區表後，NVS 數據會被清除，需要重新配置 WiFi 等設置。

## 快速命令總結

```bash
# 1. 設置環境
C:\Espressif\frameworks\esp-idf-v5.5.1\export.bat

# 2. 打開 menuconfig
idf.py menuconfig

# 3. 導航到: Partition Table → Custom partition table CSV
# 4. 設置文件名: partitions_16mb.csv
# 5. 保存 (S) 並退出 (Q)

# 6. 重新編譯
idf.py build

# 7. 燒錄
idf.py -p COM6 flash monitor
```

## 完成！

配置完成後，你的 ESP32-S3 將有：
- ✅ 4 MB 應用程式空間（足夠 Edge Impulse 模型）
- ✅ 12 MB 存儲空間（用於音頻文件等）
- ✅ 24-bit 音頻處理
- ✅ Hi Lemon 喚醒詞檢測

現在可以開始測試你的語音助理了！
