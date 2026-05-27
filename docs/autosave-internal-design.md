# ODBC autosave=internal 需求分析与设计文档

本文档描述 openGauss ODBC Driver 中 `Autosave=internal` 的需求、协议设计、libpq 适配、ODBC 调用链路、状态变量变化、报文对照、验证方法和工程落地边界。

相关分支：

- ODBC: <https://github.com/opengauss-mirror/openGauss-connector-odbc/compare/master...jarvis24young:openGauss-connector-odbc:feat/autosave-internal?expand=1>
- libpq: <https://github.com/opengauss-mirror/openGauss-server/compare/master...jarvis24young:openGauss-server:feat/autosave-internal?expand=1>

当前文档基于以下本地分支状态编写：

- ODBC commit: `f69c13e fix(odbc): handle silent protocol autosave packets`
- libpq commit: `963764515 fix(libpq): harden PQsendSavepointCommand and PQsendProtocolSync`

## 1. 背景与目标

### 1.1 业务背景

ODBC Driver 在关闭自动提交后，应用通常会在一个显式事务中连续执行多条语句：

```sql
START TRANSACTION;
INSERT ...;
INSERT ...;   -- 可能失败
INSERT ...;   -- 应继续执行
COMMIT;
```

在 PostgreSQL/openGauss 协议语义中，事务块内任意一条 SQL 失败后，事务会进入 failed transaction 状态。此时继续执行普通 SQL 会得到 `25P02 current transaction is aborted`，除非应用显式执行 `ROLLBACK` 或 `ROLLBACK TO SAVEPOINT`。

ODBC 现有 statement rollback 机制会在合适时机创建内部 savepoint，并在语句失败后回滚到该 savepoint，从而让事务继续保持可用。

`Autosave=internal` 的目标是：用数据库内核支持的专用协议包替代 SQL 文本形式的内部 savepoint，减少 SQL 解析与报文长度开销，同时保持 ODBC 原有 statement rollback 语义。

### 1.2 传统 SQL savepoint 的问题

传统方式依赖 SQL 文本：

```sql
SAVEPOINT _EXEC_SVP_...
ROLLBACK TO _EXEC_SVP_...
```

问题：

- 每次 savepoint 都要经过 SQL 解析、执行器分发、命令完成返回。
- 报文长度大于专用协议包。
- 在高频短 SQL 场景中，savepoint 额外成本会放大。

### 1.3 autosave=internal 优化目标

`Autosave=internal` 使用前端协议包：

```text
create savepoint:   35 00 00 00 05 01
rollback savepoint: 35 00 00 00 05 02
sync:               53 00 00 00 04
```

目标：

- 创建内部 savepoint 时不再发送 SQL 文本。
- 语句失败后使用协议 rollback savepoint，而不是 SQL `ROLLBACK TO SAVEPOINT`。
- duplicate key 等普通 SQL 错误后，事务恢复到 `ReadyForQuery status=T`。
- 不改变未开启 `Autosave=internal` 的默认行为。
- 不影响 `ForExtensionConnector=1` 场景，该场景仍禁用内部 savepoint。

## 2. 需求范围

### 2.1 功能需求

1. 新增连接参数：

```text
Autosave=internal
AS=internal
```

2. 当连接满足以下条件时启用 protocol autosave：

```text
conn != NULL
conn->connInfo.autosave == AUTOSAVE_INTERNAL
conn->connInfo.drivers.for_extension_connector == 0
```

3. 在事务内执行需要 statement rollback 保护的 SQL 前，发送 create savepoint 协议包。

4. 当语句失败且内部 savepoint 有效时，发送 rollback savepoint 协议包和 Sync：

```text
35 00 00 00 05 02
53 00 00 00 04
```

5. rollback savepoint 后必须等待 `ReadyForQuery`，并将本地事务状态从 failed transaction 恢复为 in transaction。

6. 如果未建立内部 savepoint，ODBC 保持原有降级行为，即通过 `CC_abort()` 回滚整个事务。

7. 输出连接串中保留 `Autosave=internal`，便于应用确认配置生效。

### 2.2 非功能需求

1. 兼容性：

   - 未设置 `Autosave=internal` 时，不改变现有行为。
   - `ForExtensionConnector=1` 时不启用内部 savepoint。
   - 构建时必须链接包含新增 libpq 符号的 libpq。

2. 性能：

   - create savepoint 不走 SQL parser。
   - reduce round trip 场景中，create savepoint 协议包和实际 SQL 一起发送。

3. 可靠性：

   - 协议发送失败必须记录通信错误。
   - `Sync` 发送失败后连接协议状态不可判定，需要将连接标记为不可继续复用。

### 2.3 非目标

