function requireEnvironmentVariable(name) {
  const value = process.env[name];

  if (!value) {
    throw new Error(`${name} is required`);
  }

  return value;
}

const applicationDatabaseName = requireEnvironmentVariable("MONGO_APP_DATABASE");
const applicationDatabase = db.getSiblingDB(applicationDatabaseName);
const collection = applicationDatabase.replication_probe;
const duplicateMarkers = collection.aggregate([
  { $group: { _id: "$marker", count: { $sum: 1 } } },
  { $match: { count: { $gt: 1 } } }
]).toArray();

printjson({
  database: applicationDatabaseName,
  documentCount: collection.countDocuments({}),
  duplicateMarkers,
  indexes: collection.getIndexes()
});
