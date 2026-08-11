#!/usr/bin/env python3
"""
Elara 上位机客户端
=================
通过 TCP (设备端口 8080) 或串口与 Elara 设备通信。

协议说明见 docs/上位机通信协议.md

用法示例:
    # 交互式 (TCP)
    python elara_host.py --host 192.168.2.155

    # 单条命令 (TCP)
    python elara_host.py --host 192.168.2.155 status --state idle
    python elara_host.py --host 192.168.2.155 chat --role ai --text "你好"
    python elara_host.py --host 192.168.2.155 clear
    python elara_host.py --host 192.168.2.155 progress --value 50
    python elara_host.py --host 192.168.2.155 petdex --state "run right"
    python elara_host.py --host 192.168.2.155 name --value "Pixel"
    python elara_host.py --host 192.168.2.155 owner --value "Alex"
    python elara_host.py --host 192.168.2.155 snap
    python elara_host.py --host 192.168.2.155 snap

    # 串口 (115200, USB-Serial/JTAG)
    python elara_host.py --serial COM5 status --state idle
    python elara_host.py --serial /dev/tty.usbmodem0 chat --role user --text "hi"

    # 流式分片消息
    python elara_host.py --host 192.168.2.155 chat --role ai --text "你好" --chunk --seq 0
    python elara_host.py --host 192.168.2.155 chat --role ai --text "，我是" --chunk --seq 1
    python elara_host.py --host 192.168.2.155 chat --role ai --text "Elara" --seq 2

    # 上传精灵图 (PNG)
    python elara_host.py --host 192.168.2.155 upload --file sprite.png --path idle/frame_001.png

    # 角色包传输 (base64 分块)
    python elara_host.py --host 192.168.2.155 xfer --char pet --file idle/frame_001.png
"""

import argparse
import base64
import json
import os
import socket
import sys
import threading
import time

DEVICE_PORT = 8080
SERIAL_BAUD = 115200
CHUNK_MAX = 400          # 设备 base64 chunk 最大解码字节数 (xfer.c)


# ---------------------------------------------------------------------------
# 传输层: TCP 与串口
# ---------------------------------------------------------------------------
class Transport:
    def send(self, data: bytes): raise NotImplementedError
    def drain(self, timeout: float = None) -> list: raise NotImplementedError
    def close(self): raise NotImplementedError


class TcpTransport(Transport):
    def __init__(self, host: str, port: int = DEVICE_PORT):
        self._sock = socket.create_connection((host, port), timeout=10)
        self._sock.settimeout(1.0)
        self._buf = b""
        self._alive = True
        threading.Thread(target=self._reader, daemon=True).start()

    def _reader(self):
        """后台线程接收设备消息, 放入 self._messages 队列供主线程读取."""
        try:
            while self._alive:
                try:
                    data = self._sock.recv(4096)
                except socket.timeout:
                    continue
                if not data:
                    break
                self._buf += data
        except OSError:
            pass
        self._alive = False

    def send(self, data: bytes):
        self._sock.sendall(data)

    def drain(self, timeout: float = 0.5) -> list:
        """取出当前缓冲区中的所有完整行 (用于命令后接收 ack/事件)."""
        lines = []
        deadline = time.time() + timeout
        while time.time() < deadline:
            while b"\n" in self._buf:
                raw, self._buf = self._buf.split(b"\n", 1)
                line = raw.decode("utf-8", errors="replace").strip()
                if line:
                    lines.append(line)
            time.sleep(0.05)
        return lines

    def close(self):
        self._alive = False
        try:
            self._sock.close()
        except OSError:
            pass


class SerialTransport(Transport):
    """串口传输层, 依赖 pyserial (可选)."""

    def __init__(self, port: str, baud: int = SERIAL_BAUD):
        try:
            import serial  # noqa
        except ImportError:
            sys.exit("串口模式需要 pyserial: pip install pyserial")
        self._ser = serial.Serial(port, baud, timeout=0.2)
        self._buf = b""

    def send(self, data: bytes):
        self._ser.write(data)

    def drain(self, timeout: float = 0.5) -> list:
        lines = []
        deadline = time.time() + timeout
        while time.time() < deadline:
            data = self._ser.read(4096)
            if data:
                self._buf += data
            while b"\n" in self._buf:
                raw, self._buf = self._buf.split(b"\n", 1)
                line = raw.decode("utf-8", errors="replace").strip()
                if line:
                    lines.append(line)
            time.sleep(0.05)
        return lines

    def close(self):
        try:
            self._ser.close()
        except Exception:
            pass


