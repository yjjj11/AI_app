# Contributing

## 开发环境
- CMake >= 3.10
- C++20 编译器
- OpenSSL
- MySQL client（用于链接与本地开发；若只做最小运行可参考 CI 配置）

## 本地构建
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

## 运行与自检
最小验证（不依赖外部 LLM/Qdrant/Embedding）：
```bash
ENABLE_DEBUG_ROUTES=1 HTTP_PORT=19091 ./bin/app
HTTP_BASE_URL=http://127.0.0.1:19091 SSE_SECONDS=2 ./bin/sse_hold_smoketest
```

依赖外部服务的验证（需要配置密钥与地址）：
```bash
./bin/embedding_smoketest
./bin/qdrant_smoketest
./bin/memory_index_smoketest
./bin/rag_hit_rate_eval
```

## 提交前检查建议
- 构建通过：`cmake --build build -j`
- 关键自检通过：`./bin/llm_factory_smoketest` + SSE hold
- 不提交任何密钥：确认 `config/*.env`、`config/agent.config`、`config/llm_profiles.json` 未被加入版本控制

