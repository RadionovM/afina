#ifndef AFINA_STORAGE_STRIPED_LRU_H
#define AFINA_STORAGE_STRIPED_LRU_H

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <unistd.h>
#include <vector>

#include "ThreadSafeSimpleLRU.h"
#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # SimpleLRU thread safe version
 *
 *
 */
class StripedLRU : public Afina::Storage {
public:
    StripedLRU(std::size_t stripe_count, size_t max_size = 1024)
        : stripe_count(stripe_count) // 1024 байт?
    {
        std::size_t stripe_limit = max_size / stripe_count;
        if (stripe_limit < 200) // 200 байт ?
        {
            throw std::runtime_error("Small storage size for one stripe: " + std::to_string(stripe_limit));
        }

        for (std::size_t i = 0; i < stripe_count; ++i) {
            shards.emplace_back(new ThreadSafeSimplLRU(max_size / stripe_count));
        }
    }
    ~StripedLRU() {}

    // see SimpleLRU.h
    bool Put(const std::string &key, const std::string &value) override {
        return shards[hash(key) % stripe_count]->Put(key, value);
    }

    // see SimpleLRU.h
    bool PutIfAbsent(const std::string &key, const std::string &value) override {
        return shards[hash(key) % stripe_count]->PutIfAbsent(key, value);
    }

    // see SimpleLRU.h
    bool Set(const std::string &key, const std::string &value) override {
        return shards[hash(key) % stripe_count]->Set(key, value);
    }

    // see SimpleLRU.h
    bool Delete(const std::string &key) override { return shards[hash(key) % stripe_count]->Delete(key); }

    // see SimpleLRU.h
    bool Get(const std::string &key, std::string &value) override {
        return shards[hash(key) % stripe_count]->Get(key, value);
    }

private:
    std::size_t stripe_count;
    std::vector<std::unique_ptr<ThreadSafeSimplLRU>> shards;
    std::hash<std::string> hash;
};
} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_THREAD_SAFE_STRIPED_LRU_H