def format_msg(msg) -> str:
    """格式化设备返回的消息, 便于控制台阅读."""
    if isinstance(msg, dict):
        if "ack" in msg:
            tag = "ACK"
        elif msg.get("type") == "event":
            tag = "EVT"
        elif msg.get("type") == "error":
            tag = "ERR"
        elif msg.get("type") == "status":
            tag = "STATUS"
        else:
            tag = "MSG"
        color = {"ACK": "\033[36m", "EVT": "\033[33m", "ERR": "\033[31m",
                 "STATUS": "\033[32m", "MSG": "\033[35m"}[tag]
        return f"{color}[{tag}]\033[0m {json.dumps(msg, ensure_ascii=False)}"
    return msg


# ---------------------------------------------------------------------------
# 命令构建
# ---------------------------------------------------------------------------
def cmd_status(state: str):
    return {"type": "status", "state": state}


def cmd_chat(role: str, text: str, emotion: str = None, chunk: bool = False,
             seq: int = 0):
    m = {"type": "chat", "role": role, "text": text, "chunk": chunk, "seq": seq}
    if emotion:
        m["emotion"] = emotion
    return m


def cmd_clear():
    return {"type": "cmd", "action": "clear"}


def cmd_progress(value: int):
    return {"type": "progress", "value": max(0, min(100, value))}


def cmd_petdex(state: str):
    return {"type": "petdex", "state": state}


def cmd_wifi(ssid: str, password: str):
    return f"wifi {ssid} {password}"


def cmd_name(name: str):
    return {"cmd": "name", "name": name}


def cmd_owner(name: str):
    return {"cmd": "owner", "name": name}


def cmd_species(idx: int):
    return {"cmd": "species", "idx": idx}


def cmd_snap():
    return {"cmd": "status"}


# ---------------------------------------------------------------------------
# 高层操作
# ---------------------------------------------------------------------------
def send_json(transport: Transport, msg, label: str = "") -> list:
    if isinstance(msg, str):
        payload = (msg + "\n").encode("utf-8")
    else:
        payload = (json.dumps(msg, ensure_ascii=False) + "\n").encode("utf-8")
    transport.send(payload)
    prefix = f"{label} " if label else ""
    print(f">> {prefix}{payload.decode('utf-8', errors='replace').strip()}")
    return transport.drain(0.5)


