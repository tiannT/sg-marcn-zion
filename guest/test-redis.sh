#!/bin/sh
set -e
cd /root
uname -a
/usr/bin/busybox --help 2>&1 | /usr/bin/busybox head -n 1
/usr/local/redis/bin/redis-server --version
mkdir -p /tmp/redis-benchmark
if /usr/local/redis/bin/redis-cli -p 6379 PING >/dev/null 2>&1; then
 echo 'A Guest Redis server already owns port 6379; stop that test first' >&2
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
/usr/local/redis/bin/redis-server --bind 127.0.0.1 --port 6379 --save '' --appendonly no --maxmemory 64mb --dir /tmp/redis-benchmark > /tmp/redis-benchmark/server.log 2>&1 &
owned_pid=$!
sleep 2
if ! /usr/local/redis/bin/redis-cli -p 6379 PING; then
 tail -n 60 /tmp/redis-benchmark/server.log
 exit 1
fi
# Exercise the default full command set. Record the benchmark exit code,
# since tee alone would hide an application failure.
(set +e; /usr/local/redis/bin/redis-benchmark -h 127.0.0.1 -p 6379 -n 100000 -c 20 -r 10000 -d 256 -q; rc=$?; printf '%s\n' "$rc" > /tmp/redis-benchmark/benchmark.rc) 2>&1 | tee redis-benchmark.log
test "$(cat /tmp/redis-benchmark/benchmark.rc)" -eq 0
/usr/local/redis/bin/redis-cli -p 6379 INFO server | grep '^redis_version:'
/usr/local/redis/bin/redis-cli -p 6379 shutdown nosave
wait "$owned_pid"
owned_pid=
