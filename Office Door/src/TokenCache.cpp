#include "TokenCache.h"
#include <Arduino.h>
#include <Web3.h>
#include <Util.h>

TokenCache::TokenCache(uint8_t size)
{
    maxSize = size;
}
    
bool TokenCache::CheckToken(string address)
{
    uint8_t userAddressBytes[ETHERS_ADDRESS_LENGTH];
    Util::ConvertHexToBytes(userAddressBytes, address.c_str(), ETHERS_ADDRESS_LENGTH);
    //first check the cache (TODO: Revocation!)
    Serial.println(Util::ConvertBytesToHex(userAddressBytes, ETHERS_ADDRESS_LENGTH).c_str());
    for (auto itr = tokenCache.begin(); itr != tokenCache.end(); itr++)
    {
        uint8_t *addrCheck = (*itr)->address;
        Serial.println(Util::ConvertBytesToHex(addrCheck, ETHERS_ADDRESS_LENGTH).c_str());
        if (memcmp(addrCheck, userAddressBytes, ETHERS_ADDRESS_LENGTH) == 0)
        {
            (*itr)->usageTime = millis();
            return true;
        }
    }

    return false;
}
    
void TokenCache::AddToken(string address)
{
    uint8_t userAddressBytes[ETHERS_ADDRESS_LENGTH];
    Util::ConvertHexToBytes(userAddressBytes, address.c_str(), ETHERS_ADDRESS_LENGTH);

    if (tokenCache.size() < (size_t)this->maxSize)
    {
        TokenItem *item = new TokenItem();
        memcpy(item->address, userAddressBytes, ETHERS_ADDRESS_LENGTH);
        item->usageTime = millis();
        tokenCache.push_back(item);
    }
    else
    {
        //prune address cache, remove oldest item
        //find oldest
        TokenItem *oldest = tokenCache[0];
        auto itr = tokenCache.begin();
        for (itr++; itr != tokenCache.end(); itr++) if ((*itr)->usageTime < oldest->usageTime) oldest = *itr;

        //replace
        memcpy(oldest->address, userAddressBytes, ETHERS_ADDRESS_LENGTH);
        oldest->usageTime = millis();
    }
}
