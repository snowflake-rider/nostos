const children = [
  Bun.spawn(["bun", "run", "src/web-server.ts"], {
    stdin: "inherit",
    stdout: "inherit",
    stderr: "inherit",
  }),
  Bun.spawn(["bun", "x", "vite", "--config", "web/vite.config.ts"], {
    stdin: "inherit",
    stdout: "inherit",
    stderr: "inherit",
  }),
];

let closing = false;
function close(): void {
  if (closing) return;
  closing = true;
  for (const child of children) {
    if (child.exitCode === null) child.kill("SIGTERM");
  }
}

process.once("SIGINT", close);
process.once("SIGTERM", close);

const exitCode = await Promise.race(children.map((child) => child.exited));
close();
await Promise.all(children.map((child) => child.exited));
if (exitCode !== 0 && exitCode !== 143) process.exitCode = exitCode;
