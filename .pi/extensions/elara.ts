/**
 * Safe pi status extension for Elara Buddy.
 *
 * It forwards lifecycle state only. Tool arguments and permission decisions
 * stay inside pi; the device is not an authorization authority for pi.
 */
import {
  buildSessionReport,
  createElaraBridge,
  createSessionState,
  elaraConfig,
} from "../../tools/elara_bridge.mjs";

export default function elaraExtension(pi) {
  const state = createSessionState();
  const bridge = createElaraBridge(elaraConfig());
  const report = () => bridge.send(buildSessionReport(state));

  function working(message = "working...") {
    state.running = 3;
    state.waiting = 0;
    state.completed = false;
    state.msg = message;
    void report();
  }

  function done() {
    state.running = 0;
    state.waiting = 0;
    state.completed = true;
    state.msg = "done!";
    void report();
  }

  bridge.startHeartbeat(report, 15000);

  pi.on("session_start", () => {
    state.completed = false;
    state.msg = "pi online";
    void report();
  });

  pi.on("before_agent_start", () => working());
  pi.on("tool_call", () => working("tool running"));
  pi.on("tool_result", () => void report());
  pi.on("agent_settled", () => done());
  pi.on("session_shutdown", () => {
    bridge.close();
  });
}
