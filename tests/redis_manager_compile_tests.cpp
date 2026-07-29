#include "server/RedisManager.h"

using namespace hospital::server;

int main()
{
    static_assert(static_cast<int>(DeductStockResult::Success) == 1);
    static_assert(static_cast<int>(DeductStockResult::DuplicateRequest) == 0);
    static_assert(static_cast<int>(DeductStockResult::InsufficientStock) == -1);
    static_assert(static_cast<int>(DeductStockResult::InvalidQuantity) == -2);
    static_assert(static_cast<int>(DeductStockResult::StockKeyMissing) == -3);

    auto& manager = RedisManager::instance();
    return &manager == nullptr ? 1 : 0;
}
