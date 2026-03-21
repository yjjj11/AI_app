# 项目经历：渐知面试官（C++ / Agent / RAG）

## 项目简介
基于 C++20 的后端服务，提供登录鉴权、对话与面试业务的 HTTP API，并通过 SSE 实现流式输出；在此基础上实现了可插拔的 Agent 流水线（Plan/Retrieve/Compose/Generate/Reflect），以及“结构化记忆索引 + 向量检索增强”（Embedding + Qdrant）能力，用于在模拟面试与复盘中形成可复用的会话记忆与检索上下文。

## 个人工作（偏 Agent/检索增强）
- 设计并落地面试 Agent 流水线：将一次面试回合拆分为 Plan（模式/方向/是否有回答）、Retrieve（按会话过滤召回记忆）、Compose（上下文模板与预算控制）、Generate（对接 LLM 流式生成）、Reflect（预留反思输出）五个阶段，并提供默认实现，便于后续扩展与替换。
- 实现 RAG 检索增强：将用户输入向量化后在 Qdrant 中按 `session_id + memory_type` 过滤检索 topK chunk，并在 Compose 阶段按字符预算注入可控上下文，降低无关内容对生成的干扰。
- 实现结构化记忆索引流水线：当会话历史超过阈值时异步触发，将对话尾部内容汇总为“严格 JSON”结构化笔记（facts/preferences/todos/weaknesses/conclusions 等），再进行渲染与分块、批量 embedding，并写入 Qdrant 作为可检索的记忆片段。
- 落地异步写库与索引后台任务：通过队列 + worker 的方式异步写入 MySQL（会话/消息/笔记/分块），避免流式输出请求被数据库写入拖慢；记忆索引同样以后台线程执行，减少请求线程阻塞。
- 完善配置与开源安全姿势：支持配置文件与环境变量覆盖的读取方式，提供模板与忽略规则，避免明文密钥进入仓库；默认关闭调试路由，显式开启后才暴露用于 SSE 稳定性验证的接口；对缺失关键配置输出可诊断提示但不打印密钥明文。
- 提供可回归验证：补齐 smoketest/脚本与 CI 最小验证路径，保证在缺少外部依赖（LLM/Qdrant/Embedding）时也能进行基础构建与核心行为回归。

## 关键实现要点
- SSE 流式协议：对话与面试回合均以 `delta/error/done` 事件输出，实现“边生成边返回”的用户体验。
- Qdrant 数据建模：向量点 payload 中携带会话与记忆类型等过滤字段，确保召回可按 session 维度隔离，并保留 note/chunk 关联信息用于追溯与去重。
- 记忆内容结构化：优先要求模型输出严格 JSON；若返回不可解析内容则进行降级保存，保证流水线健壮性。
- 上下文预算控制：检索结果注入前进行长度裁剪，避免 prompt 过长导致成本与效果波动。

## 可验证方式（面试可讲）
- 基础启动与健康检查：`/health`
- SSE 长连接稳定性验证：`/debug/sse/hold` + `sse_hold_smoketest`
- Qdrant/Embedding 可用性验证：`embedding_smoketest`、`qdrant_smoketest`
- 记忆索引流水线验证：`memory_index_smoketest`
- 最小端到端验证脚本：`scripts/e2e_smoke.sh`、`scripts/e2e_full.sh`