1. 本文档不描述服务端如何处理 `0x35` 包。这里假设数据库已经支持该协议。
2. 不新增 RELEASE SAVEPOINT 协议包。
3. 不改变 ODBC 现有 statement rollback 策略，只替换内部 savepoint 的底层实现。
4. 不为未打补丁的 libpq 提供运行时降级。当前设计采用构建期符号检查。

## 3. 协议设计

### 3.1 Savepoint 协议包

ODBC/libpq 使用前端消息类型 `0x35`，Wireshark 通常显示为 `Unknown`。

#### create savepoint

```text
35 00 00 00 05 01
```

字段：

| 字段 | 长度 | 值 | 说明 |
| --- | --- | --- | --- |
| message type | 1 | `0x35` | savepoint 协议包 |
| length | 4 | `0x00000005` | 包体长度包含 length 自身 |
| command | 1 | `0x01` | create savepoint |

#### rollback savepoint

```text
35 00 00 00 05 02
```

字段：

| 字段 | 长度 | 值 | 说明 |
| --- | --- | --- | --- |
| message type | 1 | `0x35` | savepoint 协议包 |
| length | 4 | `0x00000005` | 包体长度包含 length 自身 |
| command | 1 | `0x02` | rollback to savepoint |

### 3.2 Sync 协议包

rollback savepoint 后需要发送 Sync：

```text
53 00 00 00 04
```

Sync 作用：

- 结束当前协议批次。
- 让 libpq 可以通过 `PQgetResult()` 消费服务端响应。
- 等待 `ReadyForQuery`，从而确认事务状态已经恢复。

### 3.3 关键协议语义：savepoint 协议包是静默包

这是 ODBC 实现中最重要的设计点。

`0x35` create savepoint 和 rollback savepoint 本身不产生独立的 `PGresult`：

- create savepoint 后，第一个 `PGresult` 属于后面的实际 SQL。
- rollback savepoint + Sync 后，成功场景服务端只返回 `ReadyForQuery`。

因此 ODBC 不能做以下错误假设：

```text
错误假设: create savepoint 协议包会返回 PGRES_COMMAND_OK
错误假设: rollback savepoint 协议包会返回 PGRES_COMMAND_OK
```

正确模型：

```text
create savepoint:   silent
rollback savepoint: silent, Sync 后等待 ReadyForQuery
```

## 4. 报文对照

### 4.1 ODBC simple query 报文

以下报文来自 ODBC `Autosave=internal` 测试场景：

```text
19  >5/Q
20  <C/Z
21  >5/Q
22  <E
23  <Z
25  >5/S
26  <Z
27  >5/Q
28  <C/Z
```

解释：

| Frame | 方向 | 报文 | 说明 |
| --- | --- | --- | --- |
| 19 | client -> server | `>5/Q` | create savepoint 协议包 + INSERT |
| 20 | server -> client | `<C/Z` | INSERT 成功，事务仍为 in transaction |
| 21 | client -> server | `>5/Q` | create savepoint 协议包 + duplicate INSERT |
| 22 | server -> client | `<E` | duplicate key 错误 |
| 23 | server -> client | `<Z` | ReadyForQuery，状态为 failed transaction |
| 25 | client -> server | `>5/S` | rollback savepoint 协议包 + Sync |
| 26 | server -> client | `<Z` | ReadyForQuery，状态恢复为 in transaction |
| 27 | client -> server | `>5/Q` | create savepoint 协议包 + 下一条 INSERT |
| 28 | server -> client | `<C/Z` | INSERT 成功 |

其中：

```text
>5/Q = 35 00 00 00 05 01 + Simple Query
>5/S = 35 00 00 00 05 02 + Sync
```

### 4.2 JDBC extended query 报文

JDBC 报文中常见：

```text
>5/4/5/P/B/D/E/S
<1/2/n/C/Z

>5/4/5/P/B/D/E/S
<1/2/n/E
<Z failed transaction

>5/S
<Z in transaction
```

ODBC simple query 和 JDBC extended query 的 SQL 承载方式不同：

- ODBC 当前测试用 `Q` Simple Query。
- JDBC 用 `P/B/D/E/S` Extended Query。

但 autosave 协议语义一致：

```text
SQL 前创建: 5
SQL 失败后回滚: 5/S
```

## 5. libpq 设计

libpq 改动位于 openGauss-server 仓库的 `src/common/interfaces/libpq`。

### 5.1 导出接口

头文件：`src/include/libpq/libpq-fe.h`

```c
extern int PQsendSavepoint(PGconn* conn);
extern int PQsendRollbackToSavepoint(PGconn* conn);
extern int PQsendProtocolSync(PGconn* conn);
```

导出表：`src/common/interfaces/libpq/exports.txt`

```text
PQsendSavepoint           172
PQsendRollbackToSavepoint 173
PQsendProtocolSync        174
```

### 5.2 PQsendSavepointCommand

实现文件：`src/common/interfaces/libpq/fe-exec.cpp`

核心职责：

