#ifndef TOKEN_CACHE_H
#define TOKEN_CACHE_H

#include <vector>
#include <string>

using namespace std;

class TokenItem
{
public:
    uint8_t address[20];
    unsigned long usageTime;
};

class TokenCache
{
public:
    TokenCache(uint8_t size);
    bool CheckToken(string address);
    void AddToken(string address);

private:
    uint8_t maxSize;
    vector<TokenItem*> tokenCache;
};

#endif