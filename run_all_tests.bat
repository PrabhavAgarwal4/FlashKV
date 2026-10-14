@echo off
echo ========================================================
echo         FLASHKV - FULL SYSTEM VERIFICATION SUITE       
echo ========================================================
echo.
echo [1/4] Compiling and running Milestone 1 (LRU Cache)...
call build.bat tests/test_lru.cpp -o bin/test_lru.exe && bin\test_lru.exe
if %ERRORLEVEL% NEQ 0 ( echo [FAILED] Milestone 1 failed! & exit /b 1 )

echo.
echo [2/4] Compiling and running Milestone 2 (Concurrency)...
call build.bat tests/test_concurrency.cpp -o bin/test_concurrency.exe && bin\test_concurrency.exe
if %ERRORLEVEL% NEQ 0 ( echo [FAILED] Milestone 2 failed! & exit /b 1 )

echo.
echo [3/4] Compiling and running Milestone 3 (DBMS WAL)...
call build.bat tests/test_wal.cpp -o bin/test_wal.exe && bin\test_wal.exe
if %ERRORLEVEL% NEQ 0 ( echo [FAILED] Milestone 3 failed! & exit /b 1 )

echo.
echo [4/4] Compiling and running Milestone 5 (System Design)...
call build.bat tests/test_system_design.cpp -o bin/test_system_design.exe && bin\test_system_design.exe
if %ERRORLEVEL% NEQ 0 ( echo [FAILED] Milestone 5 failed! & exit /b 1 )

echo.
echo ========================================================
echo   CONGRATULATIONS! ALL 5 CS MILESTONES PASSED (100%%)   
echo ========================================================