1. 检查 `conn != NULL`。
2. 清理 `conn->errorMessage`。
3. 检查连接状态为 `CONNECTION_OK`。
4. 检查 `asyncStatus == PGASYNC_IDLE`。
5. 写入协议包：

```c
pqPutMsgStart('5', false, conn);
pqPutc(command, conn);
pqPutMsgEnd(conn);
```

6. 不 flush。
7. 不修改 `asyncStatus`。

原因：

- create savepoint 要和后续 SQL 一起发送，减少 round trip。
- rollback savepoint 需要由调用方继续发送 Sync，再统一 flush。

### 5.3 PQsendSavepoint

```c
int PQsendSavepoint(PGconn* conn)
{
    return PQsendSavepointCommand(conn, (char)0x01);
}
```

写入：

```text
35 00 00 00 05 01
```

### 5.4 PQsendRollbackToSavepoint

```c
int PQsendRollbackToSavepoint(PGconn* conn)
{
    return PQsendSavepointCommand(conn, (char)0x02);
}
```

写入：

```text
35 00 00 00 05 02
```

### 5.5 PQsendProtocolSync

职责：

1. 检查连接状态。
2. 检查 `asyncStatus == PGASYNC_IDLE`。
3. 写入 Sync：

```text
53 00 00 00 04
```

4. `pqFlush(conn)`。
5. 设置：

```c
conn->asyncStatus = PGASYNC_BUSY;
```

调用方随后使用 `PQgetResult()` 消费响应，直到返回 `NULL`。

### 5.6 libpq 工程结论

libpq 设计是底层发送 API，不负责 ODBC/JDBC 的 autosave 状态机。

正确职责边界：

| 层级 | 职责 |
| --- | --- |
| libpq | 提供协议包发送和 Sync 能力 |
| ODBC | 决定何时创建 savepoint、何时 rollback、如何维护本地状态 |
| server | 识别并执行 `0x35` 协议包 |

ODBC 问题不能通过 libpq 返回 `PGRES_COMMAND_OK` 解决，因为协议定义上 `0x35` 是静默包。

## 6. ODBC 文件级设计

### 6.1 `psqlodbc.h`

新增 autosave 枚举：

```c
enum {
    AUTOSAVE_UNSPECIFIED = -1,
    AUTOSAVE_INTERNAL = 1
};
```

`ConnInfo` 中新增：

```c
signed char autosave;
```

默认值：

```c
conninfo->autosave = AUTOSAVE_UNSPECIFIED;
```

### 6.2 `dlg_specific.h` 和 `dlg_specific.c`

新增连接参数：

```c
#define INI_AUTOSAVE "Autosave"
#define ABBR_AUTOSAVE "AS"
```

连接串解析：

```c
else if (stricmp(attribute, INI_AUTOSAVE) == 0 ||
         stricmp(attribute, ABBR_AUTOSAVE) == 0)
{
    if (stricmp(value, "internal") == 0)
        ci->autosave = AUTOSAVE_INTERNAL;
    else
        ci->autosave = AUTOSAVE_UNSPECIFIED;
}
```

DSN 读取：

```c
if (SQLGetPrivateProfileString(DSN, INI_AUTOSAVE, ...) > 0)
{
    if (stricmp(temp, "internal") == 0)
        ci->autosave = AUTOSAVE_INTERNAL;
}
```

输出连接串：

```c
Autosave=internal
```

### 6.3 `connection.h`

新增门禁宏：

```c
#define CC_uses_protocol_autosave(conn_) \
    ((conn_) && (conn_)->connInfo.autosave == AUTOSAVE_INTERNAL \
     && !(conn_)->connInfo.drivers.for_extension_connector)
```

启用 internal autosave 必须同时满足：

```text
autosave == AUTOSAVE_INTERNAL
ForExtensionConnector == 0
```

### 6.4 `CMakeLists.txt`, `configure.ac`, `configure`

构建期检查 libpq 是否有新增符号：

```text
PQsendSavepoint
PQsendRollbackToSavepoint
PQsendProtocolSync
```

工程意义：

- 防止 ODBC 编译链接到旧 libpq 后运行时报缺符号。
- 对公共 PR 更安全。
- 如果移植到私有驱动且 libpq 固定可控，可以临时不引入配置脚本改动，但工程落地建议保留。

### 6.5 `execute.c`

核心函数：

- `StartRollbackState`
- `SetStatementSvp`
- `DiscardStatementSvp`

职责：

| 函数 | 职责 |
| --- | --- |
| `StartRollbackState` | 在 SQL API 入口处标记当前 statement 是否需要 rollback 保护 |
| `SetStatementSvp` | 在语句执行前创建或准备创建内部 savepoint |
| `DiscardStatementSvp` | 在语句执行后按结果决定是否 rollback to savepoint |

### 6.6 `connection.c`

核心函数：

