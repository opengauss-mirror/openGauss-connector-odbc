export ODBCSYSINI=`pwd`
export ODBCINI=`pwd`/odbc.ini
. ~/workspace/git/Private2/clienttools/sslcert_env.sh
rm -rf log
mkdir log
rm -rf regression.diff

host=`grep -nri Servername 'odbc.ini' | grep -ve ';' | awk -F '=' '{print $2}'`
port=`grep -nri port 'odbc.ini' | grep -ve ';' | awk -F '=' '{print $2}'`
gs_path=/data1/luole/workspace/git/Private3/GAUSS200_OLAP_TRUNK/mppdb_temp_install/
user=`grep -nri username 'odbc.ini' | grep -ve ';' | awk -F '=' '{print $2}'`
passwd=`grep -nri Password 'odbc.ini' | grep -ve ';' | awk -F '=' '{print $2}'`

export LD_LIBRARY_PATH=$gs_path/lib:$LD_LIBRARY_PATH
export PATH=$gs_path/bin:$PATH

gsql -U $user -d postgres -h $host -p $port -W $passwd -d postgres -c "drop database odbc;" >/dev/null 2>&1 
gsql -U $user -d postgres -h $host -p $port -W $passwd -d postgres -c "create database odbc;"
gsql -U $user -d postgres -h $host -p $port -W $passwd -d odbc -f Interface_setup.sql 

function doing()
{
	length_of_line=50
	printf "$1 ";
	for ((i=${#1};i<$length_of_line;i++)); do 
		printf '.';
	done;
	printf " "
}
function test_case()
{
	doing $1 ;
	./$1 > ./log/$1.log 2>&1;
	
	#diff -wC3 ./log/$1.log  ./expected/$1.log >> regression.diff 2>&1
	failed=`grep -E '\<failed\>' ./log/$1.log`
	if [ -z "$failed" ]; then
		printf "done";
	else
		printf "failed";
	fi
	
	echo '';
}

#for line in `cat tests.list`; do
#	chmod a+x $line
#	test_case $line
#done

#echo "make sure that you have executed Interface_setup.sql"

