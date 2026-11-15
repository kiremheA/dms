#!/bin/sh
set -eu

: "${MONGO_REPLICA_SET:?MONGO_REPLICA_SET is required}"
: "${MONGO_ROOT_USERNAME:?MONGO_ROOT_USERNAME is required}"
: "${MONGO_ROOT_PASSWORD:?MONGO_ROOT_PASSWORD is required}"
: "${MONGO_APP_DATABASE:?MONGO_APP_DATABASE is required}"
: "${MONGO_APP_USERNAME:?MONGO_APP_USERNAME is required}"
: "${MONGO_APP_PASSWORD:?MONGO_APP_PASSWORD is required}"
: "${MONGO_ANALYST_USERNAME:?MONGO_ANALYST_USERNAME is required}"
: "${MONGO_ANALYST_PASSWORD:?MONGO_ANALYST_PASSWORD is required}"

direct_mongosh() {
  mongosh \
    --quiet \
    --host mongo1:27017 \
    --username "$MONGO_ROOT_USERNAME" \
    --password "$MONGO_ROOT_PASSWORD" \
    --authenticationDatabase admin \
    "$@"
}

replica_set_mongosh() {
  mongosh \
    --quiet \
    --host "${MONGO_REPLICA_SET}/mongo1:27017,mongo2:27017,mongo3:27017" \
    --username "$MONGO_ROOT_USERNAME" \
    --password "$MONGO_ROOT_PASSWORD" \
    --authenticationDatabase admin \
    "$@"
}

echo "Waiting for authenticated access to mongo1..."
attempt=0
until direct_mongosh --eval 'quit(db.adminCommand({ ping: 1 }).ok ? 0 : 2)' >/dev/null 2>&1; do
  attempt=$((attempt + 1))
  if [ "$attempt" -ge 60 ]; then
    echo "mongo1 did not accept authenticated connections in time." >&2
    exit 1
  fi
  sleep 2
done

direct_mongosh --file /scripts/init-replica-set.js

echo "Waiting for a writable primary..."
attempt=0
until replica_set_mongosh --eval 'quit(db.hello().isWritablePrimary ? 0 : 2)' >/dev/null 2>&1; do
  attempt=$((attempt + 1))
  if [ "$attempt" -ge 60 ]; then
    echo "Replica set did not elect a primary in time." >&2
    exit 1
  fi
  sleep 2
done

replica_set_mongosh --file /scripts/init-users.js
replica_set_mongosh --file /scripts/status.js
