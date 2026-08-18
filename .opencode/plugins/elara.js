/**
 * OpenCode -> Elara Buddy adapter.
 *
 * The transport is shared with the Codex hooks and pi extension. OpenCode
 * remains the only adapter that calls the OpenCode permission REST endpoint.
 */
import { readFileSync } from "node:fs";
import {
  buildSessionReport,
  createElaraBridge,
  createSessionState,
  elaraConfig,
  truncate,
} from "../../tools/elara_bridge.mjs";

export const ElaraPlugin = async ({ client }) => {
  const config = elaraConfig();
  const DEBUG = config.debug;

  function log(level, message, extra) {
    try {
      Promise.resolve(client.app.log({
        body: { service: "elara", level, message, extra },
      })).catch(() => {});
    } catch {
      // Logging must never break the bridge.
    }
  }

  const debug = (message) => DEBUG && log("debug", message);
  let lastWarningAt = 0;
  function warnThrottled(message, intervalMs = 60000) {
    const now = Date.now();
    if (now - lastWarningAt < intervalMs) return;
    lastWarningAt = now;
    log("warn", message);
  }

  const state = createSessionState();
  const pending = new Map();
  const promptQueue = [];
  const seen = new Map();
  let sequence = 0;
  let prompt = null;
  let resetTimer = null;
  let zenUsage = { rolling: -1, weekly: -1, monthly: -1 };
  let zenUsageAt = 0;

  const bridge = createElaraBridge({
    ...config,
    log: (level, message) => {
      if (level === "debug") debug(message);
      else warnThrottled(message);
    },
  });

  function report() {
    void bridge.send(buildSessionReport(state, prompt, zenUsage));
  }

  function cancelResetTimer() {
    if (resetTimer) clearTimeout(resetTimer);
    resetTimer = null;
  }

  function busy(message) {
    cancelResetTimer();
    state.running = 3;
    state.completed = false;
    state.msg = truncate(message || "working...", 23);
    report();
  }

  function done() {
    cancelResetTimer();
    state.running = 0;
    state.waiting = promptQueue.length > 0 ? 1 : 0;
    state.completed = true;
    state.msg = "done!";
    report();
    resetTimer = setTimeout(() => {
      resetTimer = null;
      state.completed = false;
      state.msg = "idle";
      report();
    }, 4000);
  }

  function fail(message = "error!") {
    cancelResetTimer();
    state.running = 0;
    state.waiting = promptQueue.length > 0 ? 1 : 0;
    state.completed = false;
    state.msg = message;
    report();
  }

  function normalizePermission(properties) {
    const p = properties || {};
    if (!p.id) return null;

    const rawType = p.type || p.permission || p.title || "tool";
    const rawPattern = p.pattern ?? p.patterns ?? "";
    const pattern = Array.isArray(rawPattern) ? rawPattern.join(" ") : String(rawPattern || "");
    const tool = truncate(rawType, 18);
    const hint = truncate(`${p.title && p.title !== rawType ? `${p.title} ` : ""}${pattern || rawType}`, 42);
    return {
      permissionID: p.id,
      sessionID: p.sessionID || p.sessionId,
      prompt: { id: `oc_${++sequence}`, tool, hint },
    };
  }

  function activateNextPrompt() {
    const activeId = promptQueue[0];
    prompt = activeId ? pending.get(activeId)?.prompt || null : null;
    state.waiting = prompt ? 1 : 0;
    if (prompt) {
      state.completed = false;
      state.msg = "approval";
    }
    report();
  }

  function removePermission(id) {
    const record = pending.get(id);
    if (!record) return;
    pending.delete(id);
    const index = promptQueue.indexOf(id);
    if (index >= 0) promptQueue.splice(index, 1);
    activateNextPrompt();
  }

  async function replyPermission(message) {
    const record = pending.get(message.id);
    if (!record || record.inFlight) return;
    record.inFlight = true;

    try {
      await client.postSessionIdPermissionsPermissionId({
        path: { id: record.sessionID, permissionID: record.permissionID },
        body: { response: message.decision === "once" ? "once" : "reject" },
      });
      removePermission(message.id);
      debug(`permission replied: ${message.decision}`);
    } catch (error) {
      record.inFlight = false;
      log("error", `permission.reply: ${error.message}`);
    }
  }

  function addTokens(info) {
    const tokens = info?.tokens;
    if (!tokens) return false;
    const total = (tokens.input || 0) + (tokens.output || 0) + (tokens.reasoning || 0)
      + (tokens.cache?.read || 0) + (tokens.cache?.write || 0);
    if (total <= 0) return false;

    const messageId = info.id || "";
    const previous = messageId ? (seen.get(messageId) || 0) : 0;
    if (total <= previous) return false;
    state.tokens_today += total - previous;
    if (messageId) seen.set(messageId, total);
    return true;
  }

  function zenKey() {
    if (process.env.ELARA_ZEN_KEY) return process.env.ELARA_ZEN_KEY;
    try {
      const home = process.env.HOME || process.env.USERPROFILE || "";
      const dataHome = process.env.XDG_DATA_HOME || `${home}/.local/share`;
      const auth = JSON.parse(readFileSync(`${dataHome}/opencode/auth.json`, "utf8"));
      return auth?.["opencode-go"]?.key || auth?.opencode?.key || null;
    } catch {
      return null;
    }
  }

  async function fetchZenUsage() {
    const key = zenKey();
    if (!key || Date.now() - zenUsageAt < 30000) return;

    try {
      const response = await fetch("https://opencode.ai/zen/go/v1/usage", {
        headers: { authorization: `Bearer ${key}` },
        signal: AbortSignal.timeout(8000),
      });
      if (!response.ok) return;
      const usage = (await response.json())?.usage;
      if (!usage) return;
      if (typeof usage.rolling?.percent === "number") zenUsage.rolling = usage.rolling.percent;
      if (typeof usage.weekly?.percent === "number") zenUsage.weekly = usage.weekly.percent;
      if (typeof usage.monthly?.percent === "number") zenUsage.monthly = usage.monthly.percent;
      zenUsageAt = Date.now();
    } catch {
      // Usage is optional; the device still receives session state.
    }
  }

  bridge.onMessage((message) => {
    if (message?.cmd === "permission") void replyPermission(message);
  });

  // The initial report is sent immediately; Zen usage is updated opportunistically.
  bridge.startHeartbeat(report, 15000);
  void fetchZenUsage().then(report);

  return {
    event: async ({ event }) => {
      const properties = event.properties || {};

      switch (event.type) {
        case "session.idle":
          seen.clear();
          done();
          break;

        case "session.error":
          fail("error!");
          break;

        case "session.status": {
          const status = typeof properties.status === "string"
            ? properties.status
            : properties.status?.type;
          if (status === "busy" || status === "running") busy("working...");
          else if (status === "retry") {
            state.running = 0;
            state.waiting = 1;
            state.completed = false;
            state.msg = "retrying...";
            report();
          }
          break;
        }

        // Some OpenCode versions expose the v2 name; keeping both costs nothing.
        case "permission.updated":
        case "permission.asked": {
          const record = normalizePermission(properties);
          if (!record || [...pending.values()].some((p) => p.permissionID === record.permissionID)) break;
          pending.set(record.prompt.id, record);
          promptQueue.push(record.prompt.id);
          activateNextPrompt();
          debug(`permission asked: ${record.prompt.tool} -> ${record.prompt.id}`);
          break;
        }

        case "permission.replied": {
          const permissionID = properties.permissionID || properties.requestID;
          if (permissionID) {
            for (const [id, record] of pending) {
              if (record.permissionID === permissionID) removePermission(id);
            }
          } else {
            prompt = null;
            report();
          }
          break;
        }

        case "message.updated":
          if (addTokens(properties.info)) report();
          break;
      }
    },

    "tool.execute.before": async (input) => {
      busy(`tool: ${input.tool || ""}`);
    },

    // Completion is driven by session.idle to avoid celebrating every tool in a multi-tool turn.
    "tool.execute.after": async () => {
      if (!state.completed) report();
    },

    dispose: async () => {
      cancelResetTimer();
      bridge.close();
    },
  };
};
