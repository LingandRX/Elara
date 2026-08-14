/**
 * @file elara.js
 * opencode → Elara Buddy (ESP32-S3) 桥接插件 (精简版)
 *
 * 原理: 把 opencode 事件翻译成设备的 session JSON (TCP 8080),
 * 驱动 Persona 状态机 / HUD 文字 / 审批弹窗; 设备 A/B 键审批结果
 * 回传后通过 opencode SDK 回复权限请求, 形成闭环。
 *
 * 配置 (环境变量):
 *   ELARA_HOST  设备 IP, 默认 192.168.2.155
 *   ELARA_PORT  设备 TCP 端口, 默认 8080
 *   ELARA_ZEN_KEY  OpenCode Zen API key, 缺省时从 auth.json 读取 opencode-go
 */
import { readFileSync } from "fs";

export const ElaraPlugin = async ({ client }) => {
  const HOST = process.env.ELARA_HOST || "192.168.2.155";
  const PORT = parseInt(process.env.ELARA_PORT || "8080", 10);

  let sock = null, buf = "", seq = 0;
  const pending = new Map();            // 弹窗 id -> { permissionID, sessionID }
  const seen = new Map();               // messageID -> 已累计 token 数 (防重复累加)
  const state = { running: 0, waiting: 0, completed: false, msg: "idle", tokens_today: 0 };
  let prompt = null;                    // 待设备审批的弹窗 (随每次上报持续下发)

  /* ---------- 日志: 全部走 opencode 官方 client.app.log, 不污染 TUI ---------- */
  // opencode 会把插件 console.* 输出转发到 TUI (污染输入框/状态区), 这里统一改用
  // client.app.log() 写入服务端日志文件; ELARA_DEBUG=1 时额外输出 debug/info 级别。
  const DEBUG = !!process.env.ELARA_DEBUG;
  function log(level, message, extra) {
    try {
      Promise.resolve(client.app.log({
        body: { service: "elara", level, message, extra },
      })).catch(() => { /* 日志失败不影响主流程 */ });
    } catch { /* 同上 */ }
  }
  const debug = (m) => DEBUG && log("debug", m);
  const info  = (m) => DEBUG && log("info", m);
  // 同类错误节流 (默认 60s): 设备离线时 connect/send 每个心跳都会失败, 避免刷日志
  let lastThrottledAt = 0;
  function logThrottled(level, message, ms = 60000) {
    const now = Date.now();
    if (now - lastThrottledAt < ms) return;
    lastThrottledAt = now;
    log(level, message);
  }

  /* ---------- Zen 套餐用量 (rolling/weekly/monthly) ---------- */

  // 从 auth.json 提取 Zen (opencode-go) API key；可用 ELARA_ZEN_KEY 环境变量覆盖
  function zenKey() {
    if (process.env.ELARA_ZEN_KEY) return process.env.ELARA_ZEN_KEY;
    try {
      const home = process.env.HOME || process.env.USERPROFILE || "";
      const dataHome = process.env.XDG_DATA_HOME || `${home}/.local/share`;
      const auth = JSON.parse(readFileSync(`${dataHome}/opencode/auth.json`, "utf8"));
      const key = auth?.["opencode-go"]?.key || auth?.["opencode"]?.key;
      if (key) return key;
    } catch { /* 找不到就跳过用量上报 */ }
    return null;
  }

  const zenUsage = { rolling: -1, weekly: -1, monthly: -1 }; // -1 = 未知
  let zenUsageAt = 0;

  // 查询 Zen 套餐额度（官方接口），失败时保留旧值
  async function fetchZenUsage() {
    const key = zenKey();
    if (!key) return;
    const now = Date.now();
    if (now - zenUsageAt < 30000) return; // 30s 缓存，避免频繁请求
    try {
      const r = await fetch("https://opencode.ai/zen/go/v1/usage", {
        headers: { authorization: `Bearer ${key}` },
        signal: AbortSignal.timeout(8000),
      });
      if (!r.ok) return;
      const j = await r.json();
      const u = j?.usage;
      if (!u) return;
      if (typeof u.rolling?.percent === "number") zenUsage.rolling = u.rolling.percent;
      if (typeof u.weekly?.percent === "number") zenUsage.weekly = u.weekly.percent;
      if (typeof u.monthly?.percent === "number") zenUsage.monthly = u.monthly.percent;
      zenUsageAt = Date.now();
    } catch { /* 网络失败等，下次再试 */ }
  }

  /* ---------- TCP 传输 ---------- */

  async function connect() {
    if (sock) return;
    try {
      sock = await Bun.connect({
        hostname: HOST,
        port: PORT,
        socket: {
          open() { info("device connected"); },
          data(_, d) { onData(d); },
          close() { sock = null; },
          error(_, e) { sock = null; logThrottled("warn", "socket error: " + e.message); },
        },
      });
    } catch (e) { logThrottled("warn", "connect failed: " + e.message); }
  }

  async function send(obj) {
    await connect();
    if (sock) { try { sock.write(JSON.stringify(obj) + "\n"); } catch (e) { logThrottled("warn", "send failed: " + e.message); } }
  }

  /* ---------- 上报 ---------- */

  function report() {
    const msg = {
      total: state.running + state.waiting,
      running: state.running,
      waiting: state.waiting,
      completed: state.completed,
      tokens: state.tokens_today,       // 累计值, 设备端 buddy_stats_on_bridge_tokens 自行计算增量
      tokens_today: state.tokens_today,    // 累计, INFO 页 Tokens 显示
      msg: (state.msg || "opencode").slice(0, 23),
      entries: [(state.msg || "opencode").slice(0, 90)],
    };
    // Zen 套餐用量 (0-100), PET 页面进度条显示；未知时省略
    if (zenUsage.rolling >= 0 || zenUsage.weekly >= 0 || zenUsage.monthly >= 0) {
      msg.usage = {
        rolling: zenUsage.rolling,  // 滚动窗口
        weekly: zenUsage.weekly,    // 每周
        monthly: zenUsage.monthly,  // 每月
      };
    }
    if (prompt) msg.prompt = prompt;    // 有待审批时持续附带, 否则设备会撤销弹窗
    send(msg);
  }

  function busy(msg) { state.running = 3; state.waiting = 0; state.completed = false; state.msg = msg; report(); }
  function done() {
    state.running = 0; state.waiting = 0; state.completed = true; state.msg = "done!";
    report();
    setTimeout(() => { state.completed = false; state.msg = "idle"; report(); }, 4000);
  }

  /* ---------- 设备上行 (审批结果) ---------- */

  function onData(data) {
    buf += Buffer.from(data).toString("utf8");
    let i;
    while ((i = buf.indexOf("\n")) >= 0) {
      const line = buf.slice(0, i).trim();
      buf = buf.slice(i + 1);
      if (!line) continue;
      try {
        const m = JSON.parse(line);
        if (m.cmd === "permission") replyPermission(m);
      } catch { /* 忽略非 JSON 行 */ }
    }
  }

  // 设备 A/B 键结果 (once/deny) → 回复 opencode 权限请求
  async function replyPermission(m) {
    const p = pending.get(m.id);
    if (!p) return;
    try {
      await client.postSessionIdPermissionsPermissionId({
        path: { id: p.sessionID, permissionID: p.permissionID },
        body: { response: m.decision === "once" ? "once" : "reject" },
      });
      pending.delete(m.id);
      prompt = null;
      debug(`permission replied: ${m.decision}`);
    } catch (e) {
      // 失败时保留 pending/prompt: 设备端弹窗已本地关闭, 等待 opencode 超时
      // 或用户在 TUI 回复后由 permission.replied 统一清理
      log("error", "permission.reply: " + e.message);
    }
  }

  /* ---------- 启动: 立即上报一次 + 15s 心跳 (保持设备 Online) ---------- */

  fetchZenUsage().then(report);  // 先拉取用量再上报
  setInterval(() => {
    fetchZenUsage();
    report();
  }, 15000);

  /* ---------- 事件订阅 ---------- */

  return {
    event: async ({ event }) => {
      switch (event.type) {
        case "session.idle":
          seen.clear();                 // 会话结束, 清空 token 去重表防止无界增长
          done();                       // CELEBRATE, 4 秒后复位 idle
          break;

        case "session.error":
          state.running = 0; state.waiting = 0; state.completed = false; state.msg = "error!";
          report();
          break;

        case "session.status": {
          const st = event.properties?.status?.type;  // SessionStatus: "idle" | "retry" | "busy"
          if (st === "busy") {
            busy("working...");         // running=3 → Persona BUSY
          } else if (st === "retry") {  // 重试 → waiting=1 → Persona ATTENTION
            state.running = 0; state.waiting = 1; state.completed = false; state.msg = "retrying...";
            report();
          }
          break;
        }

        case "message.updated": {
          const tk = event.properties?.info?.tokens;
          if (!tk) break;
          const total = (tk.input || 0) + (tk.output || 0) + (tk.reasoning || 0)
                      + (tk.cache?.read || 0) + (tk.cache?.write || 0);
          if (total <= 0) break;
          const mid = event.properties.info.id || "";
          const prev = mid ? (seen.get(mid) || 0) : 0;
          if (total > prev) {
            state.tokens_today += total - prev;
            if (mid) seen.set(mid, total);
            report();
          }
          break;
        }

        case "permission.asked": {
          const p = event.properties || {};
          if (!p.id) break;
          const id = `oc_${++seq}`;
          pending.set(id, { permissionID: p.id, sessionID: p.sessionID });
          const perm = p.permission || "tool";
          const pat = Array.isArray(p.patterns) ? p.patterns.join(" ") : "";
          prompt = { id, tool: perm.slice(0, 18), hint: (perm + " " + pat).slice(0, 42) };
          debug(`permission asked: ${perm} -> ${id}`);
          report();                     // 设备弹出审批框 (A=批准 once / B=拒绝 deny)
          break;
        }

        case "permission.replied": {
          debug("permission.replied event");
          prompt = null;                // opencode 侧已回复, 撤销设备端弹窗
          const rid = event.properties?.requestID;
          if (rid) {
            // 清理该权限对应的 pending 条目, 防止 Map 无界增长
            for (const [id, p] of pending) if (p.permissionID === rid) pending.delete(id);
          }
          break;
        }
      }
    },

    "tool.execute.before": (input) => busy(`tool: ${(input.tool || "").slice(0, 40)}`),
    "tool.execute.after": () => done(),   // 顶层 hook (非 event), 工具执行完成 → CELEBRATE
  };
};