def upload_file(transport: Transport, filepath: str, path: str = None):
    """通过 upload 命令上传 PNG 精灵图 (JSON 握手 + 原始二进制)."""
    if not os.path.isfile(filepath):
        sys.exit(f"文件不存在: {filepath}")
    size = os.path.getsize(filepath)

    req = {"type": "upload", "size": size}
    if path:
        req["path"] = path
    payload = (json.dumps(req, ensure_ascii=False) + "\n").encode("utf-8")
    transport.send(payload)
    print(f">> upload {payload.decode('utf-8', errors='replace').strip()}")

    def wait_upload_event(action, timeout=5.0):
        """等待设备发送 upload 事件 (ready / finished)."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            for line in transport.drain(0.1):
                try:
                    m = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if m.get("type") == "error":
                    print(f"!! 设备错误: {m.get('msg')}")
                    sys.exit(1)
                if m.get("type") == "event" and m.get("source") == "upload" and m.get("action") == action:
                    return True
        return False

    if wait_upload_event("ready"):
        print(f"设备就绪, 开始发送 {size} 字节...")
    else:
        print("设备未就绪 (未收到 upload ready), 仍尝试发送...")

    with open(filepath, "rb") as f:
        while True:
            chunk = f.read(512)
            if not chunk:
                break
            transport.send(chunk)
            time.sleep(0.01)

    print("二进制数据发送完成, 等待设备确认...")
    if wait_upload_event("finished", timeout=10.0):
        print("[EVT] upload finished")
    else:
        print("!! 未收到 upload finished, 上传可能未完成")


def xfer_char_file(transport: Transport, char: str, filepath: str):
    """角色包传输: char_begin -> file -> chunk(base64) -> file_end -> char_end."""
    if not os.path.isfile(filepath):
        sys.exit(f"文件不存在: {filepath}")

    send_json(transport, {"cmd": "char_begin", "name": char, "total": 1}, label="xfer")
    path = os.path.basename(filepath)
    size = os.path.getsize(filepath)

    send_json(transport, {"cmd": "file", "path": path, "size": size}, label="xfer")

    sent = 0
    with open(filepath, "rb") as f:
        while True:
            raw = f.read(CHUNK_MAX)
            if not raw:
                break
            b64 = base64.b64encode(raw).decode("ascii")
            resp = send_json(transport, {"cmd": "chunk", "d": b64}, label="xfer")
            sent += len(raw)
            ok = any(isinstance(m, dict) and m.get("ack") == "chunk"
                     and m.get("ok") for m in resp)
            if not ok:
                print(f"!! chunk 失败 @ {sent} bytes, 中止")
                return
            print(f"   已发送 {sent}/{size} bytes")
            time.sleep(0.02)

    send_json(transport, {"cmd": "file_end"}, label="xfer")
    send_json(transport, {"cmd": "char_end"}, label="xfer")
    print("角色文件传输完成")


def interactive(transport: Transport):
    """交互式控制台: 输入命令, 设备返回自动打印."""
    print("Elara 上位机客户端 - 交互模式 (输入 help 查看命令, Ctrl+C 退出)")
    print("=" * 60)

    def print_help():
        print("""
