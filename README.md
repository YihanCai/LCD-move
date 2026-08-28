# ESP32-C6 ST7789 LCD 山景动画

ESP32-C6 + 1.3" ST7789 LCD 的动态动画项目，实现一个小人从山脚爬到山顶的效果。

## 硬件

- **MCU**: ESP32-C6-DevKitC-1
- **LCD**: 1.3" ST7789 240x240 SPI 屏幕
- **引脚**:
  - SCLK = GPIO 7
  - MOSI = GPIO 6
  - CS = GPIO 14
  - DC = GPIO 15
  - RST = GPIO 21
  - BL (背光) = GPIO 22

## 开发环境

- **平台**: PlatformIO
- **框架**: Arduino (ESP32 3.x)
- **显示库**: LovyanGFX（原生支持 ESP32-C6）

## 项目结构

```
G:\Projects\LCD-move\
├── src/
│   └── main.cpp          # ESP32-C6 固件代码
├── lib/
│   └── LovyanGFX/        # 显示库（本地副本）
├── platformio.ini        # PlatformIO 配置
├── LCD-simulator.html    # 浏览器模拟器（可视化调试）
├── LCD-animation-plan.md # 项目计划文档
└── .gitignore            # Git 忽略规则
```

## 快速开始

### 1. 编译烧录到 LCD

```bash
cd G:\Projects\LCD-move
pio run -t upload --upload-port COM4
```

### 2. 浏览器模拟器

直接双击打开 `LCD-simulator.html`，可以在浏览器中预览动画效果，方便调试：

- 调整移动速度
- 暂停/播放动画
- 手动拖动小人位置
- 独立开关各图层（天空/山体/岩层/雪帽/裂缝/小人/山脊线）

### 3. 监控串口

```bash
pio device monitor
```

## 动画效果

- 山景：折线山脊 + 岩层明暗 + 雪帽 + 岩石裂缝
- 小人：红色火柴人，双脚贴合坡面
- 爬坡：沿山脊折线从左下到右上，到达山顶后循环

## 里程碑

| 步骤 | 内容 | 状态 |
|---|---|---|
| 1 | 点亮屏幕验证 | ✅ |
| 2 | 静态山景背景 | ✅ |
| 3 | 静态小人 | ✅ |
| 4 | 爬坡动画 | 🔄 进行中 |
| 5 | 打磨特效 | 待开始 |

## 注意事项

- ESP32-C6 需要 arduino-esp32 3.x，TFT_eSPI 不支持，改用 LovyanGFX
- 本地库 `lib/LovyanGFX/` 是从 GitHub 下载的 master 分支
- 模拟器 `LCD-simulator.html` 与 LCD 代码保持同步，可放心使用
