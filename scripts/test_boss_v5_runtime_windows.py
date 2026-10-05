#!/usr/bin/env python3
"""Windows Debug client/server smoke for the v5 bosses (1 Bastion Strider, 2 Storm Seraph, 3 Siege Monolith).

Launches the real server and SDL/OpenGL client from build-v4, spawns the boss (argv[1] = kind 1..3) next to the
client with the debug-only ib_test_* commands, forces each act, and captures engine frames.
"""
import sys
import ctypes, json, os, re, socket, subprocess, time
from ctypes import wintypes
from pathlib import Path
from PIL import ImageGrab

R = Path(__file__).resolve().parents[1]
B = R / 'build-v4'
KIND = int(sys.argv[1]) if len(sys.argv) > 1 else 1
NAME = {1: 'strider', 2: 'seraph', 3: 'monolith'}[KIND]
DIRN = {1: 'bastion_strider', 2: 'storm_seraph', 3: 'siege_monolith'}[KIND]
OUT = R / 'design/boss-redesign-v5' / DIRN / 'runtime-smoke'
OUT.mkdir(parents=True, exist_ok=True)
WORK = B / ('bossv5-runtime-windows-' + NAME)
WORK.mkdir(parents=True, exist_ok=True)
processes, logs, transcript, frames = [], [], [], []
s = None


def port(kind):
    q = socket.socket(type=kind)
    q.bind(('127.0.0.1', 0))
    v = q.getsockname()[1]
    q.close()
    return v


def launch(name, exe, commands):
    home = WORK / name
    home.mkdir(exist_ok=True)
    env = dict(os.environ, HOME=str(home), SDL_AUDIODRIVER='dummy', SDL_VIDEO_WINDOW_POS='40,40')
    env['PATH'] = str(B) + ';C:/msys64/mingw64/bin;' + env.get('PATH', '')
    f = open(WORK / (name + '.log'), 'w', encoding='utf8')
    logs.append(f)
    p = subprocess.Popen([str(exe), commands], cwd=R, env=env, stdout=f, stderr=subprocess.STDOUT)
    processes.append(p)
    return p


def hwnd_for_pid(pid):
    found = []
    u = ctypes.windll.user32

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def cb(hwnd, lparam):
        value = wintypes.DWORD()
        u.GetWindowThreadProcessId(hwnd, ctypes.byref(value))
        if value.value == pid and u.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True

    u.EnumWindows(cb, 0)
    return found[0] if found else None


def capture(hwnd, name):
    im = ImageGrab.grab(window=hwnd)
    path = OUT / (name + '.png')
    im.save(path)
    frames.append(path.name)
    return path


