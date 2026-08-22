# CXO 改进计划

本文档记录 CXO 在核心功能完成后的工程改进路线。当前基线为：核心构建流水线、双语路由、分页、标签与归档、RSS、Sitemap、开发服务器和部署命令均已实现，`make test` 全部通过。

下一阶段不以继续堆叠功能为主，而是优先提高跨平台可信度、内存安全、构建可靠性和后续功能的开发效率。

## 目标与原则

- 保持纯 C11、零外部运行时依赖和 Arena 内存模型。
- 先建立质量护栏，再调整内部结构，最后增加用户功能。
- 保持现有公共接口尽量稳定，重构集中在内部 seam。
- 每个阶段都必须具有独立、可验证的交付结果。
- 避免为尚不存在的变化预先引入抽象。

## 路线概览

| 阶段 | 主题 | 优先级 | 主要结果 |
| :--- | :--- | :--- | :--- |
| 1 | 跨平台 CI | P0 | 自动验证 Linux、macOS、Windows 构建与测试 |
| 2 | Sanitizer 与模糊测试 | P0 | 捕获内存错误、越界和畸形输入问题 |
| 3 | `cxo check` | P0 | 在生成站点前验证内容和输出冲突 |
| 4 | 原子构建 | P1 | 失败构建不污染已有 `public/` |
| 5 | 站点派生模型 | P1 | 集中语言、草稿、标签和归档规则 |
| 6 | Arena 字符串构建器 | P1 | 统一安全字符串拼接和错误处理 |
| 7 | 开发服务器内部拆分 | P1 | HTTP、监控和 SSE 可独立测试 |
| 8 | 结构化诊断 | P1 | 输出带文件和原因的可操作错误信息 |
| 9 | 用户功能扩展 | P2 | 改善双语创作、自定义页面和搜索能力 |

## 阶段 1：跨平台 CI

**状态：实现完成，等待代码推送后由 GitHub-hosted runners 完成远端验收。**

### 阶段边界

本阶段只负责让干净检出在目标平台上稳定完成编译和现有测试。ASan、UBSan、模糊测试和更严格的静态分析属于阶段 2；本阶段可以预留 job，但不把它们作为完成条件。

允许为使现有代码和测试跨平台运行而修改 Makefile、`platform.h`、平台 include 和测试夹具。不在本阶段重构服务器或改变用户可见行为。

### 已知前置问题

在创建工作流前先处理或用首轮 CI 确认以下问题：

- Makefile 定义了 `LIBS`，但主程序和测试链接命令没有使用它。
- Windows 构建需要链接 `ws2_32`，当前 Makefile 尚未实际加入。
- `src/main.c`、`src/cmd_init.c` 和部分测试直接包含 `unistd.h`。
- scanner、静态资源复制和开发服务器依赖 `dirent.h`；需要确认所选 MSYS2 toolchain 提供一致语义。
- Windows 的 `RM` 一次接收多个文件时，行为可能和 POSIX `rm -f` 不同。
- fixture 测试会 `chdir`、递归删除目录并检查 dotfile，必须在 Windows runner 上实际验证。

这些问题应修复为真实的跨平台能力，不能通过在 Windows job 中跳过测试来隐藏。

### CI 拓扑

使用一个 `.github/workflows/ci.yml`，由三个职责明确的 job 组成：

| Job | Runner / toolchain | 必须执行 |
| :--- | :--- | :--- |
| `linux` | Ubuntu，GCC 与 Clang matrix | 干净构建、完整测试、CLI smoke test |
| `macos` | macOS，系统 Clang | 干净构建、完整测试、CLI smoke test |
| `windows` | Windows + MSYS2 MinGW-w64 | 干净构建、完整测试、CLI smoke test |

Linux 使用 compiler matrix，而不是复制两个几乎相同的 job。Windows job 中的所有构建命令必须在同一种 MSYS2 shell 和同一 toolchain 环境运行，避免混用 MSYS、MinGW 和 PowerShell 路径语义。

### 触发与并发策略

- `push`：针对默认分支。
- `pull_request`：针对默认分支。
- `workflow_dispatch`：允许人工重跑和调试平台问题。
- 使用 concurrency group，以 workflow、ref 为键；同一 PR 的旧运行自动取消。
- 默认权限设为只读 `contents: read`。
- 设置合理的 job timeout，避免 Windows 安装或测试异常时无限挂起。

