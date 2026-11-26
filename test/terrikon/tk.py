#!/usr/bin/env python3
"""Linux counterpart of tk.psm1's Send-TkUi: talk to a running Terrikon's
automation port (TERRIKON_AUTOMATION=<port>, default 4213).

  tk.py help
  tk.py eval 'someExpression'
  tk.py player
  tk.py grab /tmp/x.png      # the 'grab' command (see the note below)
  tk.py shot /tmp/x.png [scale]   # the whole X display (DISPLAY, default :99)
                                  # through xwd+ImageMagick, not the app

On Linux 'grab' of the main window answers "nothing to grab" (QQuickWindow::
grabWindow gives a null image); 'shot' is the way to see it there.

Each argument-line is one command; several commands can be given with
--cmd. The raw JSON line is printed for each. As a module: send(cmd).
"""
import argparse, socket, sys, json

def send(cmd, port=4213, host="127.0.0.1", timeout=15.0):
    with socket.create_connection((host, port), timeout=timeout) as s:
        s.settimeout(timeout)
        s.sendall(cmd.rstrip("\n").encode() + b"\n")
        buf = b""
        while b"\n" not in buf:
            chunk = s.recv(65536)
            if not chunk:
                break
            buf += chunk
    return buf.split(b"\n", 1)[0].decode(errors="replace")

def shot(args):
    import os, subprocess
    if not args:
        print("tk.py: shot PATH [scale-percent, 50 = half size]", file=sys.stderr); return 2
    disp = os.environ.get("DISPLAY", ":99")
    scale = args[1].rstrip("%") if len(args) > 1 else "100"
    try:
        if float(scale) <= 0:
            raise ValueError
    except ValueError:
        print("tk.py: scale is a percent, e.g. 50", file=sys.stderr); return 2
    x = subprocess.run(["xwd", "-root", "-display", disp, "-silent"], capture_output=True)
    if x.returncode:
        print("tk.py: xwd failed", file=sys.stderr); return 2
    c = subprocess.run(["convert", "xwd:-", "-resize", scale + "%", args[0]], input=x.stdout)
    print(json.dumps({"saved": args[0]}) if c.returncode == 0 else "convert failed")
    return c.returncode

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=4213)
    ap.add_argument("--timeout", type=float, default=15.0)
    ap.add_argument("--cmd", action="append", default=[], help="extra command line (repeatable)")
    ap.add_argument("words", nargs="*", help="command and arguments, joined by spaces")
    a = ap.parse_args()
    if a.words[:1] == ["shot"]:
        return shot(a.words[1:])
    cmds = a.cmd + ([" ".join(a.words)] if a.words else [])
    if not cmds:
        ap.error("no command")
    rc = 0
    for c in cmds:
        try:
            out = send(c, a.port, timeout=a.timeout)
        except OSError as e:
            print(f"tk.py: {e}", file=sys.stderr); return 2
        print(out)
        try:
            j = json.loads(out)
            if isinstance(j, dict) and j.get("ok") is False:
                rc = 1
        except ValueError:
            pass
    return rc

if __name__ == "__main__":
    sys.exit(main())
