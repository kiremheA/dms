const database = db.getSiblingDB(process.env.MONGO_APP_DATABASE);
const result = database.runCommand({
  find: "replication_probe",
  filter: {},
  limit: 20,
  readConcern: { level: "majority" }
});

printjson({
  readPreference: process.env.READ_PREFERENCE,
  connectedMember: db.hello().me,
  returnedDocuments: result.cursor.firstBatch.length
});
