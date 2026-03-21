# 学习手册：从零读懂本项目（渐知面试官 / My AI App）

目标：让你能“跑起来 → 看见数据流 → 看懂模块边界 → 能改动并回归验证”。

## 0. 你将学到什么
- 一个 C++20 + libhv 的 HTTP 服务，包含登录鉴权、REST API、SSE 流式输出
- MySQL 异步写库（队列 + worker）与会话/历史管理
- 结构化记忆索引：历史超过阈值触发 → LLM 产出结构化笔记 → 分块 → Embedding → Upsert 到 Qdrant
- RAG 检索增强：query 向量化 → Qdrant filter search → chunk 作为上下文注入
- 面试业务闭环：JD 解析 → 题单/追问 → SSE 模拟面试 → 复盘（并触发记忆索引）

## 1. 第一次运行（最小可跑）

### 1.1 准备配置（开源默认不含密钥）
项目默认提供模板：
- `config/mysql.env.template`
- `config/ai.env.template`
- `config/agent.config.template`
- `config/llm_profiles.json.template`

建议新建本地文件（不要提交到仓库）：
- `config/mysql.env`
- `config/ai.env`
- `config/agent.config`
- `config/llm_profiles.json`

### 1.2 编译
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

### 1.3 启动
```bash
HTTP_PORT=8888 ./bin/app
```

如果你还没配置 LLM/Qdrant/Embedding，也可以先启动服务并访问：
- `GET /health`
- `GET /`（登录页）

## 2. 目录与模块导航（先建立全局地图）

### 2.1 核心入口
- `src/main.cpp`：加载配置 → 初始化 server
- `src/HttpServer.cpp`：中间件（CORS/JWT）+ 路由注册

### 2.2 路由层（HTTP API）
- `src/router/LoginRouter.cpp`：`/`、`/login`
- `src/router/ChatRouter.cpp`：会话/历史/`/chat/send` SSE
- `src/router/InterviewRouter.cpp`：面试业务 API（含 SSE）
- `src/router/DebugRouter.cpp`：调试用 SSE（用于验证长连接稳定性）

### 2.3 服务层（业务逻辑）
- `src/service/ChatService.cpp`：构建 messages、调用 LLM 流式生成
- `src/service/InterviewService.cpp`：面试业务编排（plan/turn/review）
- `src/service/ChatMySqlStore.cpp`：异步写库 worker
- `src/service/MemoryIndexService.cpp`：记忆索引 worker（LLM → chunk → embedding → qdrant）
- `src/service/SettingService.cpp`：LLM profiles 管理与持久化

### 2.4 AI/Agentic 组件
- `src/ai/LLMFactory.cpp` / `src/ai/BailianLLM.cpp`：OpenAI-compatible chat 接口（含 SSE 解析）
- `src/agentic/EmbeddingClient.cpp`：OpenAI-compatible embeddings
- `src/agentic/QdrantClient.cpp`：Qdrant upsert/search/health/collection
- `src/agentic/RagRetriever.cpp`：embedding → qdrant search → chunks
- `src/agentic/InterviewPipeline*.cpp`：Plan/Retrieve/Compose/Generate/Reflect 的默认 pipeline

## 3. 一张图看懂数据流

```
浏览器/客户端
  |
  |  /login  -> Set-Cookie: token=JWT
  v
HttpServer middleware (CORS + JWT校验 + 注入 X-Auth-User)
  |
  +--> /chat/session/*, /chat/history (REST)
  |
  +--> /chat/send (SSE)
  |      |  enqueue(user msg) -> ChatMySqlStore(worker) -> MySQL
  |      |  ChatService -> LLMFactory -> BailianLLM(stream) -> SSE delta/done
  |      |  enqueue(assistant msg) -> MySQL
  |      |  maybeEnqueue MemoryIndexService (异步索引)
  |
  +--> /interview/* (REST + SSE)
         |  InterviewPipeline(Plan/Retrieve/Compose/Generate/Reflect)
         |  Retrieve -> RagRetriever -> (Embedding -> Qdrant search) -> chunks
         |  Generate -> LLM(stream) -> SSE delta/done
         |  review -> 结构化复盘 -> maybeEnqueue MemoryIndexService

MemoryIndexService(worker)
  |  拉取会话 tail history（>阈值触发）
  |  LLM 产出结构化笔记 JSON
  |  render -> chunk
  |  EmbeddingClient(embedBatch)
  |  QdrantClient(upsertPoints with payload session_id/memory_type)
  v
Qdrant Cloud（可 filter search）
```

