export ODBCSYSINI=`pwd`
export ODBCINI=`pwd`/odbc.ini
CODE_DIR=`pwd`/../../
DRIVER_FILE=`pwd`/../.libs/psqlodbcw.so

retcode=0

export LD_LIBRARY_PATH=${CODE_DIR}/Code/src/interfaces/libpq:${CODE_DIR}/mppdb_temp_install/lib:$LD_LIBRARY_PATH
export PATH=${CODE_DIR}/mppdb_temp_install/bin:${CODE_DIR}/Code/src/bin/psql:$PATH

cd ${CODE_DIR}/clienttools/
source sslcert_env.sh
cd - 

rm -rf log/*.log
mkdir -p  log
rm -rf regression.diff

echo "
ServerName = $HOST
Port       = $PORT
UserName   = $USER
Password   = $PASSWORD
"

if [ X"$HOST" == X"" -o X"PORT" == X"" -o X"$USER" == X"" -o X"$PASSWORD" == X"" ]; then
	echo "ERROR: Please declare HOST, PORT, USER, PASSWORD by environment variables" >&2;
	exit 1;
fi

echo "
[gaussdb]
Driver=$DRIVER_FILE
servername=$HOST
port=$PORT
database=odbc
username=$USER
password=$PASSWORD
sslmode=allow
Fetch=100
ForExtensionConnector=0
UseBatchProtocol=1
"  > odbc.ini

gsql -U $USER -d postgres -h $HOST -p $PORT -W $PASSWORD -d postgres -c "drop database odbc;" >/dev/null 2>&1 
gsql -U $USER -d postgres -h $HOST -p $PORT -W $PASSWORD -d postgres -c "create database odbc DBCOMPATIBILITY='ORA';"
gsql -U $USER -d postgres -h $HOST -p $PORT -W $PASSWORD -d odbc -f Interface_setup.sql 

function doing()
{
	length_of_line=50
	printf "$1 ";
	printf "$1 " >> results.log ;
	for ((i=${#1};i<$length_of_line;i++)); do 
		printf '.';
		printf '.' >> results.log ;
	done;
	printf " ";
	printf " " >> results.log ;
}
function test_case()
{
	doing $1 ;
	printf "testing"
	./$1 > ./log/$1.log 2>&1;
	ret=$?
	echo "exit code : ${ret}" >> ./log/$1.log	
	diff -wC3 ./expected/$1.log ./log/$1.log  >> regression.diff 2>&1
	diff_ret=$?
	printf "\b\b\b\b\b\b\b       \b\b\b\b\b\b\b"
	if [ $diff_ret -eq 0 -a $ret -eq 0 ]; then
		printf "ok";
		printf "ok" >> results.log;
	else
		printf "failed    ";
		printf "failed" >> results.log;
		
		# To tell make that there is something wrong.
		retcode=1
	fi
	echo '';
	echo '' >> results.log;
}

echo '' > results.log 

for line in $*; do
	if [ -f "$line" ]; then
		chmod a+x $line
		test_case $line
	fi
done

exit $retcode

