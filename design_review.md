# CXO 设计评审与改进建议 (v1.2)

本文档从**设计层面**（而非代码风格层面）审视 CXO 当前架构，列出可改进之处。
评审基线：`main` 分支，2026-07-31。所有问题均附文件与行号定位。
行号以基线为准；多数高优先级问题已修复，见下方进展表与各节状态注记。

---

## 评审后进展 (v1.2，2026-08-02 更新)

P0 与 P1 项已全部完成。修复条目如下：

| 条目 | 状态 | 说明 |
| :--- | :--- | :--- |
| §1.1 og:url 中文文章 404 | ✅ 已修复（含根因） | bug 修复：`get_output_subdir()`（commit `5a6e6a2`）；根因修复：`cxo_entry_url()` 成为全站唯一 URL 构造入口，meta/RSS/sitemap/lang switch/prev-next/列表全部迁移（commit `dc5f058`） |
| §1.2 模板变量不转义 | ✅ 已修复 | meta 属性转义（`5a6e6a2`）；通用契约落地：纯文本变量（title/date/description/site_title/tag_name 及列表/导航标题）注入前统一 `escape_attr()`，HTML 变量明确为 trusted，契约写入 AGENTS.md（commit `8392acc`） |
| §1.3 错误吞没 | ✅ 已修复 | `process_entries()` 返回错误计数，`do_build()` 在计数 > 0 时渲染完毕后以非零退出并打印 `Error: Build completed with N error(s)`（commit `8e5a7f0`） |
| §1.4 解析失败段错误 | ✅ 已修复 | 新增 `set_entry_defaults()`，在文件不可读与 cmark 解析失败两条路径上统一设置安全默认值（空 `html_content`/`toc`、`Untitled`、`1970-01-01`）。验证：同一不可读文章从 SIGSEGV 变为正常渲染 + rc=1 报错（commit `8e5a7f0`） |
| §2.1 renderer.c 巨型模块 | ✅ 已修复 | 2352 → 112 行（仅留 `cxo_render_site` 编排）；新模块 template/render_posts/render_index/render_taxonomy/render_feeds/path_util + 内部头 `renderer_internal.h`；纯搬迁无逻辑改动，产物字节级一致（commit `b096ac2`） |
| §2.2 语言二分法硬编码 | ✅ 已修复 | `cxo_lang_t` 语言描述表（code/prefix/locale/label）落地 `src/lang.c`，~20 处硬编码判断全部迁移；og:locale 硬编码问题随之解决（commit `2e656bd`） |
| §3.4 meta 标签截断 | ✅ 已修复 | meta 缓冲区改为按转义后长度在 arena 分配，截断类已消除（commit `5a6e6a2`）；TOC/watch list 的静默截断仍在 |
| §5 中文 TOC slug | ✅ 已修复 | `slugify()` 保留 ≥0x80 的 UTF-8 字节，中文标题得到可用锚点（如 `安装指南`）；空 slug 回退 `heading-N`（commit `8e5a7f0`）。cmark AST 重写与内联标签噪声问题仍未做 |
| 附带修复 | ✅ | `base_url` 末尾斜杠归一化（`config.c`）、tag 页 og:url 动态分配、`tag.html`/`archive.html` 补 `{{meta_tags}}`、post/site meta 构造函数合并（commit `5a6e6a2`） |

---

## 0. 总体评价

CXO 的四阶段流水线（Scanner → Parser → Linker → Renderer）职责清晰，
Arena 分配器一刀切地解决了内存生命周期问题，双语作为一等公民的设计
在同类静态博客引擎中是差异化优势。评审时（2026-07-31）的改进空间
主要集中在：

1. **语言模型硬编码** —— zh/en 二分法散落各处，且已产生实际 bug；
2. **renderer.c 巨型模块** —— 2170 行承载了 6 种产物的生成逻辑；
3. **输出安全性** —— 模板与 meta 标签注入未做 HTML 转义；
4. **静默降级哲学** —— 多处错误被吞掉，构建"成功"但产物不完整。

