# /plan：基于本项目生成“C++ + Agent方向简历项目经历”与常见问答

## Summary
基于仓库现有实现（不添加任何虚构指标/能力），输出两份可直接用于求职的文档：
1) “简历项目经历段落”（C++ + Agent/RAG/Memory 方向偏重）
2) “常见面试问题与回答”（每个回答都能在仓库中找到对应实现依据）

## Current State Analysis（已从仓库核实）
- 技术栈：C++20 + libhv（HTTP/SSE）+ jwt-cpp（JWT）+ MySQL（ormpp 连接池）+ Qdrant（向量库）+ OpenAI兼容 embeddings/chat 接口客户端。见 README、HttpServer、ChatMySqlStore、AgentConfig 等实现文件。
- Agent 流水线：Plan/Retrieve/Compose/Generate/Reflect 接口 + 默认实现，Retrieve 对接 RagRetriever（embedding→qdrant filter search→chunks），Generate 支持 SSE 流式输出。见 InterviewPipeline 与 InterviewPipelineDefaults。
- 记忆索引：异步 worker 触发（历史消息数阈值）→ LLM 产出结构化 JSON → 渲染/分块 → embedding 批量 → Qdrant upsert（payload 带 session_id/memory_type/note_id 等）。见 MemoryIndexService。
- 鉴权与安全基线：JWT（Cookie/Bearer），默认关闭 /debug 路由，CORS 可配置，请求体大小限制，JWT_SECRET 未设置会告警；登录 Cookie 可选 Secure。见 HttpServer、LoginRouter。
- 工程化与验证：学习手册、CI（GitHub Actions）、本地 e2e 脚本与 smoketest 已存在。见 docs/learning-handbook.md、.github/workflows/ci.yml、scripts/ 与 test/。

## 用户偏好与约束（来自本轮对话）
- 输出语言：中文
- 简历形式：只输出“项目经历段落”（可直接粘贴进简历）
- 引用方式：最终文档不带源码引用；但生成过程必须逐条对照源码，确保不写仓库不存在的功能/指标
- 关键约束：不得编造、不得幻觉（不写未实现内容，不写无证据的性能提升/用户量/准确率等）

## Proposed Changes（执行阶段将按此落地）

### 1) 生成“简历项目经历段落”（偏 Agent）
产出文件（执行阶段创建）：
- docs/resume_project_cpp_agent.md

内容结构（全部从仓库真实能力抽取）：
- 项目一句话定位（C++ 服务端 + Agent/RAG/记忆）
- 个人职责（用“实现/设计/落地/加固/验证”描述；不出现无法证实的团队规模、DAU、QPS 等）
- 关键技术点（分 4–6 组）：
  - HTTP 服务与 SSE 流式（/chat/send、/interview/turn）
  - Agent pipeline（Plan/Retrieve/Compose/Generate/Reflect）
  - RAG：embedding + Qdrant filter search + 上下文预算注入
  - 结构化记忆索引：异步触发、JSON schema、chunk、去重 id、upsert
  - 工程化：smoketest、e2e、CI、配置模板与密钥治理
  - 安全基线：JWT、CORS、调试路由默认关闭、请求体大小限制
- 难点与解决：只选取仓库确实出现并已解决的点（例如 Qdrant point id 非负、SSE hold query 解析、密钥模板化等）
- 可验证方式：列出仓库已有脚本/自检二进制，不写虚构测试覆盖率

### 2) 生成“常见问题与回答”（面试向）
产出文件（执行阶段创建）：
- docs/resume_cpp_agent_faq.md

问题范围（覆盖面试常问，回答均可在仓库中核实）：
- 架构与数据流：请求如何从路由到服务到 LLM，再到异步落库与索引
- Agent 设计：为什么拆 Plan/Retrieve/Compose/Generate/Reflect，各阶段输入输出是什么
- RAG：payload 过滤如何做 session 隔离，context budget 为什么需要，如何避免上下文污染（仅描述仓库已有策略）
- 记忆索引：触发条件、结构化 JSON、chunk 策略、去重/稳定 id、失败降级
- SSE：delta/done 协议、连接稳定性验证方式（/debug/sse/hold 与 sse_hold_smoketest）
- 配置与密钥：env > config > default 的优先级、如何避免明文密钥入仓（模板 + gitignore）
- 安全：JWT 位置（Cookie/Bearer）、/debug 默认关闭原因、CORS 取舍、body size 限制目的
- 工程化：CI 跑什么、不依赖外部服务时怎么验证

回答风格：
- 每题 5–10 行：先结论，再结合“本项目具体怎么做”解释，不引入“业界常见但项目未实现”的额外机制
- 明确“项目当前没有实现的点”（例如分布式追踪、熔断、多租户等）会直说“当前未实现”

## Assumptions & Decisions
- 不扩展新功能：本次目标是“基于现有项目写简历与 Q&A”，不做新的架构改造。
- 若发现文案可能被误读为夸大，优先调整措辞而不是改实现。
- 若发现确实存在会影响开源展示的安全/稳定性问题，会单独列出修复建议并征得确认后再执行。

## Verification（执行阶段验收）
- 建立“事实清单”：对简历与 FAQ 中每一个技术点，逐条在仓库内定位到对应实现（文件/函数/常量/脚本）。
- 禁止指标幻觉：所有数字（阈值、topK、超时等）必须来自仓库现有默认值/常量/文档。
- 验证步骤可复现：文档中提到的每个验证步骤，都能在仓库中找到对应脚本或可执行。
