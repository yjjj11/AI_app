# Tasks

- [x] Task 1: 开源前敏感信息治理与配置模板化
  - [x] 盘点并清理仓库内可用的明文密钥/Token（LLM/Qdrant/Embedding）
  - [x] 提供模板文件与配置说明（示例：`*.template` / `.env.example`），并添加合理的 `.gitignore` 规则
  - [x] 启动时缺失配置提示保持可诊断且不泄露敏感信息

- [x] Task 2: 编写“学习手册”（一步一步读懂系统）
  - [x] 手册结构：从运行→接口→数据模型→SSE→异步写库→记忆索引→RAG→面试业务→扩展点
  - [x] 每章包含：目标、关键文件入口、数据流图（文本/ASCII）、可运行的验证步骤
  - [x] 增补“常见错误与排障”章节，覆盖 MySQL、LLM、Qdrant、Embedding、端口占用、SSE 断流等

- [x] Task 3: 安全审计与加固（最小但有效）
  - [x] 鉴权与中间件：默认配置安全姿势、公开接口边界、CORS 策略与敏感头处理
  - [x] 输入校验：请求体大小、字段合法性、SSE 输出注入/转义与错误收敛
  - [x] 外部请求：超时、重试、错误码分层、避免日志泄露敏感信息

- [x] Task 4: 缺陷修复与冗余代码清理
  - [x] 修复已发现的可触发错误（含边界条件、类型/解析问题）
  - [x] 移除未使用变量与重复逻辑，统一配置与错误处理路径
  - [x] 保持行为不变或以 **BREAKING** 形式明确说明

- [x] Task 5: 增强验证与 CI（开源可回归）
  - [x] 补齐端到端验证脚本（本地可跑）：启动服务→登录→会话→/chat/send SSE→/interview/turn SSE→复盘→记忆索引
  - [x] 添加 GitHub Actions：构建 + 最小 smoketest（可用环境变量控制跳过外部依赖）
  - [x] 确保 CI 输出对读者友好（失败原因可定位）

- [x] Task 6: README 与仓库元信息完善
  - [x] README 调整为开源导向：项目定位、能力清单、快速开始、目录导航、学习手册入口
  - [x] 增加贡献指南（Contributing）、安全说明（Security）、许可证（License）与版本策略（可选）

# Task Dependencies
- Task 2 depends on Task 1
- Task 3 depends on Task 1
- Task 4 depends on Task 3
- Task 5 depends on Task 4
- Task 6 depends on Task 2
