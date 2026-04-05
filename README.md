# 自动按键器 (Auto Clicker)

一个用 C 语言编写的 Windows 自动按键工具，支持自定义按键序列、全局热键开关、循环执行，并通过配置文件进行设置。无需复杂的 GUI 操作，编辑文本文件即可完成所有配置。

## AI 生成声明

本项目的绝大部分源代码由 AI 编程工具（deepseek）根据作者的需求描述生成。作者对所有代码进行了审查、整合、测试和修改，并对最终成品负责。项目采用 MIT 许可证，使用风险由使用者自行承担。

## 功能特点

- ✅ 自定义按键序列（支持单个按键、组合键如 Ctrl+C、Alt+Tab）
- ✅ 全局热键开关（默认 F4，可在配置文件中修改）
- ✅ 循环执行模式
- ✅ 配置文件驱动，无需对话框，永不卡死
- ✅ 支持特殊按键：Delete、Insert、Home、End、PageUp、PageDown、PrintScreen、Pause、NumLock、ScrollLock
- ✅ 支持主键盘数字键（0-9）和小键盘数字键（Num0-Num9）
- ✅ 使用扫描码（Scan Code）发送按键，兼容大多数游戏
- ✅ 静态链接，单个 exe 文件，无依赖，支持 Windows XP 及以上

## 下载与编译

### 直接下载
前往 [Releases](https://github.com/Orange-Today/AutoClicker/releases) 页面下载已编译好的 `AutoClicker.exe`。

### 自行编译
使用 MinGW 编译：
```bash
gcc -static -mwindows -o AutoClicker.exe autoclicker.c -lcomctl32 -luser32 -lgdi32
