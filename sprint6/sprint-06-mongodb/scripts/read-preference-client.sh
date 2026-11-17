#!/bin/sh
set -eu

: "${MONGO_REPLICA_SET:?MONGO_REPLICA_SET is required}"
: "${MONGO_APP_DATABASE:?MONGO_APP_DATABASE is required}"
: "${MONGO_APP_USERNAME:?MONGO_APP_USERNAME is required}"
: "${MONGO_APP_PASSWORD:?MONGO_APP_PASSWORD is required}"
: "${READ_PREFERENCE:?READ_PREFERENCE is required}"

exec mongosh \
  --quiet \
  "mongodb://${MONGO_APP_USERNAME}:${MONGO_APP_PASSWORD}@mongo1:27017,mongo2:27017,mongo3:27017/${MONGO_APP_DATABASE}?replicaSet=${MONGO_REPLICA_SET}&authSource=${MONGO_APP_DATABASE}&readPreference=${READ_PREFERENCE}" \
  "$@"