- `CC_queue_protocol_savepoint`
- `CC_send_protocol_savepoint`
- `CC_internal_rollback`
- `CC_send_query_append`

职责：

| 函数 | 职责 |
| --- | --- |
| `CC_queue_protocol_savepoint` | 调用 libpq 发送 create/rollback savepoint 包 |
| `CC_send_protocol_savepoint` | 发送 savepoint 包，可选 Sync，并消费响应 |
| `CC_internal_rollback` | 执行 statement/per-query rollback |
| `CC_send_query_append` | 在 SQL 前拼入 protocol create savepoint，并处理结果 |

## 7. 核心执行链路

以下链路以测试 SQL 为例：

```sql
CREATE TEMP TABLE odbc_autosave_capture_2(id int primary key);
INSERT INTO odbc_autosave_capture_2 VALUES (1);
INSERT INTO odbc_autosave_capture_2 VALUES (1); -- duplicate key
INSERT INTO odbc_autosave_capture_2 VALUES (2);
```

### 7.1 SQLExecDirect 入口

文件：`odbcapi.c`

```c
StartRollbackState(stmt);
ret = PGAPI_ExecDirect(...);
ret = DiscardStatementSvp(stmt, ret, FALSE);
```

状态：

```text
SQL API 入口负责前置标记和后置清理
真正 SQL 执行在 PGAPI_ExecDirect -> PGAPI_Execute -> SC_execute
```

### 7.2 StartRollbackState

文件：`execute.c`

internal 模式：

```c
if (CC_uses_protocol_autosave(conn))
{
    ret = (conn && PG_VERSION_LT(conn, 8.0)) ? 1 : 2;
}
```

openGauss 版本大于 8.0，因此：

```text
ret = 2
```

然后：

```c
SC_start_rb_stmt(stmt);
```

变量变化：

| 变量 | 变化 |
| --- | --- |
| `stmt->rb_or_tc` | 设置 rollback statement bit |
| `SC_is_rb_stmt(stmt)` | `false -> true` |

### 7.3 SC_execute

文件：`statement.c`

对于普通 SQLExecDirect，`copy_statement_with_parameters()` 通常会生成 `stmt->stmt_with_params`，因此使用 simple query：

```c
res = CC_send_query(conn, self->stmt_with_params, NULL, qflag, SC_get_ancestor(self));
```

即：

```text
CC_send_query -> CC_send_query_append
```

### 7.4 CC_send_query_append 进入前置 savepoint 决策

文件：`connection.c`

关键变量：

```c
issue_begin = ((flag & GO_INTO_TRANSACTION) != 0 && !CC_is_in_trans(self));
rollback_on_error = (flag & ROLLBACK_ON_ERROR) != 0;
query_rollback = (rollback_on_error && !end_with_commit && PG_VERSION_GE(self, 8.0));
consider_rollback = (issue_begin ||
    (CC_is_in_trans(self) && !CC_is_in_error_trans(self)) ||
    strnicmp(query, "begin", 5) == 0);
```

典型 INSERT 场景：

```text
CC_is_in_trans(self) = true
issue_begin = false
rollback_on_error = false
query_rollback = false
consider_rollback = true
```

于是调用：

```c
SetStatementSvp(astmt, svpopt);
```

### 7.5 SetStatementSvp 设置 PREPEND_IN_PROGRESS

文件：`execute.c`

前置条件：

```c
!CC_started_rbpoint(conn)
0 == (conn->opt_previous & SVPOPT_RDONLY)
SC_is_rb_stmt(stmt)
CC_is_in_trans(conn)
!conn->connInfo.drivers.for_extension_connector
```

internal 模式并且 reduce round trip：

```c
if (CC_uses_protocol_autosave(conn))
{
    if (0 != (option & SVPOPT_REDUCE_ROUNDTRIP))
    {
        conn->internal_op = PREPEND_IN_PROGRESS;
        CC_set_accessed_db(conn);
        return ret;
    }
}
```

变量变化：

| 变量 | 变化 |
| --- | --- |
| `conn->internal_op` | `0 -> PREPEND_IN_PROGRESS` |
| `conn->rbonerr` | 设置 accessed bit |
| `CC_accessed_db(conn)` | `false -> true` |
| `conn->internal_svp` | 保持 `0` |

此时只是准备将 savepoint 包拼到 SQL 前面，还没有真正标记 savepoint 可用。

### 7.6 CC_send_query_append 拼接并发送协议包

回到 `connection.c`：

```c
if (PREPEND_IN_PROGRESS == self->internal_op)
    prepend_savepoint = TRUE;
```

进入：

```c
else if (prepend_savepoint)
{
    if (CC_uses_protocol_autosave(self))
    {
        CC_queue_protocol_savepoint(self, FALSE, func);
        self->internal_op = SAVEPOINT_IN_PROGRESS;
        protocol_statement_savepoint = TRUE;
        protocol_savepoint_queued = TRUE;
    }
}
```

