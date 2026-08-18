/**
 * Shared Elara transport and wire-format helpers.
 *
 * This module deliberately uses node:net instead of Bun APIs so the same
 * bridge can be used by OpenCode (Bun), Codex hooks (Node), and pi (Node).
 */
import net from "node:net";

const DEFAULT_HOST = "192.168.2.155";
const DEFAULT_PORT = 8080;
const MAX_LINE_BUFFER = 16 * 1024;

function numberOr(value, fallback) {
  const n = Number(value);
  return Number.isFinite(n) ? n : fallback;
}

export function elaraConfig(env = process.env) {
  const parsedPort = Math.trunc(numberOr(env.ELARA_PORT, DEFAULT_PORT));
  return {
    host: env.ELARA_HOST || DEFAULT_HOST,
    port: parsedPort > 0 && parsedPort <= 65535 ? parsedPort : DEFAULT_PORT,
    debug: env.ELARA_DEBUG === "1" || env.ELARA_DEBUG === "true",
  };
}

export function truncate(value, max) {
  return String(value ?? "").slice(0, max);
}

export function createSessionState() {
  return {
    running: 0,
    waiting: 0,
    completed: false,
    msg: "idle",
    tokens_today: 0,
  };
}

export function buildSessionReport(state, prompt = null, usage = null) {
  const message = truncate(state.msg || "elara", 23);
  const report = {
    total: Math.max(0, Number(state.running) || 0) + Math.max(0, Number(state.waiting) || 0),
    running: Math.max(0, Number(state.running) || 0),
    waiting: Math.max(0, Number(state.waiting) || 0),
    completed: Boolean(state.completed),
    tokens: Math.max(0, Math.trunc(Number(state.tokens_today) || 0)),
    tokens_today: Math.max(0, Math.trunc(Number(state.tokens_today) || 0)),
    msg: message,
    entries: [truncate(state.msg || "elara", 90)],
  };

  if (usage && [usage.rolling, usage.weekly, usage.monthly].some((v) => Number.isFinite(v) && v >= 0)) {
    report.usage = {
      rolling: Number.isFinite(usage.rolling) && usage.rolling >= 0 ? usage.rolling : undefined,
      weekly: Number.isFinite(usage.weekly) && usage.weekly >= 0 ? usage.weekly : undefined,
      monthly: Number.isFinite(usage.monthly) && usage.monthly >= 0 ? usage.monthly : undefined,
    };
  }

  if (prompt) {
    report.prompt = {
      id: truncate(prompt.id, 39),
      tool: truncate(prompt.tool || "tool", 18),
      hint: truncate(prompt.hint || prompt.tool || "approval required", 42),
    };
  }

  return report;
}

/**
 * A small newline-delimited JSON transport.
 *
 * `exchange()` registers its waiter before sending, which avoids losing a
 * fast device response. Multiple callers share one connection attempt.
 */
