const database = db.getSiblingDB(process.env.MONGO_APP_DATABASE);
const primaryBefore = db.hello().primary;
const marker = `retryable-probe-${new Date().toISOString()}-${new ObjectId()}`;

printjson({ phase: "before-stop", primary: primaryBefore });
sleep(5000);

const result = database.replication_probe.insertOne(
  { marker, createdAt: new Date(), writer: process.env.MONGO_APP_USERNAME },
  { writeConcern: { w: "majority", j: true, wtimeout: 5000 } }
);

printjson({
  phase: "after-election",
  retryWrites: true,
  acknowledged: result.acknowledged,
  marker,
  primary: db.hello().primary
});
