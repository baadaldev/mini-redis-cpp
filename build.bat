@echo off
echo ========================================================
echo   Building MiniRedis C++ Server ^& Test Suite
echo ========================================================

echo [1/2] Compiling mini_redis.exe...
g++ -std=c++14 src/main.cpp src/network/server.cpp src/core/storage_engine.cpp src/protocol/resp_parser.cpp src/persistence/wal.cpp -lws2_32 -O2 -o mini_redis.exe
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile mini_redis.exe!
    exit /b %ERRORLEVEL%
)

echo [2/2] Compiling test.exe...
g++ -std=c++14 tests/test_engine.cpp src/core/storage_engine.cpp src/protocol/resp_parser.cpp src/persistence/wal.cpp -O2 -o test.exe
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile test.exe!
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================================
echo   Running Automated Test Suite
echo ========================================================
test.exe
if %ERRORLEVEL% neq 0 (
    echo [ERROR] One or more tests failed!
    exit /b %ERRORLEVEL%
)

echo.
echo [SUCCESS] Build completed!
echo.
echo To start the server:
echo   .\mini_redis.exe --port 6379 --aof data.aof
echo.
echo To run client tests:
echo   python client/test_client.py 6379
echo.
echo To run benchmark:
echo   python client/benchmark.py 6379 5000
echo ========================================================
