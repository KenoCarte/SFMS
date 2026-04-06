# SFMS (Simple File Management System)

一个基于 Qt Widgets 的本地文件管理与检索工具，面向日常“分散目录文件快速定位”场景。

## 主要功能

- 多目录索引：支持添加多个根目录并递归扫描文件。
- 实时检索：按文件名关键字即时过滤。
- 扩展名过滤：按扩展名快速缩小结果范围。
- 双击打开：双击结果项用系统默认程序打开文件。
- 单目录重建：双击左侧目录，仅重建该目录并只显示该目录结果。
- 导入文件：将外部文件复制到左侧选中的目录。
- 本地持久化：
   - 保存已添加目录（binary）
   - 保存窗口大小/位置与上次选中目录（binary）
- UI 优化：扁平化风格、较大字号、目录卡片化展示。

## 技术栈

- C++17
- Qt 6 Widgets（兼容 Qt5 构建文件保留）
- CMake + qmake（双构建入口）
- PowerShell 打包脚本

## 项目结构

```text
SFMS/
   main.cpp
   MainWindow.h
   MainWindow.cpp
   CMakeLists.txt
   QtFileManager.pro
   resources.qrc
   app.rc
   app.ico
   Release.ps1
   README.md
```


## 开发环境要求

- Windows 10/11
- Qt 6.x（建议 MinGW 64-bit 套件）
- MinGW（若使用 MinGW 构建）
- CMake（可选，脚本会自动回退 qmake）

## 本地运行

### 方式 1：Qt Creator

1. 打开 `CMakeLists.txt` 或 `QtFileManager.pro`
2. 选择匹配的 Qt Kit（例如 Qt 6.x MinGW 64-bit）
3. 构建并运行

### 方式 2：命令行（CMake）

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="F:/Qt/6.11.0/mingw_64"
cmake --build build -j 4
```

### 方式 3：命令行（qmake）

```powershell
qmake -r -spec win32-g++ QtFileManager.pro
mingw32-make -j 4
```

## 打包可分发目录

项目内置脚本：`Release.ps1`

```powershell
powershell -ExecutionPolicy Bypass -File .\Release.ps1
```

脚本会自动：

1. 清理旧构建目录
2. 优先 CMake 构建，找不到则回退 qmake
3. 复制 exe 到 `dist/`
4. 调用 `windeployqt` 部署 Qt 依赖
5. 补齐 MinGW 运行时 DLL

