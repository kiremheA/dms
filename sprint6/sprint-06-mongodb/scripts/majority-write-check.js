const database = db.getSiblingDB(process.env.MONGO_APP_DATABASE);

try {
  database.replication_probe.insertOne(
    {
      marker: `majority-probe-${new Date().toISOString()}-${new ObjectId()}`,
      createdAt: new Date(),
      writer: process.env.MONGO_APP_USERNAME
    },
    { writeConcern: { w: "majority", j: true, wtimeout: 5000 } }
  );
  printjson({ acknowledged: true });
} catch (error) {
  printjson({ acknowledged: false, error: error.message });
}
