#!/bin/sh
set -eu

: "${MONGO_REPLICA_SET:?MONGO_REPLICA_SET is required}"
: "${MONGO_ROOT_USERNAME:?MONGO_ROOT_USERNAME is required}"
: "${MONGO_ROOT_PASSWORD:?MONGO_ROOT_PASSWORD is required}"

exec mongosh \
  --quiet \
  --host "${MONGO_REPLICA_SET}/mongo1:27017,mongo2:27017,mongo3:27017" \
  --username "$MONGO_ROOT_USERNAME" \
  --password "$MONGO_ROOT_PASSWORD" \
  --authenticationDatabase admin \
  "$@"
