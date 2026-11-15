function requireEnvironmentVariable(name) {
  const value = process.env[name];

  if (!value) {
    throw new Error(`${name} is required`);
  }

  return value;
}

const applicationDatabaseName = requireEnvironmentVariable("MONGO_APP_DATABASE");
const applicationUsername = requireEnvironmentVariable("MONGO_APP_USERNAME");
const applicationPassword = requireEnvironmentVariable("MONGO_APP_PASSWORD");
const analystUsername = requireEnvironmentVariable("MONGO_ANALYST_USERNAME");
const analystPassword = requireEnvironmentVariable("MONGO_ANALYST_PASSWORD");
const applicationDatabase = db.getSiblingDB(applicationDatabaseName);

if (applicationDatabase.getUser(applicationUsername)) {
  applicationDatabase.updateUser(applicationUsername, {
    pwd: applicationPassword,
    roles: [{ role: "readWrite", db: applicationDatabaseName }]
  });
} else {
  applicationDatabase.createUser({
    user: applicationUsername,
    pwd: applicationPassword,
    roles: [{ role: "readWrite", db: applicationDatabaseName }]
  });
}

if (applicationDatabase.getUser(analystUsername)) {
  applicationDatabase.updateUser(analystUsername, {
    pwd: analystPassword,
    roles: [{ role: "read", db: applicationDatabaseName }]
  });
} else {
  applicationDatabase.createUser({
    user: analystUsername,
    pwd: analystPassword,
    roles: [{ role: "read", db: applicationDatabaseName }]
  });
}

const collectionName = "replication_probe";
const validator = {
  $jsonSchema: {
    bsonType: "object",
    required: ["marker", "createdAt", "writer"],
    properties: {
      marker: { bsonType: "string", minLength: 1 },
      createdAt: { bsonType: "date" },
      writer: { bsonType: "string", minLength: 1 }
    }
  }
};

if (applicationDatabase.getCollectionNames().includes(collectionName)) {
  applicationDatabase.runCommand({
    collMod: collectionName,
    validator,
    validationLevel: "strict",
    validationAction: "error"
  });
} else {
  applicationDatabase.createCollection(collectionName, {
    validator,
    validationLevel: "strict",
    validationAction: "error"
  });
}

applicationDatabase[collectionName].createIndex(
  { marker: 1 },
  { unique: true, name: "uq_replication_probe_marker" }
);

print(`Application database initialized: ${applicationDatabaseName}`);
