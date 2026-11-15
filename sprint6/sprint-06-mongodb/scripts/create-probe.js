function requireEnvironmentVariable(name) {
  const value = process.env[name];

  if (!value) {
    throw new Error(`${name} is required`);
  }

  return value;
}

const applicationDatabaseName = requireEnvironmentVariable("MONGO_APP_DATABASE");
const applicationUsername = requireEnvironmentVariable("MONGO_APP_USERNAME");
const applicationDatabase = db.getSiblingDB(applicationDatabaseName);
const marker = `probe-${new Date().toISOString()}-${new ObjectId().toString()}`;

const result = applicationDatabase.replication_probe.insertOne(
  {
    marker,
    createdAt: new Date(),
    writer: applicationUsername
  },
  {
    writeConcern: {
      w: "majority",
      j: true,
      wtimeout: 5000
    }
  }
);

printjson({
  acknowledged: result.acknowledged,
  insertedId: result.insertedId,
  marker,
  primary: db.hello().primary,
  writeConcern: { w: "majority", j: true, wtimeout: 5000 }
});
