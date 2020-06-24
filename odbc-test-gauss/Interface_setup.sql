--Author:o00231432 Date:2013-07-27
--update the jdbcconfig.properties
/*
\! sed -i '/user/d' sql/MPP_NEW_FEATURE/Interface/JDBC/lib/jdbcconfig.properties
\! sed -i '/^port/d' sql/MPP_NEW_FEATURE/Interface/JDBC/lib/jdbcconfig.properties
\! sed -i "/dbname/a user=$USER" sql/MPP_NEW_FEATURE/Interface/JDBC/lib/jdbcconfig.properties
\! port=`sed -n '/^port =/p' $GAUSSCN/postgresql.conf |awk -F "=" '{print $2}' |awk '{print $1}'`;sed -i "/dbname/a port=$port" sql/MPP_NEW_FEATURE/Interface/JDBC/lib/jdbcconfig.properties
--update the jdbcboolconfig.properties
\! sed -i '/user/d' sql/MPP_NEW_FEATURE/Interface/JDBC/lib/jdbcboolconfig.properties
\! sed -i '/^port/d' sql/MPP_NEW_FEATURE/Interface/JDBC/lib/jdbcboolconfig.properties
\! sed -i "/dbname/a user=$USER" sql/MPP_NEW_FEATURE/Interface/JDBC/lib/jdbcboolconfig.properties
\! port=`sed -n '/^port =/p' $GAUSSCN/postgresql.conf |awk -F "=" '{print $2}' |awk '{print $1}'`;sed -i "/dbname/a port=$port" sql/MPP_NEW_FEATURE/Interface/JDBC/lib/jdbcboolconfig.properties
create schema FVT_INTERFACE;
--update the odbc.ini and odbcinst.ini
\! sed -i '/Username/d' sql/MPP_NEW_FEATURE/Interface/ODBC/lib/odbc.ini
\! sed -i '/^port/d' sql/MPP_NEW_FEATURE/Interface/ODBC/lib/odbc.ini
\! sed -i "/Database/a Username=$USER" sql/MPP_NEW_FEATURE/Interface/ODBC/lib/odbc.ini
\! port=`sed -n '/^port =/p' $GAUSSCN/postgresql.conf |awk -F "=" '{print $2}' |awk '{print $1}'`;sed -i "/Password/a port=$port" sql/MPP_NEW_FEATURE/Interface/ODBC/lib/odbc.ini
\! sed -i '/^Driver64/d' sql/MPP_NEW_FEATURE/Interface/ODBC/lib/odbcinst.ini
\! sed -i '/^setup/d' sql/MPP_NEW_FEATURE/Interface/ODBC/lib/odbcinst.ini
\! cd sql/MPP_NEW_FEATURE/Interface/ODBC/lib;odbcpath=`pwd`;sed -i "/Driver/a Driver64=$odbcpath/psqlodbcw.so" odbcinst.ini
\! cd sql/MPP_NEW_FEATURE/Interface/ODBC/lib;odbcpath=`pwd`;sed -i "/Driver64/a setup=$odbcpath/psqlodbcw.so" odbcinst.ini
--prepared for TSjdbc3SavePointTest
\! java junit.textui.TestRunner -m Graft.TSjdbc3SavePointTest.TSjdbc3SavePointTest_pre_000
--prepared for testResultSetTest
\! java junit.textui.TestRunner -m Graft.testResultSetTest.testResultSet_pre_000
-- This file creates some tables to be used in the tests
*/
create schema info_dev_sys_view;

create schema FVT_INTERFACE;
CREATE TABLE testtab1 (id integer, t varchar(20));
INSERT INTO testtab1 VALUES (1, 'foo');
INSERT INTO testtab1 VALUES (2, 'bar');
INSERT INTO testtab1 VALUES (3, 'foobar');

CREATE TABLE byteatab (id integer, t bytea);
INSERT INTO byteatab VALUES (1, E'\\001\\002\\003\\004\\005\\006\\007\\010'::bytea);
INSERT INTO byteatab VALUES (2, 'bar');
INSERT INTO byteatab VALUES (3, 'foobar');
INSERT INTO byteatab VALUES (4, 'foo');
INSERT INTO byteatab VALUES (5, 'barf');

CREATE TABLE intervaltable(id integer, iv interval, d varchar(100));
INSERT INTO intervaltable VALUES (1, '1 day', 'one day');
INSERT INTO intervaltable VALUES (2, '10 seconds', 'ten secs');
INSERT INTO intervaltable VALUES (3, '100 years', 'hundred years');

CREATE TABLE booltab (id integer, t varchar(5), b boolean);
INSERT INTO booltab VALUES (1, 'yeah', true);
INSERT INTO booltab VALUES (2, 'yes', true);
INSERT INTO booltab VALUES (3, 'true', true);
INSERT INTO booltab VALUES (4, 'false', false);
INSERT INTO booltab VALUES (5, 'not', false);
