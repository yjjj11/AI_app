#include "service/MemoryIndexService.h"

#include "agentic/EmbeddingFactory.h"
#include "agentic/QdrantClient.h"
#include "ai/LLMFactory.h"
#include "config/AgentConfig.h"
#include "ormpp/connection_pool.hpp"
#include "service/ChatDbModels.h"
#include "service/ChatService.h"

#include <chrono>
#include <iostream>
#include <sstream>

static uint64_t fnv1a64(std::string_view s) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h ^= (uint64_t)c;
        h *= 1099511628211ULL;
    }
    return h;
}

static std::string hex64(uint64_t x) {
    std::ostringstream oss;
    oss << std::hex << x;
    return oss.str();
}

static std::vector<std::string> chunkText(const std::string& text, int max_chars) {
    std::vector<std::string> out;
    if (max_chars <= 0) max_chars = 800;
    std::string buf;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if ((int)(buf.size() + line.size() + 1) > max_chars) {
            if (!buf.empty()) out.push_back(buf);
            buf.clear();
        }
        if (!buf.empty()) buf.push_back('\n');
        buf += line;
    }
    if (!buf.empty()) out.push_back(buf);
    if (out.empty() && !text.empty()) out.push_back(text.substr(0, (size_t)max_chars));
    return out;
}

static std::string renderNoteText(const hv::Json& j) {
    auto joinArr = [&](const char* key, const char* title) -> std::string {
        if (!j.contains(key) || !j[key].is_array() || j[key].empty()) return "";
        std::string s;
        s += title;
        s += "\n";
        for (const auto& it : j[key]) {
            if (it.is_string()) {
                s += "- " + it.get<std::string>() + "\n";
            } else {
                s += "- " + it.dump() + "\n";
            }
        }
        return s;
    };
    std::string out;
    out += joinArr("facts", "FACTS");
    out += joinArr("preferences", "PREFERENCES");
    out += joinArr("todos", "TODOS");
    out += joinArr("weaknesses", "WEAKNESSES");
    out += joinArr("conclusions", "CONCLUSIONS");
    if (out.empty()) out = j.dump();
    return out;
}

static hv::Json parseJsonMaybeFenced(const std::string& s) {
    hv::Json j = hv::Json::parse(s, nullptr, false);
    if (j.is_object()) return j;
    const size_t l = s.find('{');
    const size_t r = s.rfind('}');
    if (l != std::string::npos && r != std::string::npos && r > l) {
        hv::Json jj = hv::Json::parse(s.substr(l, r - l + 1), nullptr, false);
        if (jj.is_object()) return jj;
    }
    return hv::Json();
}

static bool queryLastIndexed(Db* conn,
                             const std::string& username,
                             const std::string& session_id,
                             const std::string& memory_type,
                             int64_t& out_last_message_id) {
    try {
        auto rows = conn->query_s<note_row>(
            "username=? and session_id=? and memory_type=? order by id desc limit 1",
            username,
            session_id,
            memory_type
        );
        if (rows.empty()) return false;
        out_last_message_id = rows[0].source_last_message_id;
        return true;
    } catch (...) {
        return false;
    }
}

static int64_t queryLastInsertId(Db* conn) {
    try {
        auto rows = conn->query<std::tuple<int64_t>>("select last_insert_id()");
        if (rows.empty()) return 0;
        return (int64_t)std::get<0>(rows[0]);
    } catch (...) {
        return 0;
    }
}

MemoryIndexService& MemoryIndexService::instance() {
    static MemoryIndexService inst;
    return inst;
}

MemoryIndexService::~MemoryIndexService() {
    stop();
}

void MemoryIndexService::init() {
    bool expected = false;
    if (!inited_.compare_exchange_strong(expected, true)) return;
    stop_.store(false);
    worker_ = std::thread([this]() { workerLoop(); });
}

