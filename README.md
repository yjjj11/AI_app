# My AI App / 渐知面试官（Interview Copilot）

一个基于 libhv 的 Web 服务，提供登录鉴权、对话 UI、会话/历史管理、可配置的 LLM Profiles，并支持“记忆索引 + 向量检索（Qdrant + 云端 Embedding）”与面试业务闭环（JD 解析 / 题单 / 模拟面试 / 复盘）。

学习手册：见 [docs/learning-handbook.md](docs/learning-handbook.md)

## 功能

- Web 登录鉴权：JWT（Cookie 或 Authorization Bearer）
- 对话页面：`/ai.html`（SSE 流式输出）
- 会话管理：新建/删除/列表
- 聊天记录：异步写入 MySQL（阻塞队列 + 后台线程）
- 模型配置：`/settings.html` 配置/切换 LLM Profile，并持久化到 `llm_profiles.json`
- 多模态图片：当 `capabilities.vision=false` 时，后端拒绝带图片的请求
- 结构化记忆索引：会话历史超过阈值时异步生成结构化笔记并分块，向量化后写入 Qdrant
- RAG 检索增强：按 `session_id + memory_type` 过滤召回 topK chunk 作为可控上下文注入
- 面试业务：JD 解析、题单生成、模拟面试（SSE）、复盘总结（触发记忆索引）

## 依赖

- CMake >= 3.10
- C++20 编译器
- OpenSSL（HTTPS / TLS）
- MySQL client（`libmysqlclient` + headers）

## 快速开始

1) 准备配置文件（按模板复制）

- `config/mysql.env.template` → `config/mysql.env`
- `config/ai.env.template` → `config/ai.env`
- `config/agent.config.template` → `config/agent.config`（Qdrant/Embedding/RAG 相关）
- `config/llm_profiles.json.template` → `config/llm_profiles.json`（LLM Profiles）

说明：`config/*.env`、`config/agent.config`、`config/llm_profiles.json` 会被 `.gitignore` 忽略，避免把密钥提交到仓库。

2) 确保 MySQL 可用

- 默认会连接 `MYSQL_HOST/MYSQL_PORT` 指定的 MySQL，并自动创建表（若不存在）

3) 编译

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

默认会从 `third_party/libhv` 编译 libhv；如需改用系统 libhv：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DUSE_BUNDLED_LIBHV=OFF
cmake --build build -j
```

4) 运行

```bash
./bin/app
```

默认监听 `http://127.0.0.1:8888`（可用 `HTTP_PORT` 覆盖）。

## 配置说明

### MySQL（必需）

启动时会读取：

- `/root/my_ai_app/config/mysql.env`

