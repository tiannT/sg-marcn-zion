#!/bin/sh
set -e
cd /root
uname -a
/usr/sbin/mysqld --version
/usr/bin/mysqlslap --version
mkdir -p /tmp/zion-db/mysql/data
if test ! -d /tmp/zion-db/mysql/data/mysql; then
 if ! /usr/sbin/mysqld --defaults-file=/etc/zion-mysql.cnf --initialize-insecure > /tmp/zion-db/mysql/initialize.log 2>&1; then
  tail -n 100 /tmp/zion-db/mysql/initialize.log
  exit 1
 fi
fi
if /usr/bin/mysqladmin --no-defaults --socket=/tmp/zion-db/mysql/mysql.sock -uroot ping >/dev/null 2>&1; then
 echo 'A Guest MySQL server already owns the test socket; stop that test first' >&2
 exit 1
fi
owned_pid=
cleanup() {
 if test -n "$owned_pid"; then
  kill "$owned_pid" 2>/dev/null || :
  wait "$owned_pid" 2>/dev/null || :
 fi
}
trap cleanup EXIT
/usr/sbin/mysqld --defaults-file=/etc/zion-mysql.cnf > /tmp/zion-db/mysql/stdout.log 2>&1 &
owned_pid=$!
sleep 5
# Wait for server readiness before starting the benchmark.
ready=0
attempt=0
while test "$attempt" -lt 60; do
 if /usr/bin/mysqladmin --no-defaults --socket=/tmp/zion-db/mysql/mysql.sock -uroot ping 2>/tmp/zion-db/mysql/ping.stderr; then
  ready=1
  break
 fi
 if ! kill -0 "$owned_pid" 2>/dev/null; then
  echo 'MYSQL_SERVER_EXITED_BEFORE_READY'
  break
 fi
 attempt=$((attempt + 1))
 sleep 1
done
if test "$ready" -ne 1; then
 cat /tmp/zion-db/mysql/ping.stderr
 tail -n 100 /tmp/zion-db/mysql/stdout.log
 exit 1
fi
/usr/bin/mysql --no-defaults --socket=/tmp/zion-db/mysql/mysql.sock -uroot <<'SQL'
CREATE USER IF NOT EXISTS 'zion'@'127.0.0.1' IDENTIFIED BY 'zion-test-only';
GRANT ALL ON zion_slap.* TO 'zion'@'127.0.0.1';
SQL
(set +e; MYSQL_PWD=zion-test-only /usr/bin/mysqlslap --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 --user=zion --create-schema=zion_slap --auto-generate-sql --auto-generate-sql-load-type=mixed --concurrency=10 --iterations=5 --number-of-queries=1000; rc=$?; printf '%s\n' "$rc" > /tmp/zion-db/mysql/mysqlslap.rc) 2>&1 | tee mysql-benchmark.log
test "$(cat /tmp/zion-db/mysql/mysqlslap.rc)" -eq 0
/usr/bin/mysql --no-defaults --socket=/tmp/zion-db/mysql/mysql.sock -uroot -N -e 'SELECT VERSION(), @@innodb_buffer_pool_size, @@innodb_use_native_aio;'
/usr/bin/mysqladmin --no-defaults --socket=/tmp/zion-db/mysql/mysql.sock -uroot shutdown
wait "$owned_pid"
owned_pid=