> **状态更新（2026-08-02）**：第 1、2、3 项已系统性解决（语言描述表
> `2e656bd`、模块拆分 `b096ac2`、转义契约 `5a6e6a2`+`8392acc`）；
> 第 4 项的构建侧已解决（错误计数 `8e5a7f0`），TOC/watch list 的
> 静默截断仍开放（见 §3.4）。

以下按优先级分组论述（正文保留基线状态描述，各节附状态注记）。

---

## 1. 高优先级：已产生或即将产生实际错误

### 1.1 og:url 对中文文章生成错误 URL（设计缺陷导致的实际 bug，已修复 ✅）

`build_post_meta_tags()`（`src/renderer.c:695`）用统一的
`%s/%s/posts/%s.html` 模板拼接 URL：

```c
"<meta property=\"og:url\" content=\"%s/%s/posts/%s.html\">\n",
desc, entry->title, desc, base, entry->lang, entry->slug, ...
```

中文文章得到 `base/zh/posts/slug.html`，但实际路由是 `base/posts/slug.html`
（zh 是默认语言，见 `get_output_subdir()`，`src/renderer.c:508`）。
**所有中文文章的 og:url 都是错的**，社交分享抓取会拿到 404。

根因不是这一行写错了，而是 **URL 路由规则没有一个单一权威来源**：
`get_output_subdir()`、`build_lang_switch()`（`renderer.c:494`）、
`build_post_meta_tags()`、`build_tag_links()`（`renderer.c:992`）、
RSS/sitemap 各自用自己的方式拼 URL，迟早会不一致。

**建议**：引入统一的 URL 构造函数，全站唯一入口：

```c
/* 生成某 entry 的规范 URL 路径，如 "/posts/foo.html" 或 "/en/posts/foo.html" */
char* cxo_entry_url(arena_t* arena, const cxo_entry_t* entry);
```

RSS、sitemap、meta tags、lang switch、prev/next 全部调用它。

> **状态更新（2026-08-02）**：已按建议落地。`cxo_entry_url()` 在
> `path_util.c` 中实现并成为全站唯一 URL 构造入口，上述消费方全部
> 迁移（bug 修复 `5a6e6a2`，根因统一 `dc5f058`）。

### 1.2 模板变量注入不做 HTML 转义（已修复 ✅）

`build_post_meta_tags()`（`renderer.c:691-701`）把 `entry->title`、
`description` 直接插入 HTML 属性值：

```html
<meta property="og:title" content="{title}">
```

标题含 `"` 就会截断属性并注入任意 HTML（如标题
`Foo " onload="alert(1)`）。`replace_var()` 是纯字符串替换，正文外的
所有变量（title、date、tags、TOC 文本）都是未转义注入。
讽刺的是 RSS 侧已有 `escape_xml()`（`renderer.c:1188`），HTML 侧却没有
对应的 `escape_html()`。

**建议**：增加 `escape_html_attr()` / `escape_html_text()`，并明确模板
变量的**转义契约**——哪些变量是"已渲染 HTML"（content、toc、tags），
哪些是"纯文本需转义"（title、description、date）。推荐约定：
纯文本变量在注入前统一转义，HTML 变量以下划线或约定后缀区分
（或文档化为 trusted HTML）。

> **状态更新（2026-08-02）**：已落地。meta 属性先行转义（`5a6e6a2`），
> 随后建立完整契约（`8392acc`）：纯文本变量（title、date、description、
> site_title、site_description、tag_name 及列表/导航中的标题）注入前
> 统一经 `escape_attr()`（转义 `& < > "`）；content、toc、entry_list
> 等明确为 trusted HTML。契约已写入 AGENTS.md「Template Variables」节，
> 采"文档化为 trusted"方案而非命名后缀。

### 1.3 错误吞没：构建可以"成功"地产出残缺站点（已修复 ✅）

`process_entries()`（`src/main.c:65-86`）把解析失败降级为 warning 并继续：

