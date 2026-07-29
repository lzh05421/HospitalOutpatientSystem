#include <fstream>
#include <iterator>
#include <string>
#include <vector>

bool containsRequiredOutboxSchema(const char* path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    const std::string source((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
    const std::vector<std::string> requiredFragments = {
        "CREATE TABLE IF NOT EXISTS outbox_events",
        "event_id",
        "event_type",
        "aggregate_type",
        "aggregate_id",
        "business_key",
        "route_key",
        "payload",
        "headers",
        "status",
        "retry_count",
        "last_error",
        "published_at",
        "uk_outbox_event_id",
        "idx_outbox_poll"
    };

    for (const std::string& fragment : requiredFragments) {
        if (source.find(fragment) == std::string::npos) {
            return false;
        }
    }

    return true;
}

int main()
{
    if (!containsRequiredOutboxSchema("server/src/DatabaseManager.cpp")) {
        return 1;
    }

    if (!containsRequiredOutboxSchema("database/schema.sql")) {
        return 2;
    }

    return 0;
}
