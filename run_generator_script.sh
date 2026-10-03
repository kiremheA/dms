#!/bin/bash
#   запуск ./run.sh <dev|load> <имя_бд> [seed]
set -e
MODE=$1; DB=$2; SEED=${3:-67}
cd "$(dirname "$0")"
OUT=out_$MODE

g++ -O2 -o gen main.cpp
mkdir -p $OUT
./gen $SEED $OUT $MODE

dropdb --if-exists --force $DB
createdb $DB
for f in ../migrations/[1-7]_*.sql; do
  psql -d $DB -v ON_ERROR_STOP=1 -q -f $f
done


for t in device_category repair_norm work_service staff master_specialization customer device part \
         service_order order_status_history malfunction master_assignment diagnosis estimate \
         estimate_version estimate_line part_movement work invoice payment; do
  psql -d $DB -v ON_ERROR_STOP=1 -q -c "\copy $t FROM $OUT/$t.csv WITH (FORMAT csv, DELIMITER ';')"
done

psql -d $DB -v ON_ERROR_STOP=1 -q -f $OUT/setval.sql > /dev/null