# hiredis-happ Roadmap & Playbook

本路线图基于 2026-05-26 代码审查结果制定，用于把风险收敛为可执行任务。优先级按对正确性、稳定性和发布质量的影响排序。

## 2026-05-26 进展更新

- 已新增 `test/redis/redis-fixture.sh` 与 `test/redis/redis-fixture.ps1`，可脚本化下载官方 `redis-stable.tar.gz`、构建 Redis、启动单节点实例、以及创建临时 6 节点测试集群。
- 已补充 `happ_cmd`、`happ_connection`、`happ_cluster`、`happ_raw` 的纯单元/回归测试，覆盖命令格式化与 dump、pending reply 队列、raw timer/callback 配置、以及 `CLUSTER SLOTS` 的 `NIL` / `""` / `"?"` endpoint 边界。
- 已新增 `happ_integration_raw` / `happ_integration_cluster` 集成测试，并将 CTest 拆分为 unit、raw integration、cluster integration 三类入口，同时并回 Linux/macOS/Windows 的主测试流程，避免 Redis 覆盖游离在独立 job 之外。
- 仍需继续收敛的高价值风险：ASAN/UBSAN 下的 disconnect/reset 生命周期测试，以及真实集群中的 `MOVED` / `ASK` / `TRYAGAIN` 故障注入覆盖。

## P0：异步生命周期安全

### P0 目标

确认并修复 `redisAsyncContext`、pending callback、`cmd_exec`、`connection::release()` 之间的所有权边界，避免重复回调、悬空 `privdata` 或 use-after-free。

### P0 Playbook

1. 建立最小 libevent/libuv 集成测试夹具。
2. 用 ASAN/UBSAN 运行以下场景：
   - 发送命令后立即 `reset()`。
   - 连接中断触发 hiredis pending callback `reply == nullptr`。
   - `redisAsyncDisconnect()` 与 `redisAsyncFree()` 两种释放路径。
3. 记录每个 `cmd_exec` 的状态转换：created -> queued -> callback-called -> destroyed。
4. 如果发现 hiredis 后续仍会使用已销毁 `cmd_exec`，重构为单一销毁源：优先让 hiredis callback 负责最终销毁。
5. 将验证结果写入 `doc/code-review-2026-05-26.md` 或后续审查记录。

### P0 Done 条件

- ASAN/UBSAN 集成测试通过。
- 所有 pending command 在断线/reset 后只回调一次。
- `connection::release(true)` 行为有明确注释和测试覆盖。

## P1：Redis Cluster 兼容性

### P1 Cluster 目标

覆盖 Redis Cluster 规范中的常见重定向和 resharding 行为。

### P1 Cluster Playbook

1. 使用 docker compose 或脚本启动 3 master + 3 replica 测试集群。
2. 增加以下测试：
   - `CLUSTER SLOTS` 初始化 slot map。
   - hash tags：多 key 同 slot。
   - `MOVED` 更新单 slot 并触发全量 reload。
   - `ASK` 发送 `ASKING` 后仅重试当前命令。
   - `TRYAGAIN` 进入 TTL/timer retry。
   - `MOVED <slot> :<port>` 空 endpoint。
3. 对 slot 未覆盖、异常 reply、空 host 做负向测试。

### P1 Cluster Done 条件

- Cluster 集成测试可以在 CI 或 nightly 中运行。
- MOVED/ASK/TRYAGAIN 均有断言级覆盖。

## P1：构建、安装和 CI 发布质量

### P1 Packaging 目标

让本库作为 CMake package 被外部项目稳定消费，并让 CI 能捕获内存/UB 问题。

### P1 Packaging Playbook

1. 将 `hiredis_happ_config.h` 生成到 build include 目录，避免污染源码树。
2. 增加 `hiredis-happ-config.cmake` 和 version config。
3. 增加 install-tree smoke test：`find_package(hiredis-happ CONFIG REQUIRED)` 后编译一个最小消费者。
4. 增加 Linux sanitizer job：ASAN + UBSAN + Debug。
5. 保留 Windows shared/static matrix，并验证 DLL PATH 设置。

### P1 Packaging Done 条件

- clean checkout 配置/构建后不产生未跟踪源码文件。
- install 后的外部 CMake consumer 构建通过。
- sanitizer job 纳入 CI 或文档化为 release gate。

## P2：API 和文档增强

### P2 API/文档目标

降低使用者误用概率，清晰声明非 request-response 和线程安全边界。

### P2 API/文档 Playbook

1. README 已补 raw/cluster 最小调用示例；后续补更完整的 libuv/libevent integration walkthrough。
2. 明确 `raw`、`cluster`、`connection` 非线程安全，需由使用者保证同一事件循环线程访问。
3. 增加 ACL/RESP3 认证设计：`AUTH user password`、`HELLO 3 AUTH user password`。
4. 更新 Sentinel 状态：当前仍是设计稿，未实现则保持 TODO，不在 README 中暗示已可用。
5. 样例程序补充错误码输出和命令限制说明。

### P2 API/文档 Done 条件

- README 能让新用户完成构建、连接、发送命令和处理回调。
- Unsupported commands 与 raw command escape hatch 文档一致。
- Sentinel/ACL/RESP3 状态明确。

## P2：AI Agent 配置维护

### P2 AI 配置目标

保持根级 `AGENTS.md` 和标准 `.agents/skills/` 作为唯一规范入口；仅在官方验证有收益时保留极薄兼容层；来源索引保持一致，避免重复上下文和未验证工具声明。

### P2 AI 配置 Playbook

1. 每次新增或修改 AI 工具兼容声明时，先更新或复核 `doc/ai/source-index.md`。
2. 保持 `AGENTS.md` 为唯一主入口；`CLAUDE.md` 仅作为导入 `AGENTS.md` 与精简技能索引的 shim；只有在 `doc/ai/source-index.md` 记录了明确收益时才新增工具专用兼容层，不维护重复的 `.github/copilot-instructions.md` 或 `.github/copilot/skills/` 副本。
3. 将多步骤工作流放入 `.agents/skills/<name>/SKILL.md`，不要塞进常驻提示词。
4. 校验所有 `SKILL.md` frontmatter：`name` 必须等于父目录名，`description` 必须可触发且不冗长。
5. 对无法访问或 404 的来源只记录为未验证，不写入强支持结论。

### P2 AI 配置 Done 条件

- `git diff --check` 通过。
- 新增或修改的技能 frontmatter 手工校验通过。
- `doc/ai/source-index.md` 包含最新 `last_checked`、更新触发条件和未验证来源记录。