不在第一版加入定时构建。若未来依赖 runner/toolchain 漂移成为实际问题，再增加每周 schedule。

### 每个 job 的标准步骤

1. 检出仓库，并确认嵌入的 cmark、toml-c 文件存在。
2. 输出编译器和 GNU Make 版本，便于诊断 runner 漂移。
3. 执行 `make clean`，证明构建不依赖工作区残留。
4. 执行 `make`。
5. 执行 `make test`。
6. 执行 CLI smoke test：

   ```bash
   ./cxo version
   ./cxo help
   ./cxo build
   ```

   Windows 下使用对应生成的 `cxo.exe`。`build` smoke test 用于覆盖主程序链接和命令分发，因为测试二进制不会链接 `main.c`。
7. 检查命令退出码，并在失败时保留必要日志；第一版不上传普通构建产物。

### Makefile 调整计划

1. 让 `CPPFLAGS`、`CFLAGS`、`LDFLAGS` 和 `LIBS` 可以由环境或命令行覆盖，同时保留项目默认值。
2. 所有最终链接命令统一使用 `$(LDFLAGS)` 和 `$(LIBS)`。
3. Windows 条件分支加入 `-lws2_32`，包括任何实际引用 socket 代码的测试目标。
4. 增加一个仅负责验证的统一入口：

   ```make
   ci: clean all test
   ```

   CI job 可以调用 `make ci`，但底层 `make` 和 `make test` 仍保持独立可用。
5. 审核 Windows 的 `clean` 实现，保证多文件参数、缺失文件和递归目录清理均不会错误地使 job 失败。
6. 不在本阶段加入编译缓存；当前项目体量小，缓存收益不足以抵消失效和跨平台配置成本。

### Windows 适配执行顺序

Windows 是本阶段风险最高的部分，按以下顺序推进：

1. 先提交 Linux GCC/Clang 与 macOS job，锁定 POSIX 基线。
2. 增加 Windows job，保留第一次失败日志作为真实差异清单。
3. 修复链接参数，确保 `ws2_32` 通过 Makefile 的统一 `LIBS` 接入。
4. 将可由 `platform.h` 提供的 POSIX 调用从业务文件中移出或条件 include。
5. 只在确定所选 MinGW 环境不提供所需目录接口时，增加目录遍历 adapter；不要仅凭推测增加第二套实现。
6. 修复测试夹具的路径、`chdir`、dotfile 和清理差异。
7. Windows 完整运行 `make test`，不得只验证编译。

### 文档与仓库设置

- README 的 Build 部分记录 CI 覆盖的平台和本地等价命令。
- CI 首次稳定后添加状态 badge。
- 将 `linux`、`macos`、`windows` 设为分支保护的 required checks；这是仓库设置操作，代码合并后由维护者在 GitHub 中完成。
- required check 名称一旦启用分支保护，应保持稳定，避免无意解除保护。

### 提交拆分

建议保持以下原子提交顺序：

1. `ci: add linux and macos build matrix`
2. `build: use platform libraries in link commands`
3. `fix: make platform includes portable on windows`
4. `test: make fixtures portable on windows`
5. `ci: require windows build and tests`
6. `docs: document cross-platform verification`

如果某一步不需要修改，不创建空洞提交。工作流与使其变绿的修复可以在同一 PR 中完成，但应保留上述逻辑边界，便于审查和回滚。

### 失败处理原则

- 编译警告视为需要修复的问题；是否立即加入 `-Werror` 要分别验证第三方嵌入源码，避免 runner 编译器升级导致无关阻塞。
- 平台失败优先修复根因，不通过 `continue-on-error`、条件跳过或缩减测试范围绕过。
- runner 或包镜像的偶发网络失败可以重跑；项目编译或测试失败不能自动重试来掩盖。
- 若某项只在一个平台不适用，应在代码或测试中记录明确的平台理由。

### 验收标准

- 默认分支 push、Pull Request 和人工触发都会运行 CI。
- Ubuntu/GCC、Ubuntu/Clang、macOS/Clang、Windows/MinGW-w64 均能从干净检出完成构建。
- 上述四种组合全部运行且通过完整的 `make test`。
- 所有组合通过 `version`、`help`、`build` CLI smoke test。
- Windows 主程序正确链接 Winsock，不依赖手工注入链接参数。
- 不存在 `continue-on-error` 或平台级测试跳过。
- 项目源码编译维持零警告。
- README 显示 CI 状态，并记录平台覆盖和本地等价命令。
- 分支保护可以直接选择四个稳定命名的 required checks。