```c
ret = cxo_parse_markdown(entry, arena, NULL);
if (CXO_IS_ERR(ret)) {
    fprintf(stderr, "Warning: Failed to parse entry %s\n", entry->id);
}
```

解析失败的文章带着 `NULL` 的 `html_content`、`title == "Untitled"`、
`date == "1970-01-01"` 进入渲染，`cxo build` 仍返回 0。CI 部署场景下，
一篇 frontmatter 写坏的文章会悄悄变成 "Untitled / 1970-01-01" 上线。

类似地，`build_post_meta_tags()` 在超长标题导致 snprintf 截断时返回
空字符串（`renderer.c:702`）——SEO 标签静默消失。

**建议**：区分错误级别。frontmatter 缺失字段可以降级为默认值；
但文件不可读、markdown 解析失败应计入错误计数，构建结束时
非零即返回失败（或至少提供 `--strict` 模式）。截断类问题应报警告。

> **状态更新（2026-08-01）**：错误计数已实现（`8e5a7f0`）——解析/链接
> 失败计数，渲染完毕后非零退出并打印错误数。注意验证时发现了 §1.4
> 的段错误（已一并修复），否则计数路径根本走不到。`--strict` 模式未做，
> 当前行为即严格模式。

### 1.4 解析失败的 entry 使渲染阶段段错误（2026-08-01 新发现，已修复 ✅）

在验证 §1.3 修复时发现：一篇不可读的文章（`chmod 000`）解析失败后，
`cxo build` 以 **SIGSEGV（rc=139）崩溃**，而非走完渲染流程。

复现：`content/zh/` 下放一篇不可读的 `.md`，执行 `cxo build`。
输出停在 `Warning: Failed to parse entry <id>` 之后。

根因：文件不可读时 `cxo_parse_markdown()` 在读文件失败后提前返回，
entry 的 `html_content`/`toc`/`date` 均为 NULL（默认值设置在更后面），
渲染管线（日期排序的 `strcmp`、`replace_var` 等）解引用 NULL 崩溃。
该崩溃在 §1.3 修复前就存在，属于潜在的 NULL 解引用，而非新引入。

**修复**（commit `8e5a7f0`）：新增 `set_entry_defaults()` 助手，
在文件不可读与 cmark 解析失败两条路径上统一设置安全默认值
（空 `html_content`/`toc`、`Untitled`、`1970-01-01`），并替换了正常
路径上原有的内联默认值块。验证：同一不可读文章从 SIGSEGV 变为
正常渲染 + `Error: Build completed with 1 error(s)` + rc=1。

---

## 2. 架构层面

### 2.1 renderer.c 是巨型模块（2170 行，6 种产物，已拆分 ✅）

一个文件承载：文章页、分页索引、标签页、年/月归档、RSS、sitemap、
主题资源拷贝、静态资源拷贝、模板加载、SEO meta、prev/next 导航。
61 个函数挤在一起，已经违反自身"函数单一职责"的约定精神。

**建议拆分**（保持现有函数原样搬迁即可，纯机械重构）：

| 新文件 | 内容 |
| :--- | :--- |
| `template.c` | `load_*_template`、`replace_var`、fallback 模板 |
| `render_posts.c` | 文章页 + prev/next + lang switch |
| `render_index.c` | 索引 + 分页 |
| `render_taxonomy.c` | 标签页 + 归档页 |
| `render_feeds.c` | RSS + sitemap |
| `path_util.c` | `ensure_dir`、`copy_file`、`copy_dir_recursive`、URL 构造 |

`cxo_render_site()` 留在 `renderer.c` 作为编排入口。

> **状态更新（2026-08-02）**：已按上表拆分（`b096ac2`）。renderer.c
> 2352 → 112 行仅留编排；六个模块 + 内部头 `renderer_internal.h`；
> 单文件使用的函数保持 static，跨文件函数去 static 入内部头。
> 纯搬迁无逻辑改动，全站产物字节级一致。

### 2.2 语言模型：zh/en 二分法硬编码在 10+ 处（已修复 ✅）