变量变化：

| 变量 | 变化 |
| --- | --- |
| `prepend_savepoint` | `false -> true` |
| `self->internal_op` | `PREPEND_IN_PROGRESS -> SAVEPOINT_IN_PROGRESS` |
| `protocol_statement_savepoint` | `false -> true` |
| `protocol_savepoint_queued` | `false -> true` |

`CC_queue_protocol_savepoint(FALSE)` 调用：

```c
PQsendSavepoint(self->pqconn);
```

向 libpq 输出缓冲写入：

```text
35 00 00 00 05 01
```

随后执行：

```c
PQsendQuery(self->pqconn, query_buf.data);
```

实际报文：

```text
>5/Q
```

### 7.7 本地 rollback point 标记

最新修复后的关键逻辑：

```c
protocol_savepoint_queued = FALSE;
/*
 * The protocol savepoint packet is silent. The first PGresult belongs to
 * the SQL query, so mark the local rollback point without discarding it.
 */
if (protocol_statement_savepoint &&
    SAVEPOINT_IN_PROGRESS == self->internal_op)
{
    CC_start_rbpoint(self);
    self->internal_op = 0;
}
```

`CC_start_rbpoint`：

```c
#define CC_start_rbpoint(a) \
    ((a)->rbonerr |= (1L << 4), (a)->internal_svp = 1)
```

变量变化：

| 变量 | 变化 |
| --- | --- |
| `protocol_savepoint_queued` | `true -> false` |
| `conn->rbonerr` | 设置 rbpoint bit |
| `CC_started_rbpoint(conn)` | `false -> true` |
| `conn->internal_svp` | `0 -> 1` |
| `conn->internal_op` | `SAVEPOINT_IN_PROGRESS -> 0` |

这一步必须发生。否则后续语句失败时不会进入 `CC_internal_rollback()`。

### 7.8 SQL 失败后的结果处理

duplicate key 时服务端返回：

```text
<E
<Z status=69
```

ODBC 收到 `PGRES_FATAL_ERROR`，设置 statement error，最终：

```text
SC_execute 返回 SQL_ERROR
PGAPI_Execute 返回 SQL_ERROR
PGAPI_ExecDirect 返回 SQL_ERROR
```

然后回到 `odbcapi.c`：

```c
ret = DiscardStatementSvp(stmt, ret, FALSE);
```

### 7.9 DiscardStatementSvp 触发 CC_internal_rollback

文件：`execute.c`

前置检查：

```c
if (!CC_accessed_db(conn) || !CC_is_in_trans(conn))
    goto cleanup;

if (!SC_is_rb_stmt(stmt) && !SC_is_tc_stmt(stmt))
    goto cleanup;
```

正常 duplicate key 场景：

| 变量 | 期望值 |
| --- | --- |
| `ret` | `SQL_ERROR` |
| `CC_accessed_db(conn)` | `true` |
| `CC_is_in_trans(conn)` | `true` |
| `SC_is_rb_stmt(stmt)` | `true` |
| `CC_started_rbpoint(conn)` | `true` |
| `conn->internal_svp` | `1` |

核心分支：

```c
if (SQL_ERROR == ret)
{
    if (CC_started_rbpoint(conn) && conn->internal_svp)
    {
        int cmd_success = CC_internal_rollback(conn, PER_STATEMENT_ROLLBACK, FALSE);
        if (!cmd_success)
        {
            SC_set_error(stmt, STMT_INTERNAL_ERROR, "internal ROLLBACK failed", func);
            goto cleanup;
        }
    }
    else if (!conn->connInfo.drivers.for_extension_connector)
    {
        CC_abort(conn);
        goto cleanup;
    }
}
```

进入 `CC_internal_rollback()` 的必要条件：

```text
ret == SQL_ERROR
CC_accessed_db(conn) == true
CC_is_in_trans(conn) == true
SC_is_rb_stmt(stmt) == true
CC_started_rbpoint(conn) == true
conn->internal_svp == 1
```

如果 `conn->internal_svp == 0`，会走：

```c
CC_abort(conn);
```

这会发送 SQL：

```sql
ROLLBACK
```

整个事务会被回滚，临时表也可能消失。这正是修复前测试失败的原因。

### 7.10 CC_internal_rollback

文件：`connection.c`

statement rollback 分支：

```c
case PER_STATEMENT_ROLLBACK:
    if (CC_uses_protocol_autosave(self) && self->internal_svp)
    {
        ret = CC_send_protocol_savepoint(self, TRUE, TRUE, ignore_abort, __FUNCTION__);
        break;
    }
```

变量：

| 变量 | 值 |
| --- | --- |
| `rollback_type` | `PER_STATEMENT_ROLLBACK` |
| `CC_uses_protocol_autosave(self)` | `true` |
| `self->internal_svp` | `1` |
| `rollback` | `TRUE` |
| `sync` | `TRUE` |

