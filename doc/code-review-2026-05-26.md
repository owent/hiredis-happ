# Code Review Report - 2026-05-26

本报告记录对 `hiredis-happ` 的一次全面代码审查。审查范围覆盖主仓库的 C++ 公共头文件、核心实现、单元测试、样例、CMake、CI、项目文档和 Copilot Agent 配置。`atframework/` 与 `cmake-toolset/` 作为外部/子模块依赖，原则上仅审查与本仓库构建入口相关的使用方式；本次仅对阻断 Windows 构建的 SSL 兼容问题做了最小修复。

## 调研依据

本次结论基于源码阅读、现有测试、CI 配置和官方资料交叉验证，不使用猜测作为决策依据。

- hiredis 异步 API：`redisAsyncContext` 非线程安全；`redisAsyncConnect()` 返回带 `err` 的 context 时调用方需要释放；异步 reply 只在回调期间有效；连接错误时 pending callbacks 会以 `NULL` reply 调用；`redisAsyncDisconnect()` 会在 pending callbacks 完成后释放 context。
- Redis Cluster 规范：slot 计算应支持 hash tags；`MOVED` 可永久更新 slot map，`ASK` 仅对下一条命令生效且必须先发送 `ASKING`；`TRYAGAIN` 可能在 resharding 的多 key 操作中出现；`MOVED` endpoint 可以为空，表示沿用当前请求的 endpoint 并替换端口；`CLUSTER SLOTS` 结果不保证覆盖全部 16384 slots。
- RESP 协议：`SUBSCRIBE`/`MONITOR`/RESP3 push 属于 request-response 模型例外，必须避免进入普通 `cmd_exec` 一次请求一次回复的生命周期。

## 已实施修复

### C++ 核心实现

- `src/detail/happ_cmd.cpp`
  - 为 `va_copy()` 增加匹配的 `va_end()`。
  - 避免 `redisvFormatCommand()` 返回负数时写入 `size_t raw_len` 形成超大长度。
  - `vformat(const sds*)` 增加空指针和 `sdsdup()` 失败保护。

- `src/detail/happ_connection.cpp`
  - `set_connecting()` 增加 `nullptr` 防御。
  - `redis_raw_cmd(const sds*)` 增加参数校验。
  - `make_name()` 修复端口 `0` 生成空端口名的问题。

- `src/detail/happ_raw.cpp`
  - `redisAsyncConnect()` 创建失败但返回非空 context 时调用 `redisAsyncFree()`，避免泄漏。
  - 修复网络错误重试判断：原条件 `REDIS_ERR_IO && REDIS_ERR_EOF` 永远不成立，改为 `||`。
  - `exec(connection, cmd)` 中对空 context 增加保护，避免失败路径空指针解引用。
  - AUTH 回调校验 reply 类型和 `str` 指针。
  - 日志缓冲区增加 `malloc()` 失败保护，并修复 `vsnprintf()` 截断返回值导致的越界写风险。

- `src/detail/happ_cluster.cpp`
  - 实现 Redis Cluster hash-tag slot 规则。
  - `redisAsyncConnect()` 创建失败但返回非空 context 时调用 `redisAsyncFree()`。
  - 修复网络错误重试判断：`REDIS_ERR_IO || REDIS_ERR_EOF`。
  - `exec(connection, cmd)` 中对空 context 增加保护。
  - `MOVED`/`ASK` 处理支持空 endpoint，按规范复用当前连接 endpoint。
  - `MOVED` slot 下标增加边界校验。
  - `TRYAGAIN` 进入现有 TTL/timer retry 流程。
  - `CLUSTER SLOTS` reply 增加类型、空指针和 slot 范围校验，避免异常 reply 造成越界或崩溃。
  - AUTH 回调校验 reply 类型和 `str` 指针。
  - 日志缓冲区增加 `malloc()` 失败保护，并修复截断越界写风险。

- `atframework/atframe_utils/src/algorithm/crypto_dh.cpp`
  - 修复 LibreSSL/OpenSSL 兼容层中 `ECerr` 宏不可用导致的 MSVC 编译失败，改用已存在于本地 SSL 头文件中的 `ERR_put_error()`。

### 测试与 CI

- `test/case/hiredis_happ_cluster_test.cpp`
  - 增加 hash-tag slot 规则回归测试，覆盖 `{user1000}`、`foo{}{bar}`、`foo{{bar}}zap`、`foo{bar}{zap}` 和空 key。