### 完成定义

连续多个 PR 或至少三次人工重跑结果一致，所有 required checks 均为绿色；从新 clone 开始无需未记录的本地依赖或手工步骤，即可复现对应平台的 CI 命令。本阶段完成后，再在阶段 2 中为 Linux 增加 sanitizer、静态分析和 fuzzing job。

## 阶段 2：Sanitizer 与模糊测试

### 工作内容

- 增加 `make test-sanitize`，启用 AddressSanitizer 和 UndefinedBehaviorSanitizer。
- 为以下输入建立模糊测试入口：
  - Frontmatter 与 Markdown 解析。
  - 标题收集、slugify、重复标题 ID 和 TOC 生成。
  - HTTP 请求行与 URL 解码。
- 增加回归语料：超长字段、截断文件、非法日期、非法 UTF-8、空字段、深层目录和异常请求。
- 每个模糊测试发现的问题都固化为普通回归测试。

### 重点文件

- `src/parser.c`
- `src/cmd_serve.c`
- `src/template.c`
- `src/render_feeds.c`

### 验收标准

- 完整测试集在 ASan/UBSan 下无错误。
- 模糊测试入口可以独立编译和运行。
- CI 至少执行固定时间或固定语料的 smoke fuzzing。

## 阶段 3：实现 `cxo check`

`cxo check` 只执行扫描、解析、关联和验证，不生成 `public/`。

### 验证范围

- Frontmatter 格式与日期合法性。
- 同语言重复 `id` 和重复 `slug`。
- 生成路径冲突，包括文章、分页、标签、归档与静态资源之间的冲突。
- 未知语言目录。
- 模板存在性、大小限制和必要变量。
- `base_url` 等关键配置的合法性。
- 翻译关联异常；缺少译文可以是提示而非错误。

### 验收标准

- 成功返回 0，验证错误返回稳定的非零错误码。
- 错误信息包含源文件、字段或冲突双方。
- 命令不创建或修改 `public/`。
- README 和 CLI help 包含使用说明。

## 阶段 4：原子构建

### 目标流程

```text
扫描与解析
    ↓
写入同文件系统的临时输出目录
    ↓
完整渲染和验证成功
    ↓
替换 public/
```

### 工作内容

- 构建期间不直接修改已有 `public/`。
- 失败时清理临时目录并保留旧站点。
- 替换操作失败时给出明确诊断。
- Windows 与 POSIX 分别实现可靠的目录替换策略。
- 明确静态资源覆盖生成页面时的规则，推荐将其视为错误。

### 验收标准

- 人为注入渲染失败后，原站点内容保持不变。
- 成功构建后不存在临时目录残留。
- Linux、macOS、Windows 行为一致。

## 阶段 5：建立站点派生模型

当前 index、tag、archive、RSS 和 sitemap 分别遍历 `ctx->entries`，重复执行语言过滤、草稿过滤、日期分组和去重。应由一个内部深模块集中完成这些规则。

概念接口示例：

```c
typedef struct cxo_site_index cxo_site_index_t;

cxo_site_index_t* cxo_site_index_build(cxo_context_t* ctx,
                                       arena_t* arena,
                                       int include_drafts);
```

派生模型负责提供：

- 每种语言按日期排序的已发布文章。
- 标签及其文章集合。
- 年、月归档及其文章集合。
- sitemap 和 feed 所需的规范化页面集合。

渲染模块只消费派生结果，不再各自重新实现筛选和分组规则。

### 验收标准

- 草稿、语言和日期规则只有一个权威实现。
- `render_index.c`、`render_taxonomy.c`、`render_feeds.c` 不再重复收集年份、月份和标签。
- 通过派生模型的接口即可测试所有分组规则。
- 输出与重构前保持一致。

## 阶段 6：Arena 字符串构建器

为动态 HTML、XML 和路径片段提供统一的 Arena 字符串构建模块，隐藏容量计算、扩容、终止符和截断检测。

建议接口：

```c
typedef struct cxo_buf cxo_buf_t;

cxo_buf_t* cxo_buf_create(arena_t* arena);
int cxo_buf_append(cxo_buf_t* buf, const char* text);
int cxo_buf_printf(cxo_buf_t* buf, const char* fmt, ...);
char* cxo_buf_finish(cxo_buf_t* buf);
```

### 迁移顺序

