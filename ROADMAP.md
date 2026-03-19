# CXO 开发路线图

## 开发规范

- **分支策略**: 每个功能阶段创建独立的 feature 分支
- **合并方式**: 使用 PR（Pull Request）合并到 main，禁用直接推送
- **提交粒度**: 每个 commit 只做一件事，保持历史清晰
- **测试要求**: 每个阶段必须有测试验证

## 阶段规划

### Phase 1: 基础架构 ✅
**分支**: `feature/arena`  
**状态**: 已完成

- [x] Arena 内存分配器
- [x] 核心数据结构 (cxo.h)
- [x] 上下文管理

**PR**: #1 - add arena allocator and core data structures

---

### Phase 2: 内容扫描器 ✅
**分支**: `feature/scanner`  
**状态**: 已完成

- [x] 递归扫描 content/ 目录
- [x] 识别 .md 文件
- [x] 区分 zh/en 语言目录
- [x] 扫描器测试

**PR**: #2 - add content directory scanner

---

### Phase 3: Markdown 解析器 ✅
**分支**: `feature/parser`  
**状态**: 已完成

- [x] YAML frontmatter 解析
- [x] Markdown → HTML 转换 (libcmark)
- [x] 解析器测试

**PR**: #3 - add markdown and frontmatter parser

---

### Phase 4: 文章链接器 ✅
**分支**: `feature/linker`  
**状态**: 已完成

- [x] 哈希表实现
- [x] 根据 id 关联中英文文章
- [x] 设置 peer 指针
- [x] 链接器测试

**PR**: #4 - add entry linker for bilingual support

---

### Phase 5: HTML 渲染器 ⏳
**分支**: `feature/renderer`  
**状态**: 待开发

- [ ] 模板变量替换
- [ ] HTML 页面生成
- [ ] 多语言路由 (zh: /posts/, en: /en/posts/)
- [ ] 语言切换链接
- [ ] 渲染器测试

**PR**: #5 - add HTML renderer with i18n support

---

### Phase 6: 配置解析 ⏳
**分支**: `feature/config`  
**状态**: 待开发

- [ ] TOML 配置解析 (toml-c)
- [ ] config.toml 加载
- [ ] 站点元数据 (title, description, base_url)

**PR**: #6 - add TOML config parser

---

### Phase 7: 构建系统 ⏳
**分支**: `feature/build-system`  
**状态**: 待开发

- [ ] Makefile 创建
- [ ] 编译规则 (macOS/Linux)
- [ ] 依赖检测 (libcmark)
- [ ] 安装目标

**PR**: #7 - add Makefile build system

---

### Phase 8: CLI 完善 ⏳
**分支**: `feature/cli`  
**状态**: 待开发

- [ ] 完善 main.c 命令处理
- [ ] build 命令集成所有模块
- [ ] version 命令
- [ ] help 命令
- [ ] 错误处理优化

**PR**: #8 - complete CLI commands

---

### Phase 9: 默认主题 ⏳
**分支**: `feature/theme`  
**状态**: 待开发

- [ ] 默认 HTML 模板
- [ ] 基础 CSS 样式
- [ ] 响应式布局
- [ ] 主题目录结构

**PR**: #9 - add default theme

---

### Phase 10: 测试与文档 ⏳
**分支**: `feature/tests`  
**状态**: 待开发

- [ ] 集成测试
- [ ] 端到端测试
- [ ] README 完善
- [ ] 使用文档

**PR**: #10 - add comprehensive tests and documentation

## 当前状态

**已完成**: Phase 1-3  
**进行中**: Phase 4 (linker)  
**待开始**: Phase 5-10

## 开发流程示例

```bash
# 1. 从 main 创建新分支
git checkout main
git pull origin main
git checkout -b feature/linker

# 2. 开发功能... 提交 commit
git add src/linker.c test_linker.c
git commit -m "add linker: associate bilingual entries by id"

# 3. 推送到远程
git push origin feature/linker

# 4. 创建 PR 请求合并到 main
# PR 标题: add entry linker for bilingual support
# PR 描述: 实现根据 id 关联中英文文章的功能...

# 5. 代码审查通过后，使用 Squash Merge 或 Merge Commit 合并
# 6. 删除 feature 分支
```

## 分支命名规范

- `feature/<name>` - 新功能
- `fix/<name>` - Bug 修复
- `docs/<name>` - 文档更新
- `refactor/<name>` - 代码重构
