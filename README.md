# my_ai_app

一个基于 libhv 的极简 Web 服务，提供：

- 登录鉴权（jwt-cpp，Cookie 方式）
- AI 对话接口（/chat/send）
- 会话管理（创建/删除/列表）
- 聊天记录异步落库（MySQL + ormpp，阻塞队列 + 后台线程写入）

## 依赖

- CMake >= 3.10
- C++20 编译器
- OpenSSL（用于 HTTPS 请求）
- MySQL client（`libmysqlclient` 以及头文件）

## 配置

项目启动时会读取：

- `/root/my_ai_app/config/mysql.env`
- `/root/my_ai_app/config/ai.env`

参考模板：

- [mysql.env.template](file:///root/my_ai_app/config/mysql.env.template)
- [ai.env.template](file:///root/my_ai_app/config/ai.env.template)

## 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

默认会从 `third_party/libhv` 构建 libhv，并启用 OpenSSL（支持 HTTPS）。如需改用系统 libhv：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DUSE_BUNDLED_LIBHV=OFF
cmake --build build -j
```

编译产物：

- `bin/app`

根目录 `compile_commands.json` 会自动软链接到 `build/compile_commands.json`。

## 运行

```bash
./bin/app
```

默认监听：

- http://127.0.0.1:8888

## 登录

当前登录校验是固定账号：

- username: `admin`
- password: `123456`

登录成功会在 Cookie 写入 `token`。

## 路由一览

- GET `/login`：登录页
- POST `/login`：登录接口
- GET `/`：AI 页面（需要登录）
- GET `/ai.html`：AI 页面（需要登录）
- GET `/health`：健康检查
- POST `/chat/send`：发送消息（需要登录）
  - JSON: `sessionId`, `question`, `modelType`
- POST `/chat/session/new`：创建会话（需要登录）
  - JSON: `title`
- POST `/chat/session/delete`：删除会话（需要登录）
  - JSON: `sessionId`
- GET `/chat/sessions?limit=50`：会话列表（需要登录）
- POST `/chat/history`：拉取历史（需要登录）
  - JSON: `sessionId`, `limit`, `offset`

## 数据库表

ormpp 通过结构体映射表：

- `chat_messages` 对应 `chat_message_row`
- `chat_sessions` 对应 `chat_session_row`

字段包含：

- `chat_messages`：`username`, `role`, `model`, `content`, `created_at_ms`, `session_id`
- `chat_sessions`：`username`, `session_id`, `title`, `created_at_ms`

服务启动时会初始化连接池并尝试创建表（如果表已存在则保持不变）。

## 异步写入机制

- `/chat/send` 负责把用户问题与 AI 回复各写入一条 `chat_message_row` 到阻塞队列
- `ChatMySqlStore` 内部后台线程从队列取出并写入 MySQL

## 已知说明

- ormpp 头文件存在链接层面的重复符号问题，当前通过链接参数允许重复定义以保证编译通过。