### 7.11 CC_send_protocol_savepoint 执行 rollback

文件：`connection.c`

```c
CC_queue_protocol_savepoint(self, rollback, func);
PQsendProtocolSync(self->pqconn);
while ((pgres = PQgetResult(self->pqconn)) != NULL)
{
    ...
}
if (ret && rollback)
{
    LIBPQ_update_transaction_status(self);
}
```

`rollback == TRUE` 时：

```c
PQsendRollbackToSavepoint(self->pqconn);
```

写入：

```text
35 00 00 00 05 02
```

然后：

```c
PQsendProtocolSync(self->pqconn);
```

写入：

```text
53 00 00 00 04
```

实际报文：

```text
>5/S
```

服务端返回：

```text
<Z status=84
```

`LIBPQ_update_transaction_status()` 将本地状态更新为：

```text
CC_is_in_trans(conn) = true
CC_is_in_error_trans(conn) = false
```

### 7.12 cleanup 阶段

`DiscardStatementSvp()` 最后会：

```c
CC_start_stmt(conn);
```

宏：

```c
#define CC_start_stmt(a) ((a)->rbonerr = 0)
```

变量变化：

| 变量 | 变化 |
| --- | --- |
| `conn->rbonerr` | 清零 |
| `CC_started_rbpoint(conn)` | `true -> false` |
| `CC_accessed_db(conn)` | `true -> false` |

下一条 SQL 会重新创建新的 protocol savepoint：

```text
>5/Q
```

## 8. 状态变量总表

### 8.1 正常失败恢复路径

| 阶段 | `internal_op` | `internal_svp` | `CC_started_rbpoint` | `CC_accessed_db` | 说明 |
| --- | --- | --- | --- | --- | --- |
| SQL API 入口 | `0` | `0` | `false` | `false` | 初始 |
| `StartRollbackState` | `0` | `0` | `false` | `false` | `stmt` 被标记为 rb stmt |
| `SetStatementSvp` | `PREPEND_IN_PROGRESS` | `0` | `false` | `true` | 准备 prepend savepoint |
| `CC_queue_protocol_savepoint` | `SAVEPOINT_IN_PROGRESS` | `0` | `false` | `true` | 已排队 `35...01` |
| `PQsendQuery` 成功后 | `0` | `1` | `true` | `true` | 本地 rollback point 建立 |
| SQL 返回错误 | `0` | `1` | `true` | `true` | 事务处于 failed |
| `DiscardStatementSvp` | `0` | `1` | `true` | `true` | 命中 rollback 分支 |
| `CC_internal_rollback` 后 | `0` | `1` | `true` | `true` | 服务端恢复 in transaction |
| cleanup | `0` | `1` | `false` | `false` | `rbonerr` 清零，下一条语句重建 savepoint |

说明：`CC_start_stmt()` 清的是 `rbonerr`，不直接清 `internal_svp`。后续逻辑会根据新语句重新维护 rbpoint。

### 8.2 异常退化路径

如果 duplicate key 到达 `DiscardStatementSvp()` 时：

```text
conn->internal_svp = 0
```

则不会进入：

```c
CC_internal_rollback(...)
```

而会进入：

```c
CC_abort(conn)
```

报文表现：

```text
Q ROLLBACK
```

后果：

- 整个事务回滚。
- 在该事务中创建的临时表也会回滚。
- 后续 SQL 可能报 relation does not exist。

## 9. 关键修复说明

### 9.1 修复前错误模型

修复前 ODBC 将 protocol create savepoint 当成会返回 `PGRES_COMMAND_OK` 的命令：

```text
错误处理方式:
1. 发送 35...01 + SQL
2. 等第一个 PGresult
3. 如果是 PGRES_COMMAND_OK，就认为是 savepoint 结果并丢弃
```

问题：

- 成功 INSERT 的第一个 `PGRES_COMMAND_OK` 实际是 `INSERT 0 1`，不是 savepoint。
- duplicate key 的第一个结果是 `PGRES_FATAL_ERROR`，导致 `CC_start_rbpoint()` 没执行。
- `conn->internal_svp` 保持 `0`，失败后退化成整事务 `ROLLBACK`。

### 9.2 修复后正确模型

修复后：

```text
1. 发送 35...01 + SQL
2. 不等待 savepoint 独立响应
3. PQsendQuery 成功后立即本地标记 rollback point
4. 第一个 PGresult 正常交给 SQL 结果处理
```

关键代码：

```c
protocol_savepoint_queued = FALSE;
if (protocol_statement_savepoint &&
    SAVEPOINT_IN_PROGRESS == self->internal_op)
{
    CC_start_rbpoint(self);
    self->internal_op = 0;
}
```

### 9.3 rollback savepoint 也按静默包处理