void MemoryIndexService::stop() {
    stop_.store(true);
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void MemoryIndexService::maybeEnqueue(const std::string& username, const std::string& session_id) {
    if (!inited_.load()) return;
    if (session_id.empty()) return;
    {
        std::lock_guard<std::mutex> lock(mu_);
        q_.emplace_back(username.empty() ? "unknown" : username, session_id);
        if (q_.size() > 256) q_.pop_front();
    }
    cv_.notify_one();
}

void MemoryIndexService::workerLoop() {
    AgentConfig cfg = AgentConfig::loadFromEnv();
    auto emb = createEmbeddingClientFromConfig(cfg);
    const bool qdrant_enabled = !cfg.qdrant_url.empty() && !cfg.qdrant_api_key.empty();
    std::unique_ptr<QdrantClient> qdrant;
    if (qdrant_enabled) {
        qdrant = std::make_unique<QdrantClient>(cfg.qdrant_url, cfg.qdrant_api_key, cfg.qdrant_timeout_seconds);
    }

    ChatService chat;
    auto& pool = ormpp::connection_pool<Db>::instance();

    while (!stop_.load()) {
        std::pair<std::string, std::string> job;
        {
            std::unique_lock<std::mutex> lock(mu_);
            cv_.wait(lock, [&]() { return stop_.load() || !q_.empty(); });
            if (stop_.load()) break;
            job = std::move(q_.front());
            q_.pop_front();
        }

        cfg = AgentConfig::loadFromEnv();
        const std::string username = job.first;
        const std::string session_id = job.second;

        const int count = chat.countMessages(username, session_id);
        if (count <= cfg.memory_trigger_messages) continue;
        const int64_t last_id = chat.maxMessageId(username, session_id);
        if (last_id <= 0) continue;

        auto conn = pool.get();
        if (!conn) continue;

        int64_t prev_last = 0;
        if (queryLastIndexed(conn.get(), username, session_id, "short", prev_last) && prev_last == last_id) {
            continue;
        }

        auto llm = LLMFactory::get("active");
        if (!llm) llm = LLMFactory::get();
        if (!llm) continue;

        const auto tail = chat.getTailHistory(username, session_id, 120);
        std::string transcript;
        transcript.reserve(4096);
        for (const auto& m : tail) {
            std::string c = m.content;
            if (c.size() > 800) c = c.substr(0, 800);
            transcript += m.role + ": " + c + "\n";
        }

        hv::Json messages = hv::Json::array();
        messages.push_back({{"role", "system"},
                            {"content",
                             "You are an interview coach agent. Summarize the conversation into STRICT JSON only.\n"
                             "Schema:\n"
                             "{\n"
                             "  \"facts\": [\"...\"],\n"
                             "  \"preferences\": [\"...\"],\n"
                             "  \"todos\": [\"...\"],\n"
                             "  \"weaknesses\": [\"...\"],\n"
                             "  \"conclusions\": [\"...\"],\n"
                             "  \"tags\": [\"...\"],\n"
                             "  \"memory_type\": \"short\"\n"
                             "}\n"
                             "Do not include markdown. Do not include any extra text."}});
        messages.push_back({{"role", "user"}, {"content", transcript}});

        const std::string raw = llm->chat_messages(messages);
        hv::Json note_json = parseJsonMaybeFenced(raw);
        if (note_json.is_discarded() || !note_json.is_object()) {
            note_json = {{"raw", raw}, {"memory_type", "short"}, {"tags", hv::Json::array()}};
        }
        if (!note_json.contains("tags") || !note_json["tags"].is_array()) note_json["tags"] = hv::Json::array();

        note_row note;
        note.username = username;
        note.session_id = session_id;
        note.memory_type = "short";
        note.title = "interview_note";
        note.content_json = note_json.dump();
        note.tags_json = note_json["tags"].dump();
        note.source_last_message_id = last_id;
        note.created_at_ms = chat_now_ms();
        conn->insert(note);
        const int64_t note_id = queryLastInsertId(conn.get());
        if (note_id <= 0) continue;

        const std::string rendered = renderNoteText(note_json);
        const auto chunks = chunkText(rendered, 800);
        if (chunks.empty()) continue;

        if (!emb) emb = createEmbeddingClientFromConfig(cfg);
        if (!emb) continue;

        if (qdrant) {
            const int vec_size = cfg.qdrant_vector_size > 0 ? cfg.qdrant_vector_size : emb->configuredDimensions();
            const auto col = qdrant->ensureCollection(cfg.qdrant_collection, vec_size, cfg.qdrant_distance);
            if (!col.ok) continue;
        }

        const int batch = 8;
        for (int i = 0; i < (int)chunks.size(); i += batch) {
            std::vector<std::string> part;
            for (int k = i; k < (int)chunks.size() && k < i + batch; k++) part.push_back(chunks[k]);
            EmbeddingResult er = emb->embedBatch(part);
            if (!er.ok || (int)er.vectors.size() != (int)part.size()) continue;

            hv::Json points = hv::Json::array();
            for (int k = 0; k < (int)part.size(); k++) {
                const int chunk_index = i + k;
                const std::string key = username + "|" + session_id + "|" + std::to_string(last_id) + "|" + std::to_string(chunk_index);
                const uint64_t pid_u = fnv1a64(key);
                const uint64_t pid = pid_u & 0x7fffffffffffffffULL;
                const std::string th = hex64(fnv1a64(part[k]));

                hv::Json payload = {
                    {"session_id", session_id},
                    {"memory_type", "short"},
                    {"note_id", note_id},
                    {"chunk_index", chunk_index},
                    {"text_hash", th},
                    {"text", part[k]}
                };
                hv::Json vec = hv::Json::array();
                for (float x : er.vectors[k]) vec.push_back(x);
                points.push_back({{"id", pid}, {"vector", vec}, {"payload", payload}});

                note_chunk_row row;
                row.note_id = note_id;
                row.chunk_index = chunk_index;
                row.text = part[k];
                row.text_hash = th;
                row.qdrant_point_id = (int64_t)pid;
                row.created_at_ms = chat_now_ms();
                conn->insert(row);
            }

            if (qdrant) {
                const auto up = qdrant->upsertPoints(cfg.qdrant_collection, points, true);
                if (!up.ok) {
                    std::cerr << "[MemoryIndexService] qdrant upsert failed: " << up.error << "\n";
                }
            }
        }
    }
}
