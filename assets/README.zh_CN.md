<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

### 蛙声跳跳字体

应用使用 [Noto Sans CJK SC Regular](https://github.com/notofonts/noto-cjk/tree/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans)，
源文件保存为 `fonts/NotoSansCJKsc-Regular.otf`，使用 SIL Open Font License，
许可见 [`fonts/OFL.txt`](fonts/OFL.txt)。源文件 SHA-256：
`2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`。

`fonts/frog_font_14.c` 和 `fonts/frog_font_20.c` 是 14／20 像素、4 bpp、
无压缩 LVGL 字体子集，由 `main/CMakeLists.txt` 编入应用。
原始 OTF 仅用于复现，不嵌入固件。青蛙与池塘是 `main/frog_ui.c` 中原创的
代码图形，使用项目 MIT 许可，没有复用 demo 的视觉资产。

`main/frog_text.h` 统一定义固定界面文案。`fonts/frog-characters.txt` 和
`main/frog_glyphs.h` 记录所有所需码点，包括可打印 ASCII。
使用固定版本的官方转换器重新生成：

```sh
npm install --prefix .tools --no-audit --no-fund lv_font_conv@1.5.3
python3 tools/generate_frog_fonts.py
```

脚本使用 14／20 像素、`--bpp 4 --format lvgl --no-compress --no-kerning`，
通过 `--symbols` 指定字符清单，字体名称为 `frog_font_14`／`frog_font_20`。
`tests/test_frog_assets.py` 检查源文件哈希和字符清单；实际 LVGL 主机渲染器
检查字形描述符、控件字体和文本边界。中文实机显示仍属于设备验收项。
详见[应用验证说明](../docs/frog-voice-score.zh_CN.md)。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

| 文件 | 尺寸与格式 | 用途与来源 |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160，JPEG | 嵌入中英文项目 README 的产品主图，突出 AI Passport 产品形象与开放、人人可创作的理念。 |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724，PNG RGBA | 保留为可选技术参考图，不再用于首页主视觉。于 2026-09-17 使用内置图像生成工具为本仓库生成；已根据文档中的硬件能力契约核对图中的六项标签与参数。 |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336，PNG RGBA | 从仓库原始 `images/logo.png` 中精确裁切并去除背景的黑色字标；用于中英文项目 README 的浅色主题。 |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336，PNG RGBA | 提取字标的白色版本；README 使用 `<picture>` 在 GitHub 深色主题下显示。 |

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。
