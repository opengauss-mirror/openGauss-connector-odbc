# Security Fix Tests

This directory contains `security-fix-test`, a regression test for the security
fixes tracked in `odbc-brainafk/漏洞清单.csv` rows 7–15.

## Build

```bash
cd odbc-test-gauss
make security-fix-test
```

## Run

```bash
./security-fix-test
```

To force a specific driver binary instead of the one configured in the
`gaussdb` DSN:

```bash
ODBC_DRIVER=$PWD/../output/odbc/lib/psqlodbcw.so ./security-fix-test
```

## Enabling MYLOG for the password-redaction test

`test_log_password_redaction` inspects the driver's MYLOG output to confirm
that passwords and SSL file paths are masked. The test will fail if MYLOG is
not enabled.

On Linux the driver reads its global `Debug` setting from `odbcinst.ini`.

1. Find the driver section used by your DSN or `ODBC_DRIVER` path:

   ```bash
   odbcinst -q -d
   # or
   grep -A5 '^\[.*\]$' /etc/odbcinst.ini
   ```

2. Add `Debug=1` to that section. For example, if the section is
   `[PostgreSQL Unicode]`:

   ```ini
   [PostgreSQL Unicode]
   Driver=/home/luodongxu/openGauss-connector-odbc/output/odbc/lib/psqlodbcw.so
   Debug=1
   ```

   If you are unsure of the exact section name, look at the `Driver=` line in
   `/etc/odbc.ini` (or `~/.odbc.ini`) for your DSN; the value is the section
   name in `odbcinst.ini`.

3. MYLOG files are written to `/tmp` by default:

   ```
   /tmp/mylog_<exe-name>_<user><pid>.log
   ```

   Make sure `/tmp` is writable. You can also set `LogDir` in the same
   `odbcinst.ini` section to change the directory.

4. Run the test again. You should see lines such as:

   ```
   OK: no plaintext password/SSL values found in MYLOG
   OK: MYLOG masks password
   ```

## What is covered

| # | Vulnerability | Test |
|---|---------------|------|
| #1 | `SQLDriverConnect` debug log password leak | `test_log_password_redaction` |
| #2 | `SQLPutData` negative length heap underflow | `test_sqlputdata_negative_length` |
| #3 | libpq URL debug log password/SSL leak | `test_log_password_redaction` |
| #5 | Descriptor UAF after free | `test_descriptor_uaf` |
| #6 | Unbounded result-set materialization | `test_result_set_cap` |
| #7 | libpq URL buffer overflow | `test_long_sslcert_path` |
| #8 | `SQLBindParameter(ipar=0)` out-of-bounds write | `test_bind_parameter_zero` |
| #10 | `SQLPrepareW` orphan UTF-16 high surrogate handling | `test_sqlpreparew_orphan_surrogate` (Linux when `SQLWCHAR` is 2 bytes, and Windows) |

| #9 | `SQLConnectW` length validation | `test_sqlconnectw_invalid_length` (Windows only) |
