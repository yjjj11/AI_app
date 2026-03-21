# 渐知面试官（Interview Copilot）实现 Spec

## Why
当前项目以“聊天服务”为主，缺少可落地的业务闭环与 Agentic RAG/记忆能力的真实实现路径。本变更将补齐“模拟面试 + 追问陪练 + 复盘错题本 + 记忆增强”的可实现方案，使简历描述对应到可运行系统。

## What Changes
- 新增“面试训练业务闭环”后端能力：JD 解析、题单生成、多轮追问、评分、复盘、训练计划。
- 新增 Agent Pipeline 编排：Plan → Retrieve → Compose → Generate → Reflect（阶段可插拔）。
- 新增结构化笔记（短期/长期记忆）生成与分块（chunk）规范。
- 新增云端 Embedding 调用（OpenAI 兼容 `/v1/embeddings`）。
- 新增 Qdrant Cloud 向量索引：批量 upsert、按 session/memory_type 过滤检索 topK。
- 新增异步索引流水线：当会话历史超过阈值（默认 100 条）触发“结构化笔记 → 分块 → embedding → qdrant upsert”。
- 新增配置文件与密钥注入位置：`/root/my_ai_app/config/agent.config`（不写入仓库密钥）。
- 新增可观测指标：SSE 单连接持续时长统计、检索召回命中率（人工标注/AB）统计口径。

## Impact
- Affected specs: SSE 流式对话、鉴权与数据隔离、会话/历史管理、异步写库、模型配置、Agentic RAG/记忆索引。
- Affected code (计划改动范围):
  - 路由层：新增 InterviewRouter（或扩展现有 ChatRouter/SettingRouter）
  - 服务层：InterviewService、MemoryIndexService、EmbeddingClient、QdrantClient
  - 存储层：新增 notes/note_chunks 表（或在现有 chat_* 表旁新增），以及对应异步写入队列/worker
  - 配置层：新增 agent.config 读取逻辑（支持 env 覆盖）
  - 前端：新增“面试模式”入口与复盘页面（最小版本可先仅后端+接口）

## 实现路径图（高层数据流）
```
               ┌─────────────────────────────┐
User(JD/简历) → │  Interview API (HTTP/SSE)  │
               └──────────────┬──────────────┘
                              │
                              ▼
                    ┌──────────────────┐
                    │  Agent Pipeline  │
                    │ Plan/Retrieve/...│
                    └───────┬──────────┘
                            │
          ┌─────────────────┼─────────────────┐
          ▼                 ▼                 ▼
   Retrieve(Memory)   Generate(LLM)      Reflect(复盘)
          │                 │                 │
          ▼                 ▼                 ▼
    Qdrant Search      SSE delta输出     结构化笔记(JSON)
          │                                   │
          └──────────────┬────────────────────┘
                         ▼
             Index Pipeline (异步触发)
     history>100 → chunk → embeddings → Qdrant upsert
                         │
                         ▼
                    MySQL notes 表
```

## ADDED Requirements

### Requirement: 面试训练业务闭环
系统 SHALL 支持用户提交 JD/简历材料并启动模拟面试会话，完成多轮追问、评分与复盘输出。

#### Scenario: 启动会话并生成题单（Success）
- **WHEN** 用户提交 JD/简历材料并请求启动面试会话
- **THEN** 系统返回会话 id 与初始题单（结构化，包含题型/难度/覆盖点）

#### Scenario: SSE 追问与实时反馈（Success）
- **WHEN** 用户进入面试会话并回答问题
- **THEN** 系统通过 SSE 持续输出追问/提示/评分增量，并以 done 事件结束本轮

### Requirement: 结构化笔记与记忆分层
系统 SHALL 在会话历史超过阈值（默认 100 条）时生成结构化笔记（短期/长期）并分块存储为可检索记忆。

#### Scenario: 超阈值触发索引（Success）
- **WHEN** 会话历史消息数 > 100
- **THEN** 系统生成结构化复盘笔记，并触发异步索引流水线写入 MySQL 与 Qdrant

### Requirement: 云端 Embedding（OpenAI compatible）
系统 SHALL 通过 OpenAI 兼容接口 `/v1/embeddings` 生成向量，支持批量输入与维度配置。

#### 模型选择（默认策略）
- 默认 SHALL 优先使用与当前 chat 同源的百炼/DashScope 兼容接口（若可用），复用同一 `api_key`。
- 默认 SHALL 采用 `text-embedding-v4` 且 `dimensions=1024`（可通过配置覆盖）。

### Requirement: Qdrant Cloud 向量索引
系统 SHALL 支持向 Qdrant Cloud 批量 upsert 分块向量，并在查询时支持 topK 检索与过滤。

#### Scenario: 检索增强（Success）
- **WHEN** 用户发起新问题
- **THEN** 系统对 query 向量化并在 Qdrant 中按 `session_id + memory_type` 过滤检索 topK，将结果拼接注入上下文后再生成答案

### Requirement: 指标与评估口径
系统 SHALL 支持以下指标的采集与可复现实验口径：
- SSE 单连接稳定推送 N 分钟无断流（压测/长连接测试脚本）
- 检索召回命中率提升 X%（人工标注集或 AB 对比）

## MODIFIED Requirements

### Requirement: 现有对话接口（/chat/send）
系统 SHALL 保持现有 SSE 输出结构不变（delta/error/done），并允许在“面试模式”下复用流式输出机制（可新增独立路由以隔离业务）。

## REMOVED Requirements

### Requirement: 无
**Reason**: 本变更为新增能力，不移除现有功能。
**Migration**: 无。

