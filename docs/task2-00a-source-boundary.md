# 00A 有边界源码对照与收敛决定（2026-11-12）

本轮仅文档审查，不修改实现、不新增或重跑测试、不安装运行环境。冻结代码/测试基线 d22cae05。#673 上游及源分支不动，Doom 等用户晚上测试。

RPCS3 固定提交：9b8974d32e73c897a5fcd6bdf69413ca7e436b01。下列行号按直接下载的固定提交核验；浏览器缓存的 master 行号不同，不混用。源码对照不是固件符合性证明。

## 已确认的事实及其限度

| 对照项 | 证据与结论 | 对00A的意义 |
|---|---|---|
| Taskset2布局 | [cellSpurs.h L1119–1157](https://github.com/RPCS3/rpcs3/blob/9b8974d32e73c897a5fcd6bdf69413ca7e436b01/rpcs3/Emu/Cell/Modules/cellSpurs.h#L1119)：128项TaskInfo、六组bitset、退出记录偏移0x1980、结构0x2900 | 我们对应主布局可与其交叉核对；头文件不定义重复Join/ID寿命，也不能单凭字段名推导状态字节语义 |
| v1分配算法 | [cellSpurs.cpp L4087–4249](https://github.com/RPCS3/rpcs3/blob/9b8974d32e73c897a5fcd6bdf69413ca7e436b01/rpcs3/Emu/Cell/Modules/cellSpurs.cpp#L4087)：原子认领enabled中的空位，满时AGAIN | 可借鉴并发认领机制；这是v1入口调用路径，不能据此认定Task2退出后、未Join时就可复用，也不能直接迁移其错误码 |
| Task2关键入口 | [cellSpurs.cpp L4510–4537](https://github.com/RPCS3/rpcs3/blob/9b8974d32e73c897a5fcd6bdf69413ca7e436b01/rpcs3/Emu/Cell/Modules/cellSpurs.cpp#L4510)：CreateTask2、JoinTask2、TryJoinTask2、DestroyTaskset2、WithBinInfo仍是UNIMPLEMENTED后返回OK | 本轮限定源码范围内没有可用于裁定两个语义问题的实现。注册了函数不等于实现了契约 |
| CreateTaskset2 | [cellSpurs.cpp L4484–4507](https://github.com/RPCS3/rpcs3/blob/9b8974d32e73c897a5fcd6bdf69413ca7e436b01/rpcs3/Emu/Cell/Modules/cellSpurs.cpp#L4484)仍明确留有未完成部分 | 不能作为我们的完整初始化、销毁和错误路径的oracle |
| lv2 flag销毁 | [sys_event_flag.cpp L97起](https://github.com/RPCS3/rpcs3/blob/9b8974d32e73c897a5fcd6bdf69413ca7e436b01/rpcs3/Emu/Cell/lv2/sys_event_flag.cpp#L97)：有等待者时BUSY | 支持“不能销毁仍在使用的同步对象”这一设计方向；并不证明Taskset2 Destroy应如何等待/返回，也不证明我们的短期host引用BUSY完全符合固件 |

00A创建失败回滚、host job/Join引用、对象代次、owned flag保护及LS回收，有自身回归/内存检查支持，属于已验证的宿主安全实现。它们不是RPCS3 Task2完整实现的移植，更不能把fork CI绿灯解释成SDK兼容认证。

## 有限的新增差异登记：ABI-01

我们的cellSpurs.c把TasksetAttribute2当0x80字节初始化/校验；RPCS3 [头文件L1164–1176](https://github.com/RPCS3/rpcs3/blob/9b8974d32e73c897a5fcd6bdf69413ca7e436b01/rpcs3/Emu/Cell/Modules/cellSpurs.h#L1164)定义512字节、8字节对齐，其[初始化L4392起](https://github.com/RPCS3/rpcs3/blob/9b8974d32e73c897a5fcd6bdf69413ca7e436b01/rpcs3/Emu/Cell/Modules/cellSpurs.cpp#L4392)清整份对象。

这是已确认的两份源码之间的ABI差异，尚未以独立SDK定义/guest观测确认目标契约。本轮只登记，不修改长度、不扩大测试。后续若处理，作为单独小项，明确结构大小、清零范围和短缓冲边界即可结束；不与Join争议混在一起。它意味着当前不能宣称00A所有结构都已与参考实现一致。

## 只保留两个语义待决问题

| 问题 | 当前已知 | 缺少的决定性依据 |
|---|---|---|
| J-01 重复/并发Join | host目前重复返回缓存退出码，两个joiner都成功；RPCS3相关入口为桩 | 明确允许几次/几个joiner；已Join后的返回码及输出写入规则。需要明确接口契约、完整相关实现路径，或可追溯的真实guest固件/LLE记录；不能靠桩返回OK推断 |
| I-01 ID寿命与复用 | host可复用退出未Join的ID；旧等待Join会读到新任务的码。对象generation只保护Taskset2本身 | Task2的slot何时释放、未Join的退出任务是否仍占ID、复用与已等待Join的先后归属；证据须说明PPU/SPU创建路径及调用前提 |

旧Join取得B的结果是确定的host观察现象；在缺少契约时，尚不能判断原调用序列在固件上是否合法，也不能据此选定epoch或禁止复用方案。退出记录中的状态值、Destroy返回BUSY、host故障退出码等现有实验约定不升级为“已证实的固件规范”。

## 收敛方案与停止条件

1. **本轮审查至此关闭。** 保存固定源码版本、对照表及差异，不再扩大源码搜索范围或增加host测试。
2. **冻结实现和观察测试。** 保留d22cae05及其已有CI证据；不为文档重跑CI，不修改golden观察来追绿灯。00A状态写为“宿主安全已有验证；Task2兼容性待证实”，暂不提交上游。
3. **后续工作上限固定为J-01、I-01、ABI-01。** 新发现先登记，另行决定范围，不自动扩展00A。安装RPCS3不是本轮任务，也不是#673的前置条件。
4. **只有取得决定性依据才恢复语义修改。** 若ID必须保留至Join，修正复用资格；若允许复用但旧Join绑定原任务，则设计task epoch/稳定完成记录；若调用序列本身无效，记录前提与错误规则。不能预先选择其中之一。处理时覆盖实际PPU/SPU入口，避免仅改host分配器。
5. **每个决定采用一次定向修复、一次撤修复对照和一次适当CI验收。** 仅在确定契约和修复后扩展那一项必要回归；未获证据则保持待验证，不继续循环host压测。
6. **00A恢复提交评估的门槛**：两个语义问题有明确结论，ABI-01已核验/处置，指定guest路径验证通过，并列明Windows原生覆盖和游戏验证的剩余范围。Doom通过不能替代Task2契约证据；00A未达门槛不阻塞独立的#673。

结论：现在停止“增加测试等于增加正确性证据”的迭代。本次产物是有来源、有缺口、有停止条件的审查结论，不是假定语义后的新实现。
