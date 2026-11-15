const replicaSetName = process.env.MONGO_REPLICA_SET;

if (!replicaSetName) {
  throw new Error("MONGO_REPLICA_SET is required");
}

let replicaSetInitialized = true;

try {
  rs.status();
} catch (error) {
  if (error.code === 94 || error.codeName === "NotYetInitialized") {
    replicaSetInitialized = false;
  } else {
    throw error;
  }
}

if (!replicaSetInitialized) {
  const result = rs.initiate({
    _id: replicaSetName,
    members: [
      { _id: 0, host: "mongo1:27017", priority: 2 },
      { _id: 1, host: "mongo2:27017", priority: 1 },
      { _id: 2, host: "mongo3:27017", priority: 1 }
    ]
  });

  print("Replica set initialization requested:");
  printjson(result);
} else {
  print("Replica set is already initialized.");
}
