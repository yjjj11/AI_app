# Tasks

- [x] Task 1: 增加 Agent/向量检索配置入口
  - [ ] 定义 `config/agent.config` 的配置项（Qdrant、Embedding、阈值、topK、超时）
  - [ ] 实现配置读取优先级：env 覆盖 > agent.config > 默认值
  - [ ] 在日志/启动检查中输出“缺少配置项”提示（不打印密钥明文）

- [ ] Task 2: 实现云端 Embedding 客户端（OpenAI 兼容）
  - [ ] 新增 EmbeddingClient：支持 `/v1/embeddings` 批量输入、dimensions 可选
  - [ ] 默认模型策略：优先复用百炼/DashScope 同源 key（如可用）；否则使用独立 embedding key
  - [ ] 失败降级：embedding 超时/失败时返回可诊断错误码与重试建议

- [ ] Task 3: 实现 Qdrant Cloud 客户端与 Collection 初始化
  - [ ] 新增 QdrantClient：upsert(points batch)、search(topK, filter)、health/ping
  - [ ] 支持 `api-key` 鉴权与超时配置
  - [ ] 提供 collection 初始化/校验（vector_size 与 distance 一致）

- [ ] Task 4: 新增结构化笔记与分块存储（MySQL）及异步索引流水线
  - [ ] 新增 MySQL 表：notes、note_chunks（含 session_id/memory_type/tags 等）
  - [ ] 新增异步 worker：notes 生成/分块 → embedding → qdrant upsert
  - [ ] 触发条件：会话历史消息数 > 100（可配置）
  - [ ] 去重策略：chunk 内容 hash + note_id/chunk_index 稳定 id

- [ ] Task 5: 实现“渐知面试官”业务 API（最小闭环）
  - [ ] JD 解析：提取关键词/要求/考点（结构化输出）
  - [ ] 题单生成：根据岗位方向与候选人材料生成题单
  - [ ] 模拟面试：多轮追问（SSE），记录回答并输出评分与改进建议
  - [ ] 复盘错题本：生成结构化复盘笔记并写入索引流水线

- [ ] Task 6: 实现 Agent Pipeline（可插拔阶段）与检索增强注入
  - [ ] Pipeline：Plan/Retrieve/Compose/Generate/Reflect 的接口与默认实现
  - [ ] Retrieve：Qdrant 按 session_id + memory_type 过滤 topK
  - [ ] Compose：上下文预算（字符/条数/段落上限）与拼接模板
  - [ ] Generate：对接现有 LLM（stream_chat_messages）

- [ ] Task 7: 指标采集与评估脚本
  - [ ] SSE：记录单连接持续时长与断流原因（日志/指标）
  - [ ] RAG：提供“命中率”评估口径（人工标注集/AB 配置）
  - [ ] 提供最小可运行验证脚本与 smoketest

- [ ] Task 8: 文档与部署说明更新
  - [ ] README 增加面试业务接口与 agent.config 配置说明
  - [ ] 提醒用户配置 QDRANT_API_KEY 与 embedding 配置（如需额外 key）

# Task Dependencies
- Task 2 depends on Task 1
- Task 3 depends on Task 1
- Task 4 depends on Task 2 and Task 3
- Task 6 depends on Task 2 and Task 3
- Task 5 depends on Task 6
- Task 7 depends on Task 5
- Task 8 depends on Task 1
