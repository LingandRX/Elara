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
 */
export const ElaraPlugin = async ({ client }) => {
  const HOST = process.env.ELARA_HOST || "192.168.2.155";
  const PORT = parseInt(process.env.ELARA_PORT || "8080", 10);

  let sock = null, buf = "", seq = 0;
  const pending = new Map();            // 弹窗 id -> { permissionID, sessionID }
  const seen = new Map();               // messageID -> 已累计 token 数 (防重复累加)
  const state = { running: 0, waiting: 0, completed: false, msg: "idle", tokens: 0, tokens_today: 0 };
  let prompt = null;                    // 待设备审批的弹窗 (随每次上报持续下发)

  /* ---------- TCP 传输 ---------- */

  async function connect() {
    if (sock) return;
    try {
      sock = await Bun.connect({
        hostname: HOST,
        port: PORT,
        socket: {
          open() { console.log("[elara] connected"); },
          data(_, d) { onData(d); },
          close() { sock = null; },
          error(_, e) { sock = null; console.error("[elara]", e.message); },
        },
      });
    } catch (e) { console.error("[elara] connect:", e.message); }
  }

  async function send(obj) {
    await connect();
    if (sock) { try { sock.write(JSON.stringify(obj) + "\n"); } catch (e) { console.error(e.message); } }
  }

  /* ---------- 上报 ---------- */

  function report() {
    const msg = {
      total: state.running + state.waiting,
      running: state.running,
      waiting: state.waiting,
      completed: state.completed,
      tokens: state.tokens,             // 增量, 喂给 buddy_stats_on_bridge_tokens
      tokens_today: state.tokens_today, // 累计, INFO 页 Tokens 显示
      msg: (state.msg || "opencode").slice(0, 23),
      entries: [(state.msg || "opencode").slice(0, 90)],
    };
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
    pending.delete(m.id);
    prompt = null;
    try {
      await client.postSessionIdPermissionsPermissionId({
        path: { id: p.sessionID, permissionID: p.permissionID },
        body: { response: m.decision === "once" ? "once" : "reject" },
      });
    } catch (e) { console.error("[elara] permission.reply:", e.message); }
  }

  /* ---------- 启动: 立即上报一次 + 15s 心跳 (保持设备 Online) ---------- */

  report();
  setInterval(report, 15000);

  /* ---------- 事件订阅 ---------- */

  return {
    event: async ({ event }) => {
      switch (event.type) {
        case "session.idle":
        case "tool.execute.after":
          done();                       // CELEBRATE, 4 秒后复位 idle
          break;

        case "session.error":
          state.running = 0; state.waiting = 0; state.completed = false; state.msg = "error!";
          report();
          break;

        case "session.status":
        case "session.updated":
          if (event.properties?.status === "running" || event.properties?.session?.status === "running")
            busy("working...");         // running=3 → Persona BUSY
          break;

        case "message.updated": {
          const tk = event.properties?.info?.tokens;
          if (!tk) break;
          const total = (tk.input || 0) + (tk.output || 0) + (tk.reasoning || 0)
                      + (tk.cache?.read || 0) + (tk.cache?.write || 0);
          if (total <= 0) break;
          const mid = event.properties.info.id || "";
          const prev = mid ? (seen.get(mid) || 0) : 0;
          if (total > prev) {
            state.tokens = total - prev;
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
          report();                     // 设备弹出审批框 (A=批准 once / B=拒绝 deny)
          break;
        }

        case "permission.replied":
          prompt = null;                // opencode 侧已回复, 撤销设备端弹窗
          break;
      }
    },

    "tool.execute.before": (input) => busy(`tool: ${(input.tool || "").slice(0, 40)}`),
  };
};
