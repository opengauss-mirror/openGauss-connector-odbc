# Windows ODBC regression tests

This directory contains minimal Windows-only regression tests that exercise
driver behavior without requiring a running openGauss server.

## Build

Use the same MinGW-w64 i686 toolchain that is used for the driver:

```cmd
set PATH=C:\buildtools\msys64\mingw32\bin;C:\buildtools\msys64\usr\bin;%PATH%
cd windows\test
i686-w64-mingw32-gcc -o sqlconnectw_invalid_length_test.exe ^
    sqlconnectw_invalid_length_test.c -lodbc32
```

## Run

```cmd
.\sqlconnectw_invalid_length_test.exe
```

Expected output:

```
OK: SQLConnectW with invalid negative length returned SQL_ERROR
```

## Test coverage

- `sqlconnectw_invalid_length_test.c`
  - Vulnerability #9 from `odbc-brainafk/漏洞清单.csv`
  - Verifies that `SQLConnectW` rejects a negative `cbDSN` length other than
    `SQL_NTS`, instead of reading past the wide-character DSN buffer.