`strcmp(entry->lang, "en") == 0` 的判断散布在 scanner、renderer、
linker 各处；`assign_prev_next()` 用 `last_zh`/`last_en` 两个局部变量
（`renderer.c:559-580`）——加第三种语言要改动一大片，而且 zh 作为
"默认语言"的语义（URL 无前缀）只是约定俗成。

**建议**：引入语言描述表作为单一事实来源：

```c
typedef struct {
    const char* code;     /* "zh" */
    const char* prefix;   /* "" (默认语言) 或 "en" */
    const char* locale;   /* "zh_CN"，给 og:locale 用 */
    const char* label;    /* "中文"，给 lang switch 用 */
} cxo_lang_t;
```

扫描器遍历该表而非写死两个 `scan_lang_dir()` 调用；prev/next 分配改成
按语言分组的小哈希/数组。即便项目决心只支持双语，这张表也能消除
目前 1.1 那种 URL 不一致 bug 的整个类别。

> **状态更新（2026-08-02）**：已按建议落地（`2e656bd`）。`cxo_lang_t`
> 表（含 code/prefix/locale/label 四字段，与建议一致）在 `src/lang.c`，
> 提供 `cxo_lang_find()` / `cxo_lang_index()`；scanner、prev/next
> （改为按表索引的小数组）、og:locale、lang switch、sitemap/RSS/索引/
> 归档路径、`cxo init/new` 共 ~20 处全部迁移。加第三种语言现在只需
> 在表里加一行（外加翻译内容目录）。

### 2.3 双配置体系：config.toml 与环境变量并存且优先级未定义

`config.toml` 管 site/theme，而草稿（`CXO_DRAFT`）和热重载
（`CXO_HOTRELOAD`）走环境变量（`renderer.c:670`、`renderer.c:881`）。
用户无法在一个地方看到全部开关；`cxo serve` 内部还要通过
`setenv("CXO_HOTRELOAD")` 把状态传递给子进程重建，属于用环境变量
当 IPC 使。

**建议**：统一为「config.toml 为基底，CLI 标志覆盖」两层，
环境变量仅作为 CI 场景的第三层覆盖，并在 AGENTS.md 写明优先级。
热重载标志应直接作为参数传入渲染函数，而不是依赖 `getenv()` 查全局状态。

### 2.4 main.c 的命令分发：extern 声明 + 长 if-else 链

`main.c:17-21` 用 `extern int cmd_init(...)` 硬声明各命令函数，
没有对应头文件；分发是 60 多行的 if-else 链，参数解析（如 serve 的
`-w` 和端口）也内联在 main 里。

**建议**：命令表驱动：

```c
typedef struct {
    const char* name;
    const char* alias;    /* "g" / "s" / NULL */
    const char* usage;
    int (*run)(int argc, char** argv);
} cxo_cmd_t;
```

每个命令自己解析 argv（main 只负责 `argv+2` 转发），`help` 命令
遍历表自动生成帮助文本——新增命令只需注册一行。
另外注意 `design.md` 承诺的 `cxo s` / `cxo v` 等简写目前只有
`g`/`-v` 实现了一半，命令表能顺带补齐。

---

## 3. 数据结构设计

### 3.1 `md_content` 字段名误导

`cxo_entry_t.md_content` 的注释写 "Raw markdown content (file path)"
（`include/cxo.h:27`）——它存的是**文件路径**而非内容，`parser.c:302`
的注释自己都承认这一点（"entry->md_content still holds the file path"）。
名字、类型语义、注释三者不一致，新人必踩。

**建议**：改名 `src_path`。如果将来要支持内存内容，再加 `md_source` 区分。

### 3.2 id 与 slug 的指针共享

scanner 里 `entry->id = entry->slug`（`src/scanner.c:99`）让两个字段
共享同一指针，parser 里又有 `if (!entry->id) entry->id = entry->slug;`
（`src/parser.c:319-321`）——但 scanner 已经保证 id 非 NULL，
这是死代码。共享指针在 Arena 模型下没有 double-free 风险，
但语义上"修改 id 会不会连带改 slug"成了一道阅读理解题。

