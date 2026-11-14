const definitions = [
  { name: "model", scope: "device", type: "string", required: true, category_ids: [NumberInt(1), NumberInt(2), NumberInt(3), NumberInt(4), NumberInt(5)] },
  { name: "os", scope: "device", type: "string", required: false, category_ids: [NumberInt(2)] },
  { name: "memory_gb", scope: "device", type: "number", required: false, category_ids: [NumberInt(1)] },
  { name: "screen_condition", scope: "intake", type: "string", required: true, category_ids: [NumberInt(1)] }
];

for (const definition of definitions) {
  db.parameter_definitions.updateOne(
    { scope: definition.scope, name: definition.name },
    { $set: definition },
    { upsert: true }
  );
}
