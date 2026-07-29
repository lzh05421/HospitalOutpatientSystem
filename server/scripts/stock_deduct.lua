-- Expert registration quota pre-deduction script.
--
-- Inputs:
--   KEYS[1] = stock key, for example schedule:123:v5:remain
--   ARGV[1] = request id / idempotency id
--   ARGV[2] = quantity to deduct
--
-- Return codes:
--    1 = success, stock was deducted and request id was recorded
--    0 = duplicate request, stock was not deducted again
--   -1 = insufficient stock
--   -2 = invalid quantity
--   -3 = stock key does not exist

local stockKey = KEYS[1]
local requestId = ARGV[1]
local quantity = tonumber(ARGV[2])

if quantity == nil or quantity <= 0 then
    return -2
end

local idempotencyHashKey = stockKey .. ':deduct:requests'
local previousResult = redis.call('HGET', idempotencyHashKey, requestId)
if previousResult ~= false then
    return 0
end

local stock = tonumber(redis.call('GET', stockKey))
if stock == nil then
    return -3
end

if stock < quantity then
    return -1
end

redis.call('DECRBY', stockKey, quantity)
redis.call('HSET', idempotencyHashKey, requestId, '1')
redis.call('EXPIRE', idempotencyHashKey, 86400)

return 1