字段含义见模板：[mysql.env.template](file:///root/my_ai_app/config/mysql.env.template)

### LLM Profiles（必需）

启动时会读取：

- `config/llm_profiles.json`（不存在则回退到 `config/llm_profiles.json.template`）

结构示例：

```json
{
  "active_key": "default",
  "profiles": [
    {
      "key": "default",
      "api_key": "sk-***",
      "base_url": "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions",
      "default_model": "qwen-plus",
      "timeout_seconds": 30,
      "capabilities": { "vision": false, "audio": false, "files": false }
    }
  ]
}
```

你可以通过 `/settings.html` 页面创建/切换 profile（后端会写回 `llm_profiles.json`）。

### Agent / 向量检索（可选，但与记忆/RAG相关的功能需要）

启动时会读取：

- `config/agent.config`（不存在则回退到 `config/agent.config.template`）

配置项（优先级：环境变量 > agent.config > 默认值）：

- `QDRANT_URL`：Qdrant Cloud 地址（例如 `https://xxx.aws.cloud.qdrant.io:6333`）
- `QDRANT_API_KEY`：Qdrant API Key（建议只通过环境变量注入，避免密钥落盘/提交）
- `QDRANT_COLLECTION`：collection 名（默认 `ai_app`）
- `QDRANT_VECTOR_SIZE`：向量维度（建议与 embedding 输出一致，例如 1024）
- `QDRANT_DISTANCE`：`cosine`/`dot`/`euclid`（会自动归一化为 Qdrant 需要的 `Cosine/Dot/Euclid`）
- `QDRANT_TIMEOUT`：秒（默认 30）
- `EMBEDDING_BASE_URL`：OpenAI 兼容 embedding endpoint（如 `.../v1/embeddings`）；不填则尝试复用当前 active 的 LLM profile 推导
- `EMBEDDING_API_KEY`：embedding key（不填则尝试复用当前 active 的 LLM profile key）
- `EMBEDDING_MODEL`：embedding 模型名（默认 `text-embedding-v4`）
- `EMBEDDING_DIMENSIONS`：embedding 维度（默认 1024）
- `EMBEDDING_TIMEOUT`：秒（默认 30）
- `MEMORY_TRIGGER_MESSAGES`：触发“结构化记忆索引”的最小历史条数（默认 100）
- `RAG_TOP_K`：检索召回 topK（默认 5）
- `RAG_CONTEXT_CHAR_BUDGET`：检索上下文预算（默认 2500 字符）

### 其它环境变量（可选）

- `HTTP_PORT`：服务监听端口（默认 8888）
- `JWT_SECRET`：JWT 签名密钥（默认 `dev_secret`，建议生产环境设置）

另外项目启动会读取 `/root/my_ai_app/config/ai.env`（用于本地管理密钥/参数；当前 LLM 实际以 `llm_profiles.json` 为准）。

## Web 页面

- 登录页：`GET /`  
  默认账号：`admin` / `123456`
- 对话页：`GET /ai.html`（需要登录）
- 设置页：`GET /settings.html`（需要登录）

## API 一览

### 登录

- `POST /login`  
  - 表单或 JSON：`username`, `password`
  - 成功：写入 Cookie `token=...`

### 会话与历史

- `POST /chat/session/new`：`{ "title": "..." }`
- `POST /chat/session/delete`：`{ "sessionId": "..." }`
- `GET /chat/sessions?limit=50`
- `POST /chat/history`：`{ "sessionId": "...", "limit": 50, "offset": 0 }`

### 发送消息（SSE）

- `POST /chat/send`
- 请求 JSON：
  - `sessionId`：可选，默认 `default`
  - `question`：必填
  - `images`：可选，最多 4 张；每张为 data URL（`data:image/...;base64,...`）
- SSE 事件：
  - `event: delta`：`{"delta":"..."}`（流式增量）
  - `event: error`：`{"error":"..."}`（错误）
  - `event: done`：`{"done":true,"len":123}`（完成）

### 面试业务（最小闭环）

- `POST /interview/jd/parse`：`{"jd":"...","direction":"..."}` → JSON
- `POST /interview/plan`：`{"jd":"...","resume":"...","direction":"..."}` → JSON
- `POST /interview/turn`：`{"sessionId":"...","jd":"...","resume":"...","direction":"...","answer":"..."}` → SSE（delta/error/done）
- `POST /interview/review`：`{"sessionId":"...","direction":"..."}` → JSON（并触发记忆索引）

### 调试接口

- `GET /debug/sse/hold?seconds=60`：每秒发一个 `delta`，结束时发 `done`（用于长连接稳定性验证；需要 `ENABLE_DEBUG_ROUTES=1` 才会注册/放行）

### LLM 配置

- `GET /llm/profiles`：列出 profiles + 当前 activeKey
- `POST /llm/config`：新增/更新 profile（并可选设为 active）
  - `modelType`, `apiKey`, `baseUrl`, `timeout`（可选）, `setActive`（可选）, `persist`（可选）
  - `capabilities`（可选）：`{vision,audio,files}`；不传则按模型名自动推断
- `POST /llm/active`：切换 active profile：`{ "key": "...", "persist": true }`

## 数据库表

启动时会自动创建（若不存在）：

- `chat_sessions`：`username`, `session_id`, `title`, `created_at_ms`
- `chat_messages`：`username`, `role`, `model`, `content`, `created_at_ms`, `session_id`
- `notes`：结构化笔记（短期/长期/复盘等）
- `note_chunks`：笔记分块（含 qdrant_point_id 等）

异步写入机制：

- `/chat/send` 把 user/assistant 两条消息入队
- 后台线程批量从队列取出写入 MySQL（见 `ChatMySqlStore`）

## 开发与调试

- 编译产物：`bin/app`
- 根目录 `compile_commands.json` 会软链接到 `build/compile_commands.json`
- 测试/自检可执行（位于 `bin/`）：
  - `bailian_smoketest`：验证流式接口（SSE）
  - `sendmessage_smoketest`：验证发送消息链路（依赖 MySQL）
  - `llm_factory_smoketest`：验证 profiles 加载与实例复用
  - `model_switch_smoketest`：验证模型配置与切换（依赖 profiles）
  - `embedding_smoketest`：验证 embedding 请求与解析（依赖 Embedding 配置）
  - `qdrant_smoketest`：验证 Qdrant health/collection/upsert/search（依赖 Qdrant + Embedding）
  - `memory_index_smoketest`：验证“>100 条触发索引”并写入 MySQL + Qdrant（依赖 MySQL + Qdrant + Embedding）
  - `sse_hold_smoketest`：验证单连接 SSE 持续推送（依赖 app 正在运行）
  - `rag_hit_rate_eval`：读取 jsonl 标注样本，计算 topK 命中率并输出 base/cand 对比（依赖 Qdrant + Embedding）
  - `scripts/e2e_smoke.sh`：最小端到端验证（/health + SSE hold）
  - `scripts/e2e_full.sh`：扩展端到端验证（会根据环境自动跳过外部依赖）

## 开源协作

- 贡献指南：见 `CONTRIBUTING.md`
- 安全说明：见 `SECURITY.md`
- License：见 `LICENSE`

## 常见问题

- 编译链接出现重复符号：项目通过 `-Wl,-z,muldefs` 允许重复定义以保证构建通过。
- 登录后仍 401：检查 `JWT_SECRET` 是否在多处启动环境不一致；或清理旧 Cookie。
- 图片发送失败：检查当前 active profile 的 `capabilities.vision` 是否为 true。
- Qdrant/Embedding 相关能力无效果：确认 `QDRANT_URL/QDRANT_API_KEY` 与 embedding endpoint/key 已配置，且 `QDRANT_VECTOR_SIZE` 与 embedding 输出维度一致。
- `/chat/send` 返回 `LLM request failed`：确认当前 active profile 指向可访问的 LLM（建议先用 `POST /llm/active` 切换到 `default`，或在设置页切换）。
