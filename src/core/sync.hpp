#ifndef SYNC_HPP
#define SYNC_HPP

#include <windows.h>
#include <functional>

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

#endif // SYNC_HPP