## 4. 关键接口怎么读（带你顺藤摸瓜）

### 4.1 登录与鉴权
- `POST /login`：写入 `token` Cookie（JWT）
- 中间件校验 token 并注入 `X-Auth-User`，后续 DB 隔离都基于该字段

验证：
```bash
curl -i -X POST http://127.0.0.1:8888/login \
  -H 'Content-Type: application/x-www-form-urlencoded' \
  --data 'username=admin&password=123456'
```

### 4.2 /chat/send 的 SSE 流
- 先 `writer->Begin()` 写入 `text/event-stream` headers
- 每次拿到 LLM delta：发 `event: delta`（JSON body）
- 完成：发 `event: done`
- 同步将 user/assistant 两条消息 enqueue 到异步写库队列

验证（需要已登录的 Cookie）：
```bash
curl -N http://127.0.0.1:8888/chat/send \
  -H 'Content-Type: application/json' \
  -H 'Cookie: token=YOUR_JWT' \
  -d '{"sessionId":"default","question":"ping"}'
```

### 4.3 面试业务最小闭环
- `POST /interview/jd/parse`：结构化解析 JD（关键词/考点）
- `POST /interview/plan`：生成题单/追问路线
- `POST /interview/turn`：SSE 模拟面试（会把回答写入 chat_messages）
- `POST /interview/review`：复盘总结并触发记忆索引

## 5. 数据库表与写入策略

启动时自动建表（若不存在）：
- `chat_sessions`
- `chat_messages`
- `notes`
- `note_chunks`

写入策略：
- 请求线程仅入队，后台线程消费队列写入 MySQL，降低 /chat/send 与 /interview/turn 的尾延迟

## 6. 记忆索引与 RAG（为什么这样设计）

### 6.1 触发条件与产物
- 条件：会话历史消息数超过阈值（默认 100，可配置）
- 产物：结构化笔记（facts/preferences/todos/weaknesses/conclusions）与其分块

### 6.2 为什么要分块
- 控制 embedding 请求的单条长度与召回粒度
- 让 filter search 后的上下文更可控、更容易裁剪拼接

### 6.3 Qdrant payload 设计（用于过滤隔离）
- `session_id`：会话隔离
- `memory_type`：短期/长期/复盘等
- `note_id/chunk_index/text_hash/text`：追溯与去重

## 7. 你应该如何验证（自检清单）

自检二进制（在 `bin/`）：
- `llm_factory_smoketest`
- `embedding_smoketest`（需要 embedding 配置）
- `qdrant_smoketest`（需要 qdrant + embedding）
- `memory_index_smoketest`（需要 mysql + qdrant + embedding）
- `sse_hold_smoketest`（需要 app 启动）
- `rag_hit_rate_eval`（需要 qdrant + embedding + 样本集）

SSE 长连接验证：
```bash
HTTP_PORT=19085 ./bin/app
HTTP_BASE_URL=http://127.0.0.1:19085 SSE_SECONDS=60 ./bin/sse_hold_smoketest
```

## 8. 常见错误与排障

### 8.1 端口占用
现象：启动输出 `bind: Address already in use`
解决：切换 `HTTP_PORT`

### 8.2 /chat/send 返回 LLM request failed
现象：SSE 直接发 `event: error`
解决：
- 确认 `config/llm_profiles.json` 已配置可用的 `api_key/base_url/default_model`
- 或用设置页切换 active profile

### 8.3 Qdrant 400 / collection mismatch
现象：upsert/search 报错
解决：
- `QDRANT_VECTOR_SIZE` 与 embedding 输出维度保持一致
- distance 使用 `cosine/dot/euclid`（系统会归一化到 Qdrant 要求格式）

## 9. 扩展练习（建议按顺序）
- 增加新的 memory_type（例如 long/review），并让检索时做多路召回
- 给 RAG 增加“去重与预算裁剪”策略（按 chunk hash 去重，按字符预算截断）
- 为 InterviewPipeline 增加新的阶段（例如 Tool-use：DB 查询/知识库检索）
- 把评估样本集做成可扩展的 jsonl，并加入简单的对比报告输出

