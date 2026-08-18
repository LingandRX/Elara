/**
 * Safe Codex status hook for Elara Buddy.
 *
 * It intentionally does not forward tool_input, command text, transcript
 * content, or permission decisions to the device. Codex keeps ownership of
 * its own approval flow.
 */
import {
  buildSessionReport,
  createElaraBridge,
  createSessionState,
  elaraConfig,
} from "./elara_bridge.mjs";

function readStdin() {
  return new Promise((resolve, reject) => {
    let input = "";
    process.stdin.setEncoding("utf8");
    process.stdin.on("data", (chunk) => { input += chunk; });
    process.stdin.on("end", () => resolve(input));
    process.stdin.on("error", reject);
  });
}

async function main() {
  let input = {};
  try {
    input = JSON.parse((await readStdin()) || "{}");
  } catch {
    return;
  }

  const state = createSessionState();
  switch (input.hook_event_name) {
    case "SessionStart":
      state.msg = "codex online";
      break;
    case "UserPromptSubmit":
      state.running = 3;
      state.msg = "working...";
      break;
    case "PreToolUse":
      state.running = 3;
      state.msg = "tool running";
      break;
    case "PostToolUse":
      state.running = 3;
      state.msg = "tool done";
      break;
    case "Stop":
      state.completed = true;
      state.msg = "done!";
      break;
    case "SessionEnd":
      state.msg = "idle";
      break;
    default:
      return;
  }

  const bridge = createElaraBridge(elaraConfig());
  await bridge.send(buildSessionReport(state));
  bridge.close();
}

main().catch((error) => {
  process.stderr.write(`[elara/codex] ${error.message}\n`);
  process.exitCode = 1;
});
