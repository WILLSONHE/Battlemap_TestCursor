# Phase5-Alpha — 标准任务与回归清单



本清单与 `EMissionType`、`ABattlemap_TestCursorGameMode` 中的胜负判定对齐；在编辑器中调整 **Mission Type**、**Defense Hold Duration Seconds**、**Balance Table / Row** 后按场景勾选验证。



## 启动流程与编组（主菜单 / 存档）

- **默认进图**：`Config/DefaultEngine.ini` 中 `GameDefaultMap` / `EditorStartupMap` 为 **`L_MainMenu`**；**`GameInstanceClass` 须写在 `[/Script/EngineSettings.GameMapsSettings]`**（与默认地图同段；UE 5.x 不再从 `[/Script/Engine.Engine]` 读取该项），值为 **`/Script/Battlemap_TestCursor.BattleGameInstance`**。`L_MainMenu` 通过 **`GameModeMapPrefixes`** 使用 **`ABattleMainMenuGameMode`**（若前缀未命中，请在 `L_MainMenu` 关卡的 **World Settings** 中手动将 **GameMode Override** 设为 `BattleMainMenuGameMode`）。
- **主菜单**：`开始游戏` 打开 **`L_TestMinimal`**（`ABattlemap_TestCursorGameMode`）；**部队编组** 编辑 `UBattleCareerSaveGame` 槽位（启用/类型/标签）并保存；友军由 **`SpawnTestEnvironment`** 按槽位沿原 Alpha/Bravo 锚点连线偏移生成，**Supply Road Anchor** 仍为 **第一个生成的友军** 位置；敌方仍为单辆测试坦克。
- **结算**：胜负判定后弹出 **`UBattleMissionDebriefWidget`**（战损摘要），并写入 **`ApplyPostMissionExperience`**（胜/败经验见 `Config/BattleBalance.ini`）。**返回主菜单** / **再来一局** 分别打开 `L_MainMenu` / `L_TestMinimal`。



## 通用前置



1. 打开使用 `ABattlemap_TestCursorGameMode` 的关卡（例如从主菜单进入 **`L_TestMinimal`**），生成 `ATacticalMapGrid` 与测试单位。

2. 输出日志：`Mission outcome: Victory|Defeat`（含任务类型或防御计时说明）。

3. **HUD**：左上角应显示 **任务类型 / 结果**、**任务经过时间**；`Defense` 且 `Defense Hold Duration > 0` 且进行中时，额外显示 **已过 / 目标 / 剩余** 秒。另有一行 **平衡版本**、**ECM 干扰感知系数**、**补给食物/燃油每秒消耗**（来自当前生效的 `FBattleBalanceTableRow`：无表时为 `Config/BattleBalance.ini` 与 `UBattleBalanceDeveloperSettings` 的合并默认值）。

4. 补给：`UBattleSupplyComponent` 每帧使用 **单位上已应用的** 食物/燃油消耗率及道路折扣系数；这些数值在 `ApplyBalanceToUnit` 时由 **合并后的 `FBattleBalanceTableRow`** 写入（与 HUD 展示同源）。若单位与 **Supply Road Anchor**（默认首名友军出生点）经 **Road/Railway/Bridge** 格网 BFS 连通，则消耗再乘以表中的 `SupplyRoadFoodMultiplier` / `SupplyRoadFuelMultiplier`。

5. **ECM 干扰**：友军 `Comms = Jammed` 仅当其 **世界坐标** 落在任一 **`JamRadiusUU > 0`** 的车载 `UBattleECMZoneComponent` 圆盘内；每帧在 **全 Actor Tick 之后** 由 `UBattleECMZoneComponent::UpdateFriendlyJamForWorld` 统一结算（`ABattlemap_TestCursorGameMode` 注册 `OnWorldPostActorTick`）。**与点击移动目的地无关**。



## M01 — 突击（Assault）



| 步骤 | 操作 | 期望 |

|------|------|------|

| 1 | `Mission Type = Assault`，`Defense Hold Duration = 0` | 任务进行中 `Mission Outcome = InProgress`；HUD 显示任务类型与经过时间 |

| 2 | 消灭全部敌方单位 | 日志 `Victory` — all hostiles eliminated |

| 3 | 友军全灭（调试伤害或放任被击毁） | 日志 `Defeat` |

| 4 | 友方身体进入敌方车载 ECM 半径（`JamRadiusUU > 0`） | `Comms = Jammed`，玩家无法下达新机动（`IsCommandEnabled`）；**敌方 AI 索敌/追击仍使用完整 `DetectionRange`**（不因友军 Jammed 再乘 `EcmJammedDetectionRangeScale`，避免与车载干扰叠乘导致无法接战）。`EcmJammedDetectionRangeScale` 由 **平衡表 / `BattleBalance.ini`** 提供，可在 HUD 核对，供后续 UI/规则扩展使用。 |

| 5 | 驶离所有干扰盘 | 友方通讯恢复 `Online`（未被其他系统置为 `Lost` 时） |



## M02 — 防御（Defense）



| 步骤 | 操作 | 期望 |

|------|------|------|

| 1 | `Mission Type = Defense`，`Defense Hold Duration > 0`（例如 120） | `Mission Elapsed` 累计；HUD 显示 **已过 / 目标 / 剩余** 秒 |

| 2 | 保持至少一名友军存活至计时结束 | 日志 `Victory (Defense hold …)` |

| 3 | 友军全灭（计时结束前） | `Defeat` |

| 4 | 计时前全歼敌军 | 仍应先触发「全歼」胜利（与突击一致） |



## 平衡表（DataTable + 补给 / ECM 管线）



- 行结构：`FBattleBalanceTableRow`（`/Script/Battlemap_TestCursor.BattleBalanceTableRow`）。

- 在 Game Mode 上指定 **Balance Table** 与 **Balance Row Name** 后，`ApplyBalanceToUnit` 对该行覆盖：**AttackDamage / AttackRange / MoveSpeed / DetectionRange**，并写入 **`UBattleSupplyComponent`**：`SupplyFoodConsumePerSecond`、`SupplyFuelConsumePerSecond`、`SupplyRoadFoodMultiplier`、`SupplyRoadFuelMultiplier`；**`EcmJammedDetectionRangeScale`** 进入合并行，由 **`GetEffectiveBattleBalanceRow`** 与 HUD 展示（与 `Config/BattleBalance.ini` 默认值合并规则一致：有表行则整行替换）。

- **版本号**：表内 `BalanceVersion` 与 `Config/BattleBalance.ini` 中 `BalanceVersion` 应同步记录在变更说明中。



## AI 行为（最小集）



- 敌方载具：`ABattleVehicleUnit` 挂载 `UBattleEnemyTacticalBrainComponent`；当 `Mission Type` 为 **Recon** 或 **Escort** 且无其它命令活动时，在巡逻点之间下发 **Low** 优先级机动，与 `TryAutoEngage` 共存。

- 其他任务类型以 `TryAutoEngage` 为主（进入探测即 `IssueAttackCommandInterrupt`）。



## 路网补给（可判定 API）



- `ATacticalMapGrid::AreWorldPositionsConnectedForRoadSupply(WorldA, WorldB)`：端点可在道路格或与其四邻接的道路格上接入网络。



## 等高线并行专项



见 `Docs/ContourTerrainImplementation.md` 文末 **每周基线**；主线合入若改 `M_contour` / `MI_contour`，用 `DebugViewMode = ContourMask` 做截图对比。


