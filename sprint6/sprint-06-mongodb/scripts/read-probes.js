function requireEnvironmentVariable(name) {
  const value = process.env[name];

  if (!value) {
    throw new Error(`${name} is required`);
  }

  return value;
}

const applicationDatabaseName = requireEnvironmentVariable("MONGO_APP_DATABASE");
const applicationDatabase = db.getSiblingDB(applicationDatabaseName);

db.getMongo().setReadPref("secondaryPreferred");

const result = applicationDatabase.runCommand({
  find: "replication_probe",
  filter: {},
  sort: { createdAt: -1 },
  limit: 20,
  readConcern: { level: "majority" }
});

printjson({
  readPreference: "secondaryPreferred",
  readConcern: "majority",
  returnedDocuments: result.cursor.firstBatch.length,
  documents: result.cursor.firstBatch
});