export function createElaraBridge(options = {}) {
  const config = { ...elaraConfig(), ...options };
  const log = typeof config.log === "function" ? config.log : () => {};

  let socket = null;
  let connecting = null;
  let closed = false;
  let inputBuffer = "";
  let heartbeatTimer = null;
  const listeners = new Set();
  const waiters = new Set();

  function deliver(message) {
    for (const waiter of [...waiters]) {
      let matched = false;
      try {
        matched = waiter.match(message);
      } catch (error) {
        log("warn", `device response matcher failed: ${error.message}`);
      }
      if (!matched) continue;
      waiters.delete(waiter);
      waiter.resolve(message);
    }

    for (const listener of [...listeners]) {
      try {
        listener(message);
      } catch (error) {
        log("warn", `device message handler failed: ${error.message}`);
      }
    }
  }

  function onData(chunk) {
    inputBuffer += Buffer.from(chunk).toString("utf8");
    if (inputBuffer.length > MAX_LINE_BUFFER) {
      log("warn", "device input buffer exceeded limit; discarding partial data");
      inputBuffer = "";
    }

    while (true) {
      const newline = inputBuffer.indexOf("\n");
      if (newline < 0) break;
      const line = inputBuffer.slice(0, newline).trim();
      inputBuffer = inputBuffer.slice(newline + 1);
      if (!line) continue;
      try {
        deliver(JSON.parse(line));
      } catch {
        log("debug", "ignored non-JSON device line");
      }
    }
  }

  async function connect() {
    if (closed) return null;
    if (socket && !socket.destroyed) return socket;
    if (connecting) return connecting;

    connecting = new Promise((resolve, reject) => {
      const candidate = net.createConnection({ host: config.host, port: config.port });
      let connected = false;

      candidate.setNoDelay(true);
      candidate.on("data", onData);
      candidate.on("error", (error) => {
        if (!connected) {
          reject(error);
          return;
        }
        if (socket === candidate) socket = null;
        log("warn", `device socket error: ${error.message}`);
      });
      candidate.on("close", () => {
        if (socket === candidate) socket = null;
      });
      candidate.once("connect", () => {
        connected = true;
        socket = candidate;
        log("debug", `device connected ${config.host}:${config.port}`);
        resolve(candidate);
      });
    }).finally(() => {
      connecting = null;
    });

    try {
      return await connecting;
    } catch (error) {
      log("warn", `device connect failed: ${error.message}`);
      return null;
    }
  }

  async function send(value) {
    const candidate = await connect();
    if (!candidate || candidate.destroyed) return false;

    let line;
    try {
      line = `${JSON.stringify(value)}\n`;
    } catch (error) {
      log("error", `cannot serialize device message: ${error.message}`);
      return false;
    }

    return new Promise((resolve) => {
      let settled = false;
      const finish = (ok) => {
        if (settled) return;
        settled = true;
        candidate.off("error", onWriteError);
        resolve(ok);
      };
      const onWriteError = () => {
        if (socket === candidate) socket = null;
        finish(false);
      };

      candidate.once("error", onWriteError);
      try {
        candidate.write(line, () => finish(true));
      } catch (error) {
        log("warn", `device send failed: ${error.message}`);
        finish(false);
      }
    });
  }

  function waitFor(match, timeoutMs) {
    let timer;
    let waiter;
    const promise = new Promise((resolve) => {
      waiter = {
        match,
        resolve: (message) => {
          clearTimeout(timer);
          resolve(message);
        },
      };
      waiters.add(waiter);
      timer = setTimeout(() => {
        waiters.delete(waiter);
        resolve(null);
      }, timeoutMs);
    });
    return { promise, waiter };
  }

  async function exchange(value, { match = () => true, timeoutMs = 120000 } = {}) {
    const pending = waitFor(match, timeoutMs);
    if (!(await send(value))) {
      waiters.delete(pending.waiter);
      return null;
    }
    return pending.promise;
  }

  function onMessage(listener) {
    listeners.add(listener);
    return () => listeners.delete(listener);
  }

  function startHeartbeat(report, intervalMs = 15000) {
    stopHeartbeat();
    const sendReport = () => {
      let value;
      try {
        value = typeof report === "function" ? report() : report;
      } catch (error) {
        log("warn", `report builder failed: ${error.message}`);
        return;
      }
      if (value) void send(value);
    };
    sendReport();
    heartbeatTimer = setInterval(sendReport, intervalMs);
    return heartbeatTimer;
  }

  function stopHeartbeat() {
    if (heartbeatTimer) clearInterval(heartbeatTimer);
    heartbeatTimer = null;
  }

  function close() {
    closed = true;
    stopHeartbeat();
    for (const waiter of waiters) waiter.resolve(null);
    waiters.clear();
    listeners.clear();
    inputBuffer = "";
    if (socket && !socket.destroyed) socket.destroy();
    socket = null;
  }

  return { config, connect, send, exchange, onMessage, startHeartbeat, stopHeartbeat, close };
}
