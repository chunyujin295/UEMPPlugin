/*************************************************
  * 描述：8位多线程安全strId生成工具
  *
  * File：id8generator.h
  * Author：chenyujin@mozihealthcare.cn
  * Date：2025/10/31
  * Update：
  * ************************************************/
#ifndef COREXI_COMMON_PC_ID8GENERATOR_H
#define COREXI_COMMON_PC_ID8GENERATOR_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <string>
#include <thread>

#if defined(_WIN32) || defined(_WIN64)
#include <process.h>

static inline uint32_t get_pid() noexcept
{
    return static_cast<uint32_t>(_getpid());
}
#else
#include <unistd.h>
static inline uint32_t get_pid() noexcept
{
    return static_cast<uint32_t>(getpid());
}
#endif

class ID8Generator
{
    private:
        std::atomic<uint64_t> counter_;

        static uint64_t mix64(uint64_t z) noexcept
        {
            z += 0x9e3779b97f4a7c15ULL;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            return z ^ (z >> 31);
        }

        static constexpr uint64_t pow62(uint64_t n) noexcept
        {
            uint64_t r = 1;
            for (uint64_t i = 0; i < n; ++i) r *= 62ULL;
            return r;
        }

        static constexpr char base62_char(uint64_t i) noexcept
        {
            return (i < 10)
                       ? ('0' + i)
                       : (i < 36)
                             ? ('A' + (i - 10))
                             : ('a' + (i - 36));
        }

    public:
        ID8Generator() noexcept
        {
            std::random_device rd;
            uint64_t seed = (static_cast<uint64_t>(rd()) << 32) ^ rd();
            seed ^= static_cast<uint64_t>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count()
            );
            seed ^= (static_cast<uint64_t>(get_pid()) << 16);
            counter_.store(seed ? seed : 1ULL, std::memory_order_relaxed);
        }

        // 生成一个8字符的Base62唯一ID，线程安全
        std::string operator()() noexcept
        {
            uint64_t c = counter_.fetch_add(1, std::memory_order_relaxed);
            uint64_t t = static_cast<uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count()
            );
            uint64_t pid = static_cast<uint64_t>(get_pid());

            uint64_t mix = mix64(c ^ t ^ (pid << 32));

            constexpr uint64_t BASE = 62ULL;
            constexpr uint64_t LEN = 8ULL;
            constexpr uint64_t MAXVAL = pow62(LEN);
            uint64_t val = mix % MAXVAL;

            std::string s;
            s.resize(static_cast<size_t>(LEN));
            for (int i = static_cast<int>(LEN) - 1; i >= 0; --i)
            {
                s[i] = base62_char(val % BASE);
                val /= BASE;
            }
            return s;
        }
};

#endif//COREXI_COMMON_PC_ID8GENERATOR_H
