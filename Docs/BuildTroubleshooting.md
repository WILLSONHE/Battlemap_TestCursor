# 编译失败排查（SourceFileCache.bin / LNK1104）

## 常见错误

```
IOException: user-mapped section open: ...\Intermediate\Build\SourceFileCache.bin
```

或 `Makefile.bin` / `UnrealEditor-*.dll` / `.exp` 无法写入。

**原因**：多个进程同时占用 `Intermediate`（Unreal Editor、Visual Studio 编译、Cursor 索引、XGE/UBT 并行）。

## 推荐编译方式

1. **关闭** Unreal Editor（若正在运行）
2. **关闭 Visual Studio**，或编译时使用 `-KillVisualStudio`（VS 会锁住 `BuildRules\*.pdb`）
3. 在项目根目录执行：

```powershell
.\Scripts\BuildEditor.ps1
```

若提示 VS 正在运行：

```powershell
.\Scripts\BuildEditor.ps1 -KillVisualStudio
```

需要彻底清理时：

```powershell
.\Scripts\BuildEditor.ps1 -CleanIntermediate -KillVisualStudio
```

4. 编译成功后再打开 VS / 编辑器

## 本项目已做的缓解

- `.gitignore`：避免 `git status` 扫描 `Intermediate/`（会触发 adaptive build 与缓存争用）
- `.cursorignore`：避免 Cursor 映射 `Intermediate/` 文件
- `Battlemap_TestCursorEditor.Target.cs`：`bUseAdaptiveUnityBuild = false`
- `Saved/UnrealBuildTool/BuildConfiguration.xml`：关闭 XGE/UBA 远程执行器

## Visual Studio 内编译仍失败时

先运行 `Scripts\BuildEditor.ps1` 确认能通过，再在 VS 里 **仅当编辑器已关闭** 时点生成；或习惯用脚本编译、编辑器里用 Live Coding。