**建议**：要么 scanner 不设 id（让 parser 统一兜底），要么删掉
parser 的死代码并注释明确"无 frontmatter id 时与 slug 同源"。

### 3.3 date 作为裸字符串贯穿全程

日期比较靠 `strcmp`（`renderer.c:536`），排序正确性完全依赖用户手写
`YYYY-MM-DD` 格式不出错；写个 `2026-3-1` 就会排乱，且无任何校验。
RSS 侧还要再做一次字符串→RFC822 的转换（`rfc822_date()`）。

**建议**：parser 阶段把 date 归一化为 `struct { int y, m, d; }` 或
epoch，校验失败给警告并回退默认值。比较、格式化（RFC822、
归档年月分组）都从结构化字段派生，消灭字符串当日期用的整个类别。

### 3.4 各类硬性上限缺乏系统性策略

| 上限 | 位置 | 超限行为 |
| :--- | :--- | :--- |
| 文章 1024 篇 | `scanner.c:17` | 报错中止（尚可） |
| 标题 64 个/篇 | `parser.c:344` | **静默截断 TOC** |
| 监听路径 32 个 | `cmd_serve.c:31` | **静默漏监听** |
| 文件 1MB | `parser.c:15` | 当不可读处理 |
| 哈希表 64 桶 | `linker.c:12` | 固定，千篇文章时退化为链表 |
| TOC 缓冲 `hcount*256+128` | `parser.c:542` | 超长标题靠 snprintf 截断兜底 |

**建议**：上限本身符合极简哲学，但应遵循两条统一原则：
(1) 截断必须告警（TOC 和 watch list 目前是静默的）；
(2) 哈希表大小应随 entry 数量伸缩（如 `next_pow2(count * 2)`）。
1024 篇上限建议改为动态扩容（Arena 下可做一次性 grow-copy）。

---

## 4. 模板系统

纯 `{{var}}` 替换极简且够用，但有两个结构性代价：

1. **`generate_html()` 是 13 次全串替换**（`renderer.c:743-814`），
   每次 `replace_var` 都扫描并复制整个模板，复杂度 O（模板大小 × 变量数）。
   且每步都要判空，样板代码占 70 行。
   **建议**：改为变量表 `{key, value}` 数组 + 单遍扫描替换，
   一遍过完所有 `{{...}}`，未知变量告警（现在拼错变量名会静默输出
   字面 `{{titel}}`）。

2. **无条件渲染能力**，导致 fallback 模板里 prev/next/lang_switch
   只能渲染空字符串占位。这是极简与表达力的取舍，可不动；
   但建议把"列表变量是预渲染 HTML 片段"这一契约写进主题开发文档，
   并校验模板中出现的未知变量名。

---

## 5. TOC 生成：扫 HTML 字符串是脆弱设计

`cxo_generate_toc()`（`parser.c:376-595`）在 cmark 输出的 HTML 上做
字符串扫描：用 `strstr` 找 `</h%d>`，两遍扫描 + 原地插 id 属性。
脆弱点：

- 标题内含内联标记（`` ## `code` 标题 ``）时，TOC 文本会带着
  `<code>` 标签原样进 `<a>` 文本；
- 标题内含嵌套标签时 `strstr` 找闭合标签可能错位；
- slugify 只对 ASCII 有效，**中文标题全部 slugify 成空串**，
  导致中文文章的 TOC 锚点是 `id=""` 或重复的 `-2`、`-3`。

**建议**：cmark 提供 AST 迭代（`cmark_iter_*`），在渲染 HTML 之前
遍历 AST 收集 heading 节点、取其纯文本子节点拼接 TOC 文本；
id 注入可通过 cmark 的自定义渲染或在 AST 阶段完成。
中文 slug 问题更根本的解法：slugify 保留非 ASCII 的 UTF-8 字节
（现代浏览器与搜索引擎均支持 Unicode 锚点），或对空 slug 回退为
`heading-N` 序号。