可用命令:
  status <state>        设置状态 (idle/listening/thinking/replying/error/
                        'run right'/'run left'/waving/jumping/failed/waiting/review)
  chat <role> <text>    发送消息 (role: user/ai/system)
  clear                 清空聊天记录
  progress <0-100>      设置进度条
  petdex <state>        切换 Petdex 动画
  wifi <ssid> <pass>    配置 Wi-Fi (纯文本)
  name <name>           设置宠物名
  owner <name>          设置主人名
  snap                  获取状态快照
  upload <file> [path]  上传精灵图
  xfer <char> <file>    角色包文件传输
  help / exit           帮助 / 退出
        """)

    print_help()
    try:
        while True:
            try:
                raw = input("elara> ").strip()
            except EOFError:
                break
            if not raw:
                continue
            parts = raw.split()
            cmd = parts[0].lower()

            if cmd == "help":
                print_help()
            elif cmd == "exit" or cmd == "quit":
                break
            elif cmd == "status" and len(parts) >= 2:
                send_json(transport, cmd_status(" ".join(parts[1:])))
            elif cmd == "chat" and len(parts) >= 3:
                send_json(transport, cmd_chat(parts[1], " ".join(parts[2:])))
            elif cmd == "clear":
                send_json(transport, cmd_clear())
            elif cmd == "progress" and len(parts) >= 2:
                send_json(transport, cmd_progress(int(parts[1])))
            elif cmd == "petdex" and len(parts) >= 2:
                send_json(transport, cmd_petdex(" ".join(parts[1:])))
            elif cmd == "wifi" and len(parts) >= 3:
                send_json(transport, cmd_wifi(parts[1], parts[2]))
            elif cmd == "name" and len(parts) >= 2:
                send_json(transport, cmd_name(" ".join(parts[1:])))
            elif cmd == "owner" and len(parts) >= 2:
                send_json(transport, cmd_owner(" ".join(parts[1:])))
            elif cmd == "snap":
                send_json(transport, cmd_snap())
            elif cmd == "upload" and len(parts) >= 2:
                upload_file(transport, parts[1], parts[2] if len(parts) >= 3 else None)
            elif cmd == "xfer" and len(parts) >= 3:
                xfer_char_file(transport, parts[1], parts[2])
            else:
                print(f"未知命令或参数不足: {raw}")
    except KeyboardInterrupt:
        print("\n退出")


# ---------------------------------------------------------------------------
# 主入口
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="Elara 上位机客户端 (TCP/串口, JSON 行协议)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)
    parser.add_argument("--host", help="设备 IP (TCP 模式)")
    parser.add_argument("--port", type=int, default=DEVICE_PORT, help="TCP 端口 (默认 8080)")
    parser.add_argument("--serial", help="串口设备 (如 COM5 或 /dev/tty.usbmodem0)")
    parser.add_argument("--baud", type=int, default=SERIAL_BAUD, help="串口波特率 (默认 115200)")

    sub = parser.add_subparsers(dest="command")

    def add_cmd(name, args=None):
        p = sub.add_parser(name)
        if args:
            for a in args:
                p.add_argument(*a["flags"], **a["opts"])
        return p

    add_cmd("status", [{"flags": ["--state"], "opts": {"required": True}}])
    add_cmd("chat", [{"flags": ["--role"], "opts": {"required": True, "choices": ["user", "ai", "system"]}},
                     {"flags": ["--text"], "opts": {"required": True}},
                     {"flags": ["--emotion"], "opts": {}},
                     {"flags": ["--chunk"], "opts": {"action": "store_true"}},
                     {"flags": ["--seq"], "opts": {"type": int, "default": 0}}])
    add_cmd("clear")
    add_cmd("progress", [{"flags": ["--value"], "opts": {"type": int, "required": True}}])
    add_cmd("petdex", [{"flags": ["--state"], "opts": {"required": True}}])
    add_cmd("wifi", [{"flags": ["--ssid"], "opts": {"required": True}},
                     {"flags": ["--password"], "opts": {"required": True}}])
    add_cmd("name", [{"flags": ["--value"], "opts": {"required": True}}])
    add_cmd("owner", [{"flags": ["--value"], "opts": {"required": True}}])
    add_cmd("species", [{"flags": ["--idx"], "opts": {"type": int, "required": True}}])
    add_cmd("snap")
    add_cmd("upload", [{"flags": ["--file"], "opts": {"required": True}},
                       {"flags": ["--path"], "opts": {}}])
    add_cmd("xfer", [{"flags": ["--char"], "opts": {"required": True}},
                     {"flags": ["--file"], "opts": {"required": True}}])

    args = parser.parse_args()

    if not args.host and not args.serial:
        parser.print_help()
        sys.exit(1)

    transport = None
    try:
        if args.serial:
            transport = SerialTransport(args.serial, args.baud)
            print(f"连接串口 {args.serial} @ {args.baud}")
        else:
            transport = TcpTransport(args.host, args.port)
            print(f"连接设备 {args.host}:{args.port}")
    except (socket.error, OSError) as e:
        sys.exit(f"连接失败: {e}")

    try:
        if not args.command:
            interactive(transport)
        elif args.command == "status":
            for m in send_json(transport, cmd_status(args.state), label="status"):
                print(format_msg(m))
        elif args.command == "chat":
            for m in send_json(transport, cmd_chat(args.role, args.text,
                                                   args.emotion, args.chunk,
                                                   args.seq), label="chat"):
                print(format_msg(m))
        elif args.command == "clear":
            send_json(transport, cmd_clear(), label="clear")
        elif args.command == "progress":
            send_json(transport, cmd_progress(args.value), label="progress")
        elif args.command == "petdex":
            send_json(transport, cmd_petdex(args.state), label="petdex")
        elif args.command == "wifi":
            send_json(transport, cmd_wifi(args.ssid, args.password), label="wifi")
        elif args.command == "name":
            send_json(transport, cmd_name(args.value), label="name")
        elif args.command == "owner":
            send_json(transport, cmd_owner(args.value), label="owner")
        elif args.command == "species":
            send_json(transport, cmd_species(args.idx), label="species")
        elif args.command == "snap":
            send_json(transport, cmd_snap(), label="snap")
        elif args.command == "upload":
            upload_file(transport, args.file, args.path)
        elif args.command == "xfer":
            xfer_char_file(transport, args.char, args.file)
    except (BrokenPipeError, ConnectionResetError, socket.error) as e:
        print(f"!! 连接中断: {e}")
    finally:
        transport.close()


if __name__ == "__main__":
    main()
