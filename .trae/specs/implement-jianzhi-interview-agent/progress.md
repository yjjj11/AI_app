# 进程笔记（渐知面试官实现）

## 2026-03-21

### 已完成
- 配置：新增 AgentConfig（env > agent.config > default），启动时输出缺失提示（不打印明文密钥）。
- Embedding：实现 OpenAI 兼容 `/v1/embeddings` 客户端与工厂（优先复用 DashScope/百炼同源 key），新增 embedding_smoketest 验证。
- Qdrant：实现 QdrantClient（health/ensureCollection/upsert/search）与 payload index 初始化，新增 qdrant_smoketest 验证。
- SSE 稳定性验证：接入 DebugRouter（/debug/sse/hold），新增 sse_hold_smoketest 用于单连接持续推送验证。

### 调试记录
- EmbeddingFactory：最初返回 `Profile*` 指向临时 vector 元素导致悬空指针，表现为 embedding URL 字符串乱码与请求失败；已改为按值复制 profile。
- Qdrant：
  - `/healthz` 返回非 JSON 文本导致解析失败；已改为 health 走 raw 响应。
  - 创建 collection 需要 distance 使用 `Cosine/Dot/Euclid` 格式；已做 normalize。
  - 过滤检索需要对 payload 字段建索引；已在 ensureCollection 阶段创建 `session_id/memory_type/note_id` 的 keyword index。
- SSE Debug：
  - 多次启动出现 `bind: Address already in use`；改用高位端口（如 19085）启动做验证。
  - `/debug/sse/hold?seconds=5` 始终按 60 秒推送；发现 HttpRequestPtr + writer handler 下 query 参数未进入 GetInt/GetParam，改为从 FullPath 手动解析 query。
  - sse_hold_smoketest 遇到 “done 已收但连接被主动停止导致 res==null” 的情况；以 done+计数作为通过条件，避免误报失败。
- RAG 评估：
  - 新增 rag_hit_rate_eval（读取 jsonl 标注样本，按 expect_contains 计算 topK 命中率；支持 base/cand 对比）。
  - 本地样本集验证：samples=3，hit_rate_base=0.666667，hit_rate_cand=0.666667（依赖 qdrant_smoketest 预置 smoke session 的向量点）。
- 全链路联调：
  - HTTP：验证 /health、/login、/chat/session/new、/chat/sessions、/interview/jd/parse、/interview/turn（SSE）、/interview/review 跑通。
  - /chat/send 初始 activeKey 指向本地 mock 时会失败；通过 `POST /llm/active` 切换到 `default` 后 SSE 正常返回 delta/done。
  - memory_index_smoketest 初次出现 Qdrant 400（point id 为负数不合法）；已将 point id 约束为非负 uint64，并复跑验证 qdrant upsert/search 正常。
