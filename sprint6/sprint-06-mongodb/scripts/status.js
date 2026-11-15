const hello = db.hello();
const status = rs.status();

printjson({
  replicaSet: hello.setName,
  connectedMember: hello.me,
  primary: hello.primary,
  isWritablePrimary: hello.isWritablePrimary,
  secondary: hello.secondary,
  electionId: hello.electionId
});

printjson(
  status.members.map((member) => ({
    name: member.name,
    state: member.stateStr,
    health: member.health,
    uptimeSeconds: member.uptime,
    lastApplied: member.optimeDate,
    syncSource: member.syncSourceHost
  }))
);
