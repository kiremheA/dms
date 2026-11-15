#!/bin/sh
set -eu

: "${MONGO_REPLICA_SET:?MONGO_REPLICA_SET is required}"
: "${MONGO_APP_DATABASE:?MONGO_APP_DATABASE is required}"
: "${MONGO_APP_USERNAME:?MONGO_APP_USERNAME is required}"
: "${MONGO_APP_PASSWORD:?MONGO_APP_PASSWORD is required}"

exec mongosh \
  --quiet \
  --host "${MONGO_REPLICA_SET}/mongo1:27017,mongo2:27017,mongo3:27017" \
  --username "$MONGO_APP_USERNAME" \
  --password "$MONGO_APP_PASSWORD" \
  --authenticationDatabase "$MONGO_APP_DATABASE" \
  "$@"
