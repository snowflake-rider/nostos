import { createCliRenderer } from "@opentui/core";
import { demoReport } from "./report";
import { startScan } from "./scanner";
import { createDashboard } from "./ui";

const args = process.argv.slice(2);
let demo = false, noConnect = false, port = 8787;
let error = "";
for (let i = 0; i < args.length; i++) {
  if (args[i] === "--demo") demo = true;
  else if (args[i] === "--no-connect") noConnect = true;
  else if (args[i] === "--port") port = Number(args[++i]);
  else error = "알 수 없는 옵션입니다. --help를 확인하세요.";
}
if (!Number.isInteger(port) || port < 1 || port > 65535) error = "port는 1..65535여야 합니다.";
if (!process.stdin.isTTY || !process.stdout.isTTY) error = "대화형 터미널에서 실행하세요. 자동화는 scripts/esp32-scan --json을 사용하세요.";
if (error) {
  process.stderr.write(error + "\n");
  process.exitCode = 2;
} else {
  const renderer = await createCliRenderer({ exitOnCtrlC: false, useMouse: false,
    targetFps: 15, maxFps: 30, backgroundColor: "#101722" });
  const close = () => renderer.destroy();
  process.on("SIGINT", close); process.on("SIGTERM", close);
  renderer.on("destroy", () => { process.off("SIGINT", close); process.off("SIGTERM", close); });
  const dashboard = createDashboard(renderer, demo
    ? () => ({ promise: Promise.resolve(demoReport()), cancel() {} })
    : () => startScan({ port, noConnect }), demo);
  await dashboard.refresh();
}
