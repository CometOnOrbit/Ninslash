#!/usr/bin/env python3
"""Windows Debug client/server smoke for the Skitter Matriarch (v5 boss pilot).

Launches the real server and SDL/OpenGL client from build-v4, spawns the matriarch next to the
client with the debug-only ib_test_* commands, forces each act, and captures engine frames.
"""
import ctypes, json, os, re, socket, subprocess, time
from ctypes import wintypes
from pathlib import Path
from PIL import ImageGrab

R = Path(__file__).resolve().parents[1]
B = R / 'build-v4'
OUT = R / 'design/boss-redesign-v5/skitter_matriarch/runtime-smoke'
OUT.mkdir(parents=True, exist_ok=True)
WORK = B / 'matriarch-runtime-windows'
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
                    f'gfx_refresh_rate 60; snd_enable 0; player_name MatriarchQA; connect 127.0.0.1:{game_port}')
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

    spawn = cmd('ib_test_spawn 0 0', 1.5)
    assert 'matriarch act=' in spawn, spawn
    states = {'spawn': re.findall(r'matriarch act=[^\n]*', spawn)}
    capture(hwnd, '00-spawn')
    # Free AI for a few seconds (skitter, stab, pounce, spit as it chooses).
    for i in range(8):
        cmd('ib_test_refill', .35)
        capture(hwnd, f'01-ai-{i}')
        states.setdefault('ai', []).extend(re.findall(r'matriarch act=[^\n]*', cmd('ib_test_state', 0)))
    acts = {2: 'stab', 3: 'pounce', 4: 'spit', 5: 'brood', 6: 'roar', 9: 'thrash'}
    for act, name in acts.items():
        cmd('ib_test_refill', .05)
        cmd(f'ib_test_action {act}', .05)
        for k, delay in enumerate((.2, .35, .35)):
            time.sleep(delay)
            capture(hwnd, f'02-{name}-{k}')
        states[name] = re.findall(r'matriarch act=[^\n]*', cmd('ib_test_state', 0))
        time.sleep(.6)
    # Break parts by damage and finish it.
    cmd('ib_test_refill', .05)
    cmd('ib_test_damage 1700 0', .8)
    capture(hwnd, '03-phase2')
    cmd('ib_test_damage 1700 0', 1.6)
    capture(hwnd, '04-phase3')
    states['phase3'] = re.findall(r'matriarch act=[^\n]*', cmd('ib_test_state', 0))
    cmd('ib_test_refill', .05)
    cmd('ib_test_damage 1000000 0', .6)
    capture(hwnd, '05-death')
    time.sleep(2.6)
    dead = cmd('ib_test_state')
    assert 'matriarch act=' not in dead, dead
    assert server.poll() is None and client.poll() is None
    report = {'status': 'PASS', 'environment': 'Windows Debug server + SDL/OpenGL client from build-v4', 'frames': frames,
              'states': states, 'removed_after_death': True}
    (OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print('MATRIARCH WINDOWS SMOKE PASS:', len(frames), 'frames')
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
