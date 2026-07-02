#pragma once
#include "thread_local_arena.h"
#include <vector>
#include <omp.h>

class ArenaPool {
private:
    std::vector<ThreadLocalArena*> pool;
    omp_lock_t lock;
    size_t default_capacity;

public:
    explicit ArenaPool(size_t initial_count = 64, size_t default_cap = 50000) 
        : default_capacity(default_cap) {
        omp_init_lock(&lock);
        for (size_t i = 0; i < initial_count; ++i) {
            pool.push_back(new ThreadLocalArena(default_capacity));
        }
    }

    ~ArenaPool() {
        for (auto* arena : pool) {
            delete arena;
        }
        omp_destroy_lock(&lock);
    }

    ThreadLocalArena* acquire() {
        ThreadLocalArena* arena = nullptr;
        omp_set_lock(&lock);
        if (!pool.empty()) {
            arena = pool.back();
            pool.pop_back();
        }
        omp_unset_lock(&lock);

        if (!arena) {
            arena = new ThreadLocalArena(default_capacity);
        }
        return arena;
    }

    void release(ThreadLocalArena* arena) {
        omp_set_lock(&lock);
        pool.push_back(arena);
        omp_unset_lock(&lock);
    }
};

class ScopedArena {
private:
    ArenaPool& pool;
    ThreadLocalArena* arena;
public:
    ScopedArena(ArenaPool& p) : pool(p), arena(p.acquire()) {}
    ~ScopedArena() { pool.release(arena); }
    ThreadLocalArena& get() { return *arena; }
};