rollback savepoint + Sync 成功时，服务端只需要返回 ReadyForQuery。`CC_send_protocol_savepoint()` 默认 `ret = 1`，只在收到错误 PGresult 时置失败。

```c
/* Protocol savepoint packets are silent; no PGresult is success. */
int ret = 1;
```

## 10. per-query rollback 路径

当前 ODBC 还存在 `ROLLBACK_ON_ERROR` 控制的 per-query rollback 路径：

```c
query_rollback = (rollback_on_error && !end_with_commit && PG_VERSION_GE(self, 8.0));
```

当 `query_rollback == true` 且 `Autosave=internal` 时：

```c
CC_queue_protocol_savepoint(self, FALSE, func);
...
if (!CC_internal_rollback(self, PER_QUERY_ROLLBACK, ignore_abort_on_conn))
    ignore_abort_on_conn = FALSE;
```

`PER_QUERY_ROLLBACK` 在 `CC_internal_rollback()` 中也走：

```c
CC_send_protocol_savepoint(self, TRUE, TRUE, ...)
```

因此 `CC_send_protocol_savepoint()` 按静默包处理后，statement rollback 和 per-query rollback 都能使用同一套协议 rollback 逻辑。

## 11. 配置与构建

### 11.1 运行配置

连接串示例：

```text
DSN=ODBC_DT_A;Protocol=7.4-2;ForExtensionConnector=0;Autosave=internal;SSLmode=disable
```

注意：

- `Autosave` 拼写必须正确。
- `internal` 大小写不敏感。
- `ForExtensionConnector=0` 才启用。

### 11.2 构建依赖

ODBC 必须链接包含以下符号的 libpq：

```text
PQsendSavepoint
PQsendRollbackToSavepoint
PQsendProtocolSync
```

验证：

```bash
nm -D /path/to/libpq.so | grep -E 'PQsendSavepoint|PQsendRollbackToSavepoint|PQsendProtocolSync'
```

确认 ODBC 实际加载的 libpq：

```bash
ldd /path/to/psqlodbca.so | grep libpq
```

确认 unixODBC 实际加载的驱动：

```bash
odbcinst -q -s -n ODBC_DT_A
```

## 12. 测试设计

### 12.1 功能正确性测试

测试步骤：

1. 连接串设置 `Autosave=internal`。
2. 关闭 autocommit。
3. 创建临时表。
4. 插入 `id=1`。
5. 再次插入 `id=1`，预期 duplicate key。
6. 插入 `id=2`，预期成功。
7. 查询 count，预期为 2。
8. commit。

预期报文：

```text
>5/Q
<C/Z
>5/Q
<E
<Z failed transaction
>5/S
<Z in transaction
>5/Q
<C/Z
```

失败判定：

- duplicate key 后出现 `Q ROLLBACK`，说明没有进入 protocol rollback。
- 后续插入报 `relation does not exist`，说明事务被整回滚。

### 12.2 配置解析测试

测试项：

| 输入 | 预期 |
| --- | --- |
| `Autosave=internal` | `connInfo.autosave = AUTOSAVE_INTERNAL` |
| `AS=internal` | `connInfo.autosave = AUTOSAVE_INTERNAL` |
| `Autosave=xxx` | `connInfo.autosave = AUTOSAVE_UNSPECIFIED` |
| 未设置 | 不启用 protocol autosave |
| `ForExtensionConnector=1` | 不启用 protocol autosave |

### 12.3 构建测试

测试项：

- 新 libpq 可构建 ODBC。
- 旧 libpq 缺符号时，configure/cmake 明确失败。

### 12.4 报文测试

使用 Wireshark/tcpdump 观察：

```text
Unknown length 5
```

Wireshark 不认识 `0x35`，显示 Unknown 是正常现象。

## 13. GDB 调试建议

### 13.1 观察是否命中 internal autosave

```gdb
b StartRollbackState
commands
silent
printf "[StartRollbackState] autosave=%d ext=%d uses=%d\n", conn->connInfo.autosave, conn->connInfo.drivers.for_extension_connector, CC_uses_protocol_autosave(conn)
continue
end
```

### 13.2 观察 savepoint 创建

```gdb
b SetStatementSvp
commands
silent
printf "[SetStatementSvp] in_trans=%d is_rb=%d started=%d internal_svp=%d op=%d option=%u\n", CC_is_in_trans(conn), SC_is_rb_stmt(stmt), CC_started_rbpoint(conn), conn->internal_svp, conn->internal_op, option
continue
end
```

### 13.3 观察 rollback point 状态变化

在 `CC_send_query_append` 停住后：

```gdb
watch -location self->internal_svp
commands
silent
printf "[WATCH internal_svp] new=%d op=%d rbonerr=0x%lx trans=0x%lx\n", self->internal_svp, self->internal_op, self->rbonerr, self->transact_status
bt 10
continue
end
```

再观察 `rbonerr`：

