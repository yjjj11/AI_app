#include "agentic/RagRetriever.h"
#include "ai/LLMFactory.h"
#include "config/AgentConfig.h"
#include "config/Dotenv.h"

#include <fstream>
#include <iostream>
#include <string>

static std::string getenvOr(const char* key, const std::string& defval) {
    const char* v = std::getenv(key);
    if (!v || !*v) return defval;
    return std::string(v);
}

static int getenvIntOr(const char* key, int defval) {
    const char* v = std::getenv(key);
    if (!v || !*v) return defval;
    return std::atoi(v);
}

static double getenvDoubleOr(const char* key, double defval) {
    const char* v = std::getenv(key);
    if (!v || !*v) return defval;
    return std::atof(v);
}

static bool hitAny(const std::vector<std::string>& chunks, const std::vector<std::string>& expects) {
    if (expects.empty()) return false;
    for (const auto& c : chunks) {
        for (const auto& e : expects) {
            if (e.empty()) continue;
            if (c.find(e) != std::string::npos) return true;
        }
    }
    return false;
}

static bool readSamples(const std::string& path, std::vector<hv::Json>& out, std::string& err) {
    std::ifstream in(path);
    if (!in.is_open()) {
        err = "open failed: " + path;
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto j = hv::Json::parse(line, nullptr, false);
        if (j.is_discarded() || !j.is_object()) continue;
        out.push_back(std::move(j));
    }
    if (out.empty()) {
        err = "no samples parsed";
        return false;
    }
    return true;
}

static int evalOnce(RagRetriever& rr, const std::vector<hv::Json>& samples, int topk, int& out_hit) {
    out_hit = 0;
    int n = 0;
    for (const auto& s : samples) {
        if (!s.contains("query") || !s["query"].is_string()) continue;
        const std::string query = s["query"].get<std::string>();
        const std::string session_id = (s.contains("session_id") && s["session_id"].is_string()) ? s["session_id"].get<std::string>() : "smoke";
        const std::string memory_type = (s.contains("memory_type") && s["memory_type"].is_string()) ? s["memory_type"].get<std::string>() : "short";
        std::vector<std::string> expects;
        if (s.contains("expect_contains") && s["expect_contains"].is_array()) {
            for (const auto& it : s["expect_contains"]) {
                if (it.is_string()) expects.push_back(it.get<std::string>());
            }
        }
        const int tk = (s.contains("topk") && s["topk"].is_number_integer()) ? s["topk"].get<int>() : topk;
        const auto chunks = rr.retrieve(session_id, query, memory_type, tk);
        const bool hit = hitAny(chunks, expects);
        if (hit) out_hit++;
        n++;
    }
    return n;
}

int main(int argc, char** argv) {
    Dotenv::loadFromFile("/root/my_ai_app/config/ai.env");
    if (!Dotenv::loadFromFile("/root/my_ai_app/config/agent.config")) {
        Dotenv::loadFromFile("/root/my_ai_app/config/agent.config.template");
    }
    if (!LLMFactory::loadProfilesFromFile("/root/my_ai_app/config/llm_profiles.json")) {
        LLMFactory::loadProfilesFromFile("/root/my_ai_app/config/llm_profiles.json.template");
    }

    AgentConfig cfg = AgentConfig::loadFromEnv();
    if (cfg.qdrant_url.empty() || cfg.qdrant_collection.empty()) {
        std::cout << "eval_ok=false\n";
        std::cout << "error=qdrant not configured\n";
        std::cout << "skipped=true\n";
        return 0;
    }

    const std::string dataset = (argc >= 2) ? std::string(argv[1]) : getenvOr("RAG_EVAL_DATASET", "/root/my_ai_app/test/rag_eval_samples.jsonl");
    const int base_topk = (argc >= 3) ? std::atoi(argv[2]) : getenvIntOr("RAG_EVAL_BASE_TOPK", 5);
    const int cand_topk = (argc >= 4) ? std::atoi(argv[3]) : getenvIntOr("RAG_EVAL_CAND_TOPK", 8);
    const double min_hit_rate = getenvDoubleOr("RAG_EVAL_MIN_HIT_RATE", -1.0);

    std::vector<hv::Json> samples;
    std::string err;
    if (!readSamples(dataset, samples, err)) {
        std::cout << "eval_ok=false\n";
        std::cout << "error=" << err << "\n";
        return 3;
    }

    RagRetriever rr(cfg);
    int hit_base = 0;
    const int n = evalOnce(rr, samples, base_topk, hit_base);
    const double rate_base = n > 0 ? (double)hit_base / (double)n : 0.0;

    int hit_cand = 0;
    const int n2 = evalOnce(rr, samples, cand_topk, hit_cand);
    const double rate_cand = n2 > 0 ? (double)hit_cand / (double)n2 : 0.0;

    std::cout << "eval_ok=true\n";
    std::cout << "dataset=" << dataset << "\n";
    std::cout << "samples=" << n << "\n";
    std::cout << "base_topk=" << base_topk << "\n";
    std::cout << "hit_base=" << hit_base << "\n";
    std::cout << "hit_rate_base=" << rate_base << "\n";
    std::cout << "cand_topk=" << cand_topk << "\n";
    std::cout << "hit_cand=" << hit_cand << "\n";
    std::cout << "hit_rate_cand=" << rate_cand << "\n";
    std::cout << "delta_hit_rate=" << (rate_cand - rate_base) << "\n";

    if (min_hit_rate >= 0.0 && rate_cand < min_hit_rate) {
        std::cout << "threshold_ok=false\n";
        return 4;
    }
    std::cout << "threshold_ok=true\n";
    return 0;
}
