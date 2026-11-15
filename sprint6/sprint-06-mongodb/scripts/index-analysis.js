const database = db.getSiblingDB(
  process.env.MONGO_APP_DATABASE || "sprint6_index_benchmark"
);
const fixtureCount = 50000;
const measurementRuns = 5;

const collections = {
  devices: database.getCollection("benchmark_device_parameters"),
  intakes: database.getCollection("benchmark_order_intake_parameters")
};

const queries = [
  {
    id: "Q1",
    title: "Профили ноутбуков с памятью 256 ГБ",
    collection: collections.devices,
    filter: { category_id: NumberInt(1), "parameters.memory_gb": NumberInt(256) },
    index: {
      key: { category_id: 1, "parameters.memory_gb": 1 },
      name: "category_memory_gb"
    }
  },
  {
    id: "Q2",
    title: "Приёмы с повреждённым экраном",
    collection: collections.intakes,
    filter: { "reception_parameters.screen_condition": "cracked" },
    index: {
      key: { "reception_parameters.screen_condition": 1 },
      name: "screen_condition"
    }
  }
];

function insertFixture() {
  const deviceBatch = [];
  const intakeBatch = [];

  for (let number = 1; number <= fixtureCount; number += 1) {
    const categoryNumber = ((number - 1) % 3) + 1;
    const categoryId = NumberInt(categoryNumber);
    const memoryGb = NumberInt([128, 256, 512][Math.floor((number - 1) / 3) % 3]);
    const profileId = new ObjectId();
    const screenCondition = number % 5 === 0 ? "cracked" : "intact";

    deviceBatch.push({
      _id: profileId,
      device_id: NumberInt(number),
      category_id: categoryId,
      parameters: categoryNumber === 1
        ? { model: `Notebook ${number}`, memory_gb: memoryGb }
        : { model: `Device ${number}` }
    });
    intakeBatch.push({
      service_order_id: NumberInt(number),
      device_profile_id: profileId,
      reception_parameters: { screen_condition: screenCondition }
    });

    if (deviceBatch.length === 1000) {
      collections.devices.insertMany(deviceBatch);
      collections.intakes.insertMany(intakeBatch);
      deviceBatch.length = 0;
      intakeBatch.length = 0;
    }
  }

  if (deviceBatch.length > 0) {
    collections.devices.insertMany(deviceBatch);
    collections.intakes.insertMany(intakeBatch);
  }
}

function singleExecutionStats(query) {
  const explain = query.collection.find(query.filter).explain("executionStats");
  const stats = explain.executionStats;

  return {
    winningPlan: explain.queryPlanner.winningPlan,
    executionTimeMillis: stats.executionTimeMillis,
    totalDocsExamined: stats.totalDocsExamined,
    totalKeysExamined: stats.totalKeysExamined,
    nReturned: stats.nReturned
  };
}

function median(values) {
  const sorted = [...values].sort((left, right) => left - right);
  return sorted[Math.floor(sorted.length / 2)];
}

function measure(query) {
  const runs = [];

  for (let run = 0; run < measurementRuns; run += 1) {
    runs.push(singleExecutionStats(query));
  }

  return {
    winningPlan: runs[0].winningPlan,
    executionTimeMillis: runs.map(({ executionTimeMillis }) => executionTimeMillis),
    medianExecutionTimeMillis: median(
      runs.map(({ executionTimeMillis }) => executionTimeMillis)
    ),
    totalDocsExamined: runs[0].totalDocsExamined,
    totalKeysExamined: runs[0].totalKeysExamined,
    nReturned: runs[0].nReturned
  };
}

for (const collection of Object.values(collections)) {
  collection.drop();
}

collections.devices.createIndex({ device_id: 1 }, { unique: true, name: "device_id_unique" });
collections.intakes.createIndex(
  { service_order_id: 1 },
  { unique: true, name: "service_order_id_unique" }
);
collections.intakes.createIndex({ device_profile_id: 1 }, { name: "device_profile_id" });
insertFixture();

const result = {
  fixture: {
    database: database.getName(),
    collections: [collections.devices.getName(), collections.intakes.getName()],
    documentsPerCollection: fixtureCount,
    measurementRuns
  },
  queries: {}
};

for (const query of queries) {
  result.queries[query.id] = {
    title: query.title,
    collection: query.collection.getName(),
    filter: query.filter,
    before: measure(query)
  };
}

for (const query of queries) {
  query.collection.createIndex(query.index.key, { name: query.index.name });
  result.queries[query.id].index = query.index;
  result.queries[query.id].after = measure(query);
}

print(JSON.stringify(result, null, 2));