- `test/case/hiredis_happ_connection_test.cpp`
  - 增加 `connection::make_name(..., 0)` 回归测试。

- `.github/workflows/main.yml`
  - 修复 CodeQL cache key 的 GitHub Actions expression 语法错误。

- `.gitattributes`
  - 增加仓库级 whitespace 规则，允许本仓库已存储的 CRLF 行尾参与 `git diff --check`，同时继续检查真正的行尾空白。

### 文档和 Agent 配置

- 更新 README、Copilot instructions 和技能文档，记录本仓库的构建/测试入口、Redis Cluster/hiredis 审查准则和后续 playbook。
- 新增 `doc/ROADMAP.md`，把本次审查未完成的大项转为可执行计划。

## 验证结果

- CMake configure：`cmake -S . -B build_jobs_review -DPROJECT_HIREDIS_HAPP_ENABLE_UNITTEST=ON -DPROJECT_HIREDIS_HAPP_ENABLE_SAMPLE=ON -DATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LOW_MEMORY_MODE=ON` 通过。
- MSVC build：`cmake --build build_jobs_review --config RelWithDebInfo` 通过。
- CTest：在 Windows 上先将 `third_party/install/windows-amd64-msvc-19/bin` 加入 `PATH` 后，`ctest --test-dir build_jobs_review -V -R hiredis-happ-run-test -C RelWithDebInfo --timeout 120` 通过，4 个测试全部通过。
- Whitespace：默认 `git diff --check` 通过。

注意：未设置 Windows `PATH` 时，测试程序因找不到 `hiredis.dll` 返回 `0xC0000135`；这不是测试逻辑失败，但需要在本地/CI playbook 中显式处理。

## 仍需优先跟进的风险

### P0：异步连接释放语义需要集成验证

`connection::release(true)` 当前会先调用 `redisAsyncDisconnect(context_)`，随后主动遍历 `reply_list_` 调用并销毁 `cmd_exec`。根据 hiredis 文档，`redisAsyncDisconnect()` 不是立即释放，而是在 pending commands 的 replies/callbacks 完成后释放 context；连接错误时 pending callbacks 也会以 `NULL` reply 被调用。因此该路径存在潜在重复回调或 `privdata` 悬空风险。

建议下一步用 libevent/libuv + ASAN 建立真实异步事件循环测试，再决定是否调整为：

1. disconnect/free 前先从 hiredis pending callback 生命周期中安全脱钩；或
2. 统一由 hiredis callback 驱动 `cmd_exec` 销毁；或
3. 明确使用 `redisAsyncFree()` 并适配 pending callback 行为。

### P1：集成测试覆盖不足

当前单测主要是对象状态与格式化行为，缺少真实 Redis/Redis Cluster 场景。建议补充：

- 单节点 raw connector：连接成功、断线、AUTH 成功/失败、pending command 错误回调。
- Cluster：`CLUSTER SLOTS` 加载、`MOVED`、`ASK + ASKING`、`TRYAGAIN`、空 endpoint、slot 未覆盖。
- 非 request-response：`SUBSCRIBE`、`UNSUBSCRIBE`、`MONITOR` 必须走 `redis_raw_cmd()`。

### P1：构建/安装包质量

- `hiredis_happ_config.h` 由 CMake 写回源码目录，容易污染工作区。建议改为生成到 build include 目录，并在 install/export 中显式安装。
- 当前只安装 targets，缺少标准 `hiredis-happ-config.cmake` / version config，外部项目 `find_package(hiredis-happ CONFIG)` 体验不足。
- 建议增加 sanitizer job（ASAN/UBSAN）和 Windows shared/static 双模式的本地 playbook。

### P2：API 能力和文档

- 认证仍是旧式 `AUTH <password>`，未覆盖 Redis ACL 的 `AUTH <user> <password>` 或 `HELLO 3 AUTH ...`。
- 项目应显式说明 `hiredis::happ::raw` / `cluster` 与 `redisAsyncContext` 一样不保证线程安全。
- README 示例偏旧，可补充最小 raw/cluster 用法和事件循环 adapter 说明。

## 审查结论

本次已修复多处可导致崩溃、泄漏或 Redis Cluster 语义错误的缺陷，并补充最小单测与执行计划。剩余最大风险集中在真实异步事件循环下的 pending callback/连接释放生命周期，需要通过集成测试和 sanitizer 继续收敛。