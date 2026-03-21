# Security

## 不要提交密钥
本仓库默认通过 `.gitignore` 忽略以下文件，避免误提交：
- `config/*.env`
- `config/agent.config`
- `config/llm_profiles.json`

请使用模板文件作为起点：
- `config/mysql.env.template`
- `config/ai.env.template`
- `config/agent.config.template`
- `config/llm_profiles.json.template`

## 默认配置提示
- 若未设置 `JWT_SECRET`，服务会使用 `dev_secret` 并输出告警；生产环境请务必设置强随机值。
- 调试路由默认关闭，需显式设置 `ENABLE_DEBUG_ROUTES=1` 才会注册并放行。

## 漏洞上报
如发现安全问题，请通过私下渠道告知维护者，并在修复发布后再公开讨论细节。