1. TOC 生成。
2. RSS 与 sitemap。
3. 文章、标签和归档列表。
4. Meta tags。
5. 开发服务器目录列表。

### 验收标准

- 字符串构建调用者不再手工维护 offset 和预估总长度。
- 内存不足和格式化失败能通过返回值传播。
- builder 本身具有边界、空字符串和大输入测试。
- 不引入逐字符串 `free()`，继续遵守 Arena 生命周期。

## 阶段 7：拆分开发服务器内部实现

保持 `cmd_serve()` 外部接口不变，将 `src/cmd_serve.c` 的职责拆为内部模块：

```text
cmd_serve.c       服务器生命周期和事件循环
http.c            请求解析、静态响应和 MIME
file_watch.c      文件快照与变化检测
live_reload.c     SSE 客户端集合和 reload 事件
```

拆分后的模块接口应尽量接受数据并返回结果，不要求测试建立真实网络连接。只有 socket 和平台调用留在 adapter 中。

### 验收标准

- HTTP 请求解析和路径解析具有无 socket 单元测试。
- 文件监控可以对临时目录进行确定性测试。
- SSE 客户端集合的新增、移除和失效连接可独立测试。
- `cmd_serve.c` 仅负责组合模块和驱动事件循环。

## 阶段 8：结构化诊断

在保留现有错误码的基础上，为 context 或构建会话增加详细诊断信息。

期望输出示例：

```text
content/en/post.md:4: invalid date "2026-13-40"
content/zh/a.md: duplicate slug "hello"; first defined in content/zh/b.md
themes/default/post.html: missing required variable {{content}}
```

### 工作内容

- 诊断包含错误码、消息、文件路径和可选行号。
- 区分 error、warning 和 note。
- 同一验证阶段尽量汇总多个内容错误，而不是只报告第一个。
- CLI 保持简洁输出，并为未来机器可读格式保留演进空间。

### 验收标准

- 所有用户可修复的解析、配置和冲突错误都包含具体上下文。
- 测试验证错误类别及关键字段，不依赖整段输出的脆弱快照。

## 阶段 9：用户功能扩展

质量基础完成后，再按实际需求选择功能：

1. 双语创作命令：

   ```bash
   cxo new --lang en --id hello "Hello"
   ```

2. 自定义页面，例如 About、Projects 和独立落地页。
3. 生成静态 `search.json`，搜索交互由主题实现。
4. 文章规模足够大且有数据证明全量构建成为瓶颈后，再实现增量构建。
5. 根据用户需求考虑 Atom 或 JSON Feed。

这些功能在进入实现前应各自补充简短 spec，不应同时混入结构重构。

## 建议里程碑

### Milestone A：质量护栏

- 跨平台 CI。
- ASan/UBSan。
- 初始模糊测试入口。
- `cxo check`。

完成标志：每个提交都能自动证明它在目标平台上可构建、可测试，并能在发布前发现主要内容错误。

### Milestone B：可靠构建

- 输出路径冲突检测。
- 原子构建。
- 结构化诊断。

完成标志：失败构建不会破坏已有站点，用户能从错误信息直接定位和修复问题。

### Milestone C：内部深模块

- 站点派生模型。
- Arena 字符串构建器。
- 开发服务器内部拆分。

完成标志：过滤、分组和字符串安全规则集中在少量接口后面，新渲染功能无需复制现有实现。

### Milestone D：创作体验

- 双语 `cxo new`。
- 自定义页面。
- 可选静态搜索索引。

完成标志：在不削弱可靠性和极简定位的前提下，改善真实博客的日常创作体验。

## 实施约束

- 每项改进独立提交，避免把行为变更和大规模重构放在同一提交。
- 重构前先补充锁定当前行为的测试。
- 新增核心逻辑必须使用 Arena；只有第三方库明确返回堆内存时才单独释放。
- 所有平台代码继续通过 `include/platform.h` 隔离。
- 函数保持约 50 行以内、最多三层嵌套、所有控制语句使用 braces。
- 每个里程碑完成后运行 `make test`、sanitizer、静态分析并检查完整 diff。

## 暂不实施

- 插件系统或通用模板语言。
- 常驻数据库或动态后端。
- 为理论上的替换需求增加多层 adapter。
- 在没有性能数据前引入并行渲染或复杂缓存。
- 将简单静态博客扩展成通用 CMS。

这些方向会明显扩大接口和维护面，与 CXO 当前的极简定位不符。