game_port = port(socket.SOCK_DGRAM)
econ_port = port(socket.SOCK_STREAM)
try:
    server = launch('server', B / 'ninslash_srv.exe',
                    f'exec cfg/invasion2.cfg; sv_mapgen_random_seed 0; sv_mapgen_seed 42817; sv_register 0; sv_port {game_port}; '
                    f'ec_port {econ_port}; ec_bindaddr 127.0.0.1; ec_password local-v5-qa; sv_mapgen_level 20; '
                    'sv_invasion_use_checkpoint 0; sv_pve_roguelite 0; sv_pve_contracts 0; sv_survivalmode 0; sv_pve_choice_time 3; debug 1')
    end = time.monotonic() + 25
    while s is None:
        try:
            s = socket.create_connection(('127.0.0.1', econ_port), timeout=1)
        except OSError:
            if time.monotonic() > end or server.poll() is not None:
                raise RuntimeError('server did not start')
            time.sleep(.2)
    s.settimeout(.015)

    def drain():
        out = b''
        while True:
            try:
                v = s.recv(65536)
                if not v:
                    raise RuntimeError('econ closed')
                out += v
            except socket.timeout:
                break
        text = out.decode(errors='replace').replace('\x00', '')
        transcript.append(text)
        return text

    drain()
    s.sendall(b'local-v5-qa\n')
    time.sleep(.2)
    assert 'Authentication successful' in drain()
    serial = [0]

    def cmd(text, wait=.12):
        serial[0] += 1
        mark = f'__V5_ACK_{serial[0]}'
        s.sendall((text + '; echo ' + mark + '\n').encode())
        out = ''
        end = time.monotonic() + 30
        while '[Console]: ' + mark not in out:
            out += drain()
            if time.monotonic() > end:
                raise RuntimeError('unacknowledged ' + text)
        time.sleep(wait)
        return out + drain()

    client = launch('client', B / 'ninslash.exe',
                    f'cl_cpu_throttle 0; gfx_fullscreen 0; gfx_borderless 0; gfx_screen_width 1280; gfx_screen_height 800; '
                    f'gfx_refresh_rate 60; snd_enable 0; player_name BossV5QA; connect 127.0.0.1:{game_port}')
    end = time.monotonic() + 40
    while True:
        state = cmd('ib_test_state', .3)
        if 'client=0 health=' in state:
            break
        if client.poll() is not None or time.monotonic() > end:
            raise RuntimeError('client did not enter game')
    hwnd = None
    end = time.monotonic() + 15
    while hwnd is None and time.monotonic() < end:
        hwnd = hwnd_for_pid(client.pid)
        time.sleep(.1)
    if hwnd is None:
        raise RuntimeError('client window not found')

    PAT = NAME + r' act=[^\n]*'
    spawn = cmd(f'ib_test_spawn {KIND} 0', 1.5)
    assert NAME + ' act=' in spawn, spawn
    states = {'spawn': re.findall(PAT, spawn)}
    capture(hwnd, '00-spawn')
    for i in range(10):
        cmd('ib_test_refill', .35)
        capture(hwnd, f'01-ai-{i}')
        states.setdefault('ai', []).extend(re.findall(PAT, cmd('ib_test_state', 0)))
    acts = {1: {2: 'bash', 3: 'charge', 4: 'stomp', 5: 'mortar', 6: 'leap', 7: 'roar', 9: 'overheat'},
            2: {2: 'dive', 3: 'lattice', 4: 'orbs', 5: 'plunge', 6: 'recharge', 7: 'roar', 9: 'zap'},
            3: {2: 'sweep', 3: 'mortar', 4: 'transform', 5: 'bomb', 6: 'drones', 7: 'slam', 8: 'roar'}}[KIND]
    for act, name in acts.items():
        cmd('ib_test_refill', .05)
        cmd(f'ib_test_action {act}', .05)
        for k, delay in enumerate((.2, .35, .35)):
            time.sleep(delay)
            capture(hwnd, f'02-{name}-{k}')
        states[name] = re.findall(PAT, cmd('ib_test_state', 0))
        time.sleep(.6)
    # Damage until phase 3 (strider shield soaks frontal hits, so loop).
    phase = 0
    for i in range(40):
        cmd('ib_test_refill', .05)
        st = cmd('ib_test_damage 900 0', .25)
        m = re.findall(r'phase=(\d)', cmd('ib_test_state', 0))
        phase = int(m[-1]) if m else phase
        if phase >= 1 and 'phase2' not in states:
            states['phase2'] = re.findall(PAT, cmd('ib_test_state', 0)); capture(hwnd, '03-phase2')
        if phase >= 2:
            break
    assert phase >= 2, 'phase 3 not reached'
    time.sleep(1.2)
    capture(hwnd, '04-phase3')
    states['phase3'] = re.findall(PAT, cmd('ib_test_state', 0))
    cmd('ib_test_refill', .05)
    cmd('ib_test_damage 1000000 0', .6)
    capture(hwnd, '05-death')
    for i in range(10):
        time.sleep(.8)
        dead = cmd('ib_test_state')
        if NAME + ' act=' not in dead:
            break
    assert NAME + ' act=' not in dead, dead
    assert server.poll() is None and client.poll() is None
    report = {'status': 'PASS', 'environment': 'Windows Debug server + SDL/OpenGL client from build-v4', 'frames': frames,
              'states': states, 'removed_after_death': True}
    (OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(NAME.upper(), 'WINDOWS SMOKE PASS:', len(frames), 'frames')
finally:
    if s:
        try:
            s.sendall(b'shutdown\n')
            time.sleep(.2)
        except Exception:
            pass
        s.close()
    for p in reversed(processes):
        if p.poll() is None:
            p.terminate()
        try:
            p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()
    for f in logs:
        f.close()
    (WORK / 'console-transcript.txt').write_text(''.join(transcript), encoding='utf8')