> **状态更新（2026-08-01）**：中文 slug 已按上述更根本的解法修复——
> `slugify()` 保留 ≥0x80 的 UTF-8 字节，空 slug 回退 `heading-N`
> （commit `8e5a7f0`）。cmark AST 重写与内联标签噪声问题仍开放。

---

## 6. 开发服务器（cmd_serve.c，900 行）

1. **fork/exec 重建依赖 PATH**（`cmd_serve.c:657`）：`execlp("cxo", ...)`
   要求二进制在 PATH 或当前目录，否则热重载静默失效。
   **建议**：改为进程内重建——serve 直接调用 `do_build()` 同款的
   build 函数（每次新建/销毁 Arena，天然无泄漏）。这还能省掉
   用环境变量传递 CXO_HOTRELOAD 的 IPC  hack（见 2.3）。

2. **单 SSE 客户端**：`sse_client` 是一个 `int`（`cmd_serve.c:417`），
   第二个打开的标签页拿不到 reload 事件。改成小型 fd 数组即可。

3. **轮询 mtime + 32 路径上限**：深度目录结构下内容文件数很容易
   超过 32，超出部分静默不监听。macOS 上可用 kqueue，但更简单的是
   维持轮询、把上限改为动态数组并告警。

---

## 7. 测试设计

测试是「对仓库自身 `content/` 目录跑真实流水线」的集成测试
（CLAUDE.md 自述）。问题：

- **测试与内容耦合**：往 `content/zh/` 加一篇博文就可能改变
  test_renderer 的断言结果；
- **纯函数无单元测试**：`slugify`、`trim`、`escape_xml`、`url_decode`
  这类纯逻辑最适合单测，目前只能靠整站构建间接覆盖；
- 边界场景（无 frontmatter、重复 id、空 tags、超长标题）没有
  对应的 fixture 文章。

**建议**：`tests/fixtures/` 下建独立的迷你站点（含边界 case 文章），
测试在临时目录构建并断言产物；纯函数抽出后可单独链接小单测。

---

## 8. 改进路线图建议

| 优先级 | 项目 | 工作量 | 收益 |
| :--- | :--- | :--- | :--- |
| ~~P0~~ | ~~统一 URL 构造函数，修 og:url bug（1.1）~~ | ✅ bug + 根因均已修（`5a6e6a2` + `dc5f058`） | — |
| ~~P0~~ | ~~HTML 转义契约 + escape_html（1.2）~~ | ✅ 已修（`5a6e6a2` + `8392acc`，契约入 AGENTS.md） | — |
| ~~P0~~ | ~~修 §1.4 段错误（解析失败 entry 的 NULL 防御）~~ | ✅ 已修复（`set_entry_defaults`，commit `8e5a7f0`） | — |
| ~~P1~~ | ~~构建错误计数，失败即非零退出（1.3）~~ | ✅ 已修复（commit `8e5a7f0`），随 §1.4 修复完整生效 | — |
| ~~P1~~ | ~~renderer.c 拆分（2.1）~~ | ✅ 已完成（commit `b096ac2`，2352 → 112 行 + 6 模块） | — |
| ~~P1~~ | ~~语言描述表（2.2）~~ | ✅ 已完成（commit `2e656bd`，cxo_lang_t 落地） | — |
| ~~P2~~ | TOC 中文 slug（5） | ✅ 已修复（UTF-8 保留 + heading-N 回退）；cmark AST 重写仍为 P2 | 中文 TOC 锚点已可用 |
| P2 | 变量表单遍模板替换（4） | 小 | 性能 + 拼写校验 |
| P2 | serve 进程内重建 + 多 SSE 客户端（6） | 中 | 开发体验 |
| P3 | 命令表驱动 CLI（2.4） | 小 | 扩展性 |
| P3 | date 结构化（3.3）、fixtures 测试（7） | 中 | 长期质量 |

**不建议做的**：为模板引擎加循环/条件、引入多线程渲染、支持插件系统
——这些都直接违背项目「极简、零依赖」的核心目标，属于复杂化而非改进。
