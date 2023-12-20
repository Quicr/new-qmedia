/*
 *  counter_service.cpp
 *
 *  Copyright (C) 2023
 *  Cisco Systems, Inc.
 *  All Rights Reserved
 *
 *  Description:
 *      This module implements code to provide a Redis-enabled counter service
 *      to allow MLS clients to update the epoch in a conference.
 *
 *  Portability Issues:
 *      None.
 */

#include <sstream>
#include <iomanip>
#include <qmedia/counter_service.h>
#include <mls/common.h>

namespace counter
{

//
// `update_epoch(epoch_key; next_epoch)`
//
// This script is used to check that the epoch value at the Redis server
// matches the expected value and, if so, increment it by one.
//
// If the epoch value does not match, an error is returned.
//
// Redis guarantees scripts will be executed serially, thus no locks are
// required.
//
static const std::string Update_Epoch_Script = R"(
local next_epoch = 0

if redis.call("EXISTS", KEYS[1]) == 1 then
  next_epoch = redis.call("GET", KEYS[1]) + 1
end

if (ARGV[1] + 0) ~= next_epoch then
  return redis.error_reply("ERR out of sync")
end

if not redis.call("SET", KEYS[1], next_epoch) then
  return redis.error_reply("ERR failed to set epoch")
end

return "OK"
)";

/*
 *  MakeCounterID()
 *
 *  Description:
 *      This function will create a CounterID value from the given MLS
 *      group identifier.
 *
 *  Parameters:
 *      group_id [in]
 *          The MLS group identifier.
 *
 *  Returns:
 *      A CounterID holding a serialized representation of the MLS GroupID.
 *
 *  Comments:
 *      None.
 */
static CounterID MakeCounterID(GroupID group_id)
{
  return mls::tls::marshal(group_id);
}

/*
 *  MakeEpochKeyName()
 *
 *  Description:
 *      Create an key name for use with the Redis server using the
 *      provided CounterID and specified name extension.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
static std::string MakeRedisKeyName(const CounterID &counter_id,
                                    const std::string &extension)
{
    // Construct the epoch key name used with the Redis server
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto c : counter_id)
    {
        oss << std::setw(2) << static_cast<unsigned>(c);
    }
    return oss.str() + "_" + extension;
}

/*
 *  Redis::Redis()
 *
 *  Description:
 *      This is the constructor for the CounterService.
 *
 *  Parameters:
 *      group_id [in]
 *          The MLS group identifier associated with this counter.
 *
 *      host [in]
 *          The host name or IP address of the Redis server.
 *
 *      port [in]
 *          Port number of the Redis server.
 *
 *      user [in]
 *          User identifier when authenticating.  Defaults to "default".
 *
 *      password [in]
 *          Password to use when authenticating.  Default is the empty string.
 *
 *      use_tls [in]
 *          Use TLS?  Default is true.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
Redis::Redis(const GroupID group_id,
             const std::string& host,
             const std::uint16_t port,
             const std::string& user,
             const std::string& password,
             const bool use_tls) :
    Service(),
    counter_id{MakeCounterID(group_id)},
    epoch_key_name{MakeRedisKeyName(counter_id, "epoch")}
{
    // Specify the connection options
    sw::redis::ConnectionOptions options;
    options.host = host;
    options.port = port;
    options.user = user;
    options.password = password;
    options.tls.enabled = use_tls;

    // Create the Redis object
    try
    {
        redis = std::make_unique<sw::redis::Redis>(options);
    }
    catch(const sw::redis::Error &e)
    {
        throw CounterException(std::string("Redis failure: ") + e.what());
    }
}

/*
 *  Redis::IsConnected()
 *
 *  Description:
 *      Returns true if the CounterService is connected to the server.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      True if connected, false if not.
 *
 *  Comments:
 *      None.
 */
bool Redis::IsConnected()
{
    try
    {
        // Send a ping request to the server
        std::string result = redis->ping();

        return result == std::string("PONG");
    }
    catch(const sw::redis::Error &)
    {
        return false;
    }
}

/*
 *  Redis::UpdateEpoch()
 *
 *  Description:
 *      Attempt to update the epoch to the given value.  If the epoch has been
 *      updated or is being updated, an appropriate error response will be
 *      returned.  A client should only proceed with updating the epoch in
 *      a conference if this call succeeds.
 *
 *  Parameters:
 *      next_epoch [in]
 *          The expected next_epoch value.  The initial next_epoch value is
 *          zero.
 *
 *  Returns:
 *      The Result of the request to update the epoch.
 *
 *  Comments:
 *      None.
 */
Result Redis::UpdateEpoch(Counter next_epoch)
{
    try
    {
        // Execute the remote script given the keys and arguments
        std::string result = redis->eval<std::string>(
            Update_Epoch_Script,
            {epoch_key_name},
            {std::to_string(next_epoch)});

        return Result::Success;
    }
    catch(const sw::redis::Error &e)
    {
        if (e.what() == std::string("ERR out of sync"))
        {
            return Result::OutOfSync;
        }

        return Result::Failure;
    }
}

}; // namespace counter
