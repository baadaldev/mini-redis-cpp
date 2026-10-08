#ifndef SYNC_HPP
#define SYNC_HPP

#include <functional>

#ifdef _WIN32
#include <windows.h>

// High-performance Win32 CriticalSection Mutex
class Mutex {
public:
    Mutex() {
        InitializeCriticalSection(&cs_);
    }

    ~Mutex() {
        DeleteCriticalSection(&cs_);
    }

    void lock() {
        EnterCriticalSection(&cs_);
    }

    void unlock() {
        LeaveCriticalSection(&cs_);
    }

private:
    CRITICAL_SECTION cs_;
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
};

// RAII Lock Guard
class LockGuard {
public:
    explicit LockGuard(Mutex& m) : m_(m) {
        m_.lock();
    }

    ~LockGuard() {
        m_.unlock();
    }

private:
    Mutex& m_;
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
};

// Thread helper using Windows native CreateThread with WINAPI calling convention
class Thread {
private:
    static DWORD WINAPI thread_runner(LPVOID param) {
        auto* fn = static_cast<std::function<void()>*>(param);
        if (fn) {
            (*fn)();
            delete fn;
        }
        return 0;
    }

public:
    template <typename Callable>
    static void spawn_detached(Callable func) {
        auto* fn_ptr = new std::function<void()>(func);
        HANDLE hThread = CreateThread(
            nullptr,
            0,
            thread_runner,
            fn_ptr,
            0,
            nullptr
        );
        if (hThread) {
            CloseHandle(hThread);
        }
    }
};

#else // Non-Windows (Linux, macOS, Unix with POSIX threads)

#include <pthread.h>

class Mutex {
public:
    Mutex() {
        pthread_mutex_init(&mutex_, nullptr);
    }

    ~Mutex() {
        pthread_mutex_destroy(&mutex_);
    }

    void lock() {
        pthread_mutex_lock(&mutex_);
    }

    void unlock() {
        pthread_mutex_unlock(&mutex_);
    }

private:
    pthread_mutex_t mutex_;
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
};

class LockGuard {
public:
    explicit LockGuard(Mutex& m) : m_(m) {
        m_.lock();
    }

    ~LockGuard() {
        m_.unlock();
    }

private:
    Mutex& m_;
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
};

class Thread {
private:
    static void* thread_runner(void* param) {
        auto* fn = static_cast<std::function<void()>*>(param);
        if (fn) {
            (*fn)();
            delete fn;
        }
        return nullptr;
    }

public:
    template <typename Callable>
    static void spawn_detached(Callable func) {
        auto* fn_ptr = new std::function<void()>(func);
        pthread_t th;
        if (pthread_create(&th, nullptr, thread_runner, fn_ptr) == 0) {
            pthread_detach(th);
        } else {
            delete fn_ptr;
        }
    }
};

#endif // _WIN32

#endif // SYNC_HPP
