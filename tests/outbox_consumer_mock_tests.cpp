#include "server/outbox/OutboxConsumer.h"

#include <chrono>
#include <memory>
#include <thread>

using namespace hospital::server::outbox;

int main()
{
    auto store = std::make_shared<MockOutboxStore>();
    const auto id = store->append(R"({"scheduleId":"123","userId":"user01","requestId":"req-test"})");

    OutboxConsumer consumer(store, 50);
    consumer.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    consumer.stop();

    return store->statusOf(id) == OutboxStatus::Processed ? 0 : 1;
}