```gdb
watch -location self->rbonerr
commands
silent
printf "[WATCH rbonerr] new=0x%lx internal_svp=%d op=%d trans=0x%lx\n", self->rbonerr, self->internal_svp, self->internal_op, self->transact_status
bt 10
continue
end
```

### 13.4 观察是否进入 CC_internal_rollback

```gdb
b DiscardStatementSvp
commands
silent
printf "[Discard] ret=%d accessed=%d in_trans=%d err_trans=%d is_rb=%d started=%d internal_svp=%d op=%d rbonerr=0x%lx\n", ret, CC_accessed_db(conn), CC_is_in_trans(conn), CC_is_in_error_trans(conn), SC_is_rb_stmt(stmt), CC_started_rbpoint(conn), conn->internal_svp, conn->internal_op, conn->rbonerr
bt 8
continue
end
```

```gdb
b CC_internal_rollback
commands
silent
printf "[CC_internal_rollback] rollback_type=%d autosave=%d internal_svp=%d err_trans=%d\n", rollback_type, self->connInfo.autosave, self->internal_svp, CC_is_in_error_trans(self)
bt 8
continue
end
```

## 14. 工程落地评估

### 14.1 当前状态

当前 ODBC 实现已经具备基本工程落地条件：

- 配置解析完整。
- libpq 符号检查完整。
- `ForExtensionConnector` 门禁统一。
- create savepoint 和 rollback savepoint 均按静默协议包处理。
- duplicate key 后可以通过 `>5/S` 恢复事务状态。
- 无 `.bitfun` 等无关文件进入远端 diff。

### 14.2 主要风险

1. 运行时 libpq 不匹配。

   即使 ODBC 编译成功，如果部署环境加载旧 libpq，也会出问题。需要通过 `ldd` 和 `nm` 验证。

2. 第一条事务语句失败。

   ODBC 现有逻辑中，第一条开启事务的语句通常不会先建立内部 savepoint，这是继承既有 statement rollback 策略，不是本次协议替换新增行为。

3. readonly 优化。

   `opt_previous & SVPOPT_RDONLY` 会影响是否创建 savepoint。本次改动保持 ODBC 原有策略。如果需要和 JDBC `always/internal` 在只读语句上完全一致，需要单独评估现有 readonly 优化。

4. 协议发送失败。

   `PQsendProtocolSync()` 失败时，连接协议状态不可判定，当前实现通过 `CC_on_abort(self, CONN_DEAD)` 关闭连接，属于保守但合理的 fail-fast 行为。

### 14.3 不需要继续修改 libpq 的原因

libpq 当前接口已经符合需求：

- `PQsendSavepoint()` 只排队 create 包，不 flush。
- `PQsendRollbackToSavepoint()` 只排队 rollback 包。
- `PQsendProtocolSync()` 发送 Sync 并 flush。
- libpq 不生成虚假的 `PGRES_COMMAND_OK`。

ODBC 必须在自身状态机中维护：

```text
internal_op
internal_svp
rbonerr
transaction status
```

因此 ODBC 的静默包处理修复是正确边界，不应转嫁到 libpq。

## 15. 验收标准

### 15.1 功能验收

满足以下条件即认为功能通过：

1. 连接输出串包含：

```text
Autosave=internal
```

2. duplicate key 前有：

```text
>5/Q
```

3. duplicate key 后有：

```text
>5/S
```

4. rollback savepoint 后服务端返回：

```text
ReadyForQuery status=84
```

5. 下一条 INSERT 成功。

6. count 结果符合预期。

### 15.2 负向验收

以下情况不应出现：

```text
duplicate key 后发送 Q ROLLBACK
ReadyForQuery 长期保持 status=69
下一条 SQL 报 25P02
临时表因整事务 ROLLBACK 消失
```

### 15.3 工程验收

1. ODBC 仓库 diff 中无 `.bitfun`、`.claude` 等无关文件。
2. ODBC 可链接含新增接口的 libpq。
3. 未启用 `Autosave=internal` 的场景行为不变。
4. `ForExtensionConnector=1` 场景行为不变。

## 16. 结论

`Autosave=internal` 的核心不是新增一个 ODBC 上层语义，而是将 ODBC 现有内部 savepoint 机制的底层实现从 SQL 文本替换为专用协议包。

最终正确链路是：

```text
语句执行前:
    ODBC 本地判断需要 statement rollback
    libpq 排队 35...01
    ODBC 发送 SQL
    ODBC 本地标记 internal_svp=1

语句失败后:
    DiscardStatementSvp 发现 internal_svp=1
    CC_internal_rollback(PER_STATEMENT_ROLLBACK)
    libpq 发送 35...02 + Sync
    ODBC 等 ReadyForQuery status=84
    后续 SQL 继续执行
```

该设计与 JDBC 报文模型一致，同时保留 ODBC 原有 statement rollback 状态机。
