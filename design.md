这份设计文档旨在为 **CXO** 提供一个清晰的工程蓝图。作为一名 C 语言内核工程师，这份文档规避了高级语言的臃肿，专注于内存效率、指针逻辑和极简的构建流程。

---

# CXO 项目设计文档 (v1.0)

**项目描述**：A minimalist, high-performance static blog engine written in pure C.
**核心目标**：原生支持中英双语、零构建依赖、极致的编译速度。

---

## 1. 系统架构 (System Architecture)

CXO 采用典型的**静态编译流水线**设计，所有数据在处理过程中尽量驻留在内存中，减少不必要的磁盘 I/O。



### 1.1 核心流程
1.  **初始化 (Init)**：加载 `config.toml`，初始化全局上下文和内存池 (Arena)。
2.  **扫描 (Scanner)**：递归遍历 `content/zh` 和 `content/en` 目录，建立文件清单。
3.  **解析 (Parser)**：
    * 提取 Markdown 的 Front-matter（YAML/TOML 格式）。
    * 调用 `libcmark` 将正文转换为 HTML 片段。
4.  **关联 (Linker)**根据 `id` 字段，通过哈希表在内存中将对应的中英文 `cxo_entry_t` 结构体进行指针关联。
5.  **生成 (Renderer)**：将数据注入 HTML 模板，输出到 `public/` 目录。

---

## 2. 核心数据结构 (Core Structures)

在 `cxo.h` 中，我们将定义如下核心对象，确保多语言逻辑是一等公民。

```c
/* 单篇文章实体 */
typedef struct cxo_entry {
    char *id;               // 跨语言关联的唯一标识
    char *lang;             // "zh" 或 "en"
    char *title;            // 文章标题
    char *date;             // 发布日期
    char *slug;             // URL 路径名
    char *html_content;     // 解析后的正文 HTML
    struct cxo_entry *peer; // 指向另一种语言的译文实体（若无则为 NULL）
} cxo_entry_t;

/* 全站上下文 */
typedef struct {
    cxo_entry_t **entries;  // 动态数组存储所有文章
    size_t count;
    char *base_url;
    char *theme_path;
} cxo_context_t;
```

---

## 3. 多语言路由设 (I18n Routing)

CXO 不通过复杂的插件实现双语，而是直接在**生成逻辑**中硬编码路由规则：

* **存储结构**：
    * 中文版（默认）：`public/posts/my-tech-blog.html`
    * 英文版：`public/en/posts/my-tech-blog.html`
* **关联逻辑**：
    在渲染页面时，检查 `peer` 指针。如果存在，则在模板变量 `{{nav_lang_switch}}` 中填入对应的 URL 链接，实现一键切换。

---

## 4. 技术栈选型 (Tech Stack)

| 组件 | 选型 | 理由 |
| :--- | :--- | :--- |
| **开发语言** | Pure C (C11) | 极致性能、无运行时依赖、底层掌控。 |
| **Markdown 解析** | [libcmark](https://github.com/commonmark/cmark) | 工业级标准，纯 C 实现，速度极快。 |
| **配置解析** | [toml-c](https://github.com/cktan/toml-c) | 兼容 TOML 标准，代码量极小。 |
| **内存管理** | **Arena Allocation** | 针对静态构建场景，统一分配/释放，消除碎片。 |
| **构建工具** | GNU Make | 简单、可靠、跨台。 |

---

## 5. 目录规范 (Directory Layout)

```text
.
├── LICENSE            # MIT License
├── Makefile           # 构建指令
├── src/               # C 源码 (main.c, parser.c, linker.c...)
├── include/           # 头文件 (cxo.h)
├── themes/            # HTML 模板与 CSS
└── content/           # 创作目录
    ├── zh/            # 中文文章 (.md)
    └── en/            # 英文文章 (.md)
```

---

## 6. 开源协议 (License)
**MIT License**
* **署名**：Mitchell (aq1u)
* **年份**：2026

---

## 7. CLI 命令设计

| 命令 | 简写 | 功能描述 | 优先级 |
| :--- | :--- | :--- | :--- |
| `cxo init [dir]` | `cxo init` | 在指定目录创建 `content/`, `themes/`, `config.toml` 等基础结构 | P1 |
| `cxo new "title"` | `cxo new` | 在 `content/zh/` 生成带有标准 Front-matter 模板的 `.md` 文件 | P2 |
| `cxo build` | `cxo g` | 核心逻辑：扫描、解析、链接、渲染，输出到 `public/` | P0 |
| `cxo clean` | `cxo clean` | 递归删除 `public/` 目录下的所有内容 | P1 |
| `cxo server` | `cxo s` | 启动极简 HTTP Server（建议用 epoll 配合单线程） | P2 |
| `cxo version` | `cxo v` | 显示版本信息 | P0 |
| `cxo help` | `cxo h` | 显示帮助信息 | P0 |

### 命令详细说明

#### cxo init [dir]
在当前目录或指定目录初始化 CXO 项目结构：
```bash
cxo init                    # 当前目录
cxo init my-blog           # 创建 my-blog/ 目录并初始化
```

生成的结构：
```
my-blog/
├── config.toml           # 站点配置
├── content/              # 内容目录
│   ├── zh/              # 中文文章
│   └── en/              # 英文文章
└── themes/              # 主题目录
    └── default/         # 默认主题
        ├── style.css
        └── post.html
```

#### cxo new "title"
创建新文章：
```bash
cxo new "Hello World"      # 创建 content/zh/hello-world.md
```

生成的文件模板：
```markdown
---
id: hello-world
title: Hello World
date: 2026-03-21
---

Write your content here...
```

#### cxo server
启动开发服务器：
```bash
cxo server                 # 默认监听 8080 端口
cxo server -p 3000        # 指定端口
```

---

### 下一步行动 (Next Steps)

1.  **实现 `cxo init`**：项目初始化命令
2.  **实现 `cxo new`**：快速创建文章
3.  **实现 `cxo server`**：开发服务器
