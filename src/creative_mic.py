#!/usr/bin/python3
"""Offline RNNoise microphone routing, compatible with Python 3.8 / PA13."""
import argparse
import fcntl
import math
import os
import re
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import uuid

ENV = dict(os.environ, LC_ALL='C', LANG='C')

def pactl(*args):
    return subprocess.check_output(['pactl'] + list(args), env=ENV,
                                   stderr=subprocess.PIPE, timeout=5).decode()

def parse_sources(text):
    result = []
    entry = None
    for line in text.splitlines():
        if line.startswith('Source #'):
            if entry:
                result.append(entry)
            entry = {}
        elif entry is not None and ':' in line:
            key, value = line.strip().split(':', 1)
            if key in ('Name', 'Description', 'Monitor of Sink'):
                entry[key] = value.strip()
    if entry:
        result.append(entry)
    return [(e['Name'], e.get('Description', e['Name'])) for e in result
            if 'Name' in e and e.get('Monitor of Sink') == 'n/a'
            and not e['Name'].endswith('.monitor')
            and not e['Name'].startswith('creative_mic_')
            and e.get('Description') != 'Creative Mic']

def sources():
    return parse_sources(pactl('list', 'sources'))

class InstanceLock:
    def __init__(self):
        # Use a private per-user directory, including when XDG_RUNTIME_DIR is absent.
        directory = Path(tempfile.gettempdir()) / ('creative-mic-' + str(os.getuid()))
        try:
            directory.mkdir(mode=0o700)
        except FileExistsError:
            pass
        st = directory.lstat()
        if directory.is_symlink() or st.st_uid != os.getuid() or st.st_mode & 0o077:
            raise RuntimeError('Unsafe lock directory')
        fd = os.open(str(directory / 'instance.lock'), os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
        self.file = os.fdopen(fd, 'w')
        try:
            fcntl.flock(self.file, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            self.file.close()
            raise RuntimeError('Creative Mic is already running for this user')
    def close(self):
        self.file.close()

class Engine:
    def __init__(self):
        self.modules = []
        self.children = []
        self.errors = []
        self.active = False
        self.last_probe = 0
        self.warning = ""
        self.token = uuid.uuid4().hex
        self.sink = 'creative_mic_sink_' + self.token
        self.source = 'creative_mic_source_' + self.token

    def load(self, *args):
        module = pactl('load-module', *args).strip()
        if not module.isdigit():
            raise RuntimeError('Invalid module identifier returned by audio server')
        self.modules.append(module)

    def spawn(self, args, stdin=None, stdout=None):
        errors = tempfile.TemporaryFile()
        self.errors.append(errors)
        proc = subprocess.Popen(args, stdin=stdin, stdout=stdout, stderr=errors,
                                env=ENV, close_fds=True)
        self.children.append(proc)
        return proc

    def start(self, selected, mix, gain, profile="podcast"):
        if self.active or self.children or self.modules:
            raise RuntimeError('Engine already started')
        try:
            if selected not in dict(sources()):
                raise RuntimeError('Select an available physical microphone')
            self.load('module-null-sink', 'sink_name=' + self.sink,
                      'rate=48000', 'channels=1', 'channel_map=mono',
                      'sink_properties=device.description=Creative_Mic_Internal')
            self.load('module-remap-source', 'master=' + self.sink + '.monitor',
                      'source_name=' + self.source, 'channels=1', 'channel_map=mono',
                      "source_properties='device.description=\"Creative Mic\"'")
            options = ['--raw', '--format=s16le', '--rate=48000', '--channels=1', '--latency-msec=20']
            capture = self.spawn(['parec', '--device=' + selected] + options, stdout=subprocess.PIPE)
            binary = Path(__file__).resolve().parent / 'creative-mic-filter'
            if not binary.exists():
                binary = Path(__file__).resolve().parents[1] / 'build/creative-mic-filter'
            try:
                filtered = self.spawn([str(binary), '--mix', str(mix), '--gain', str(gain), '--profile', profile],
                                      stdin=capture.stdout, stdout=subprocess.PIPE)
            finally:
                capture.stdout.close()
            try:
                self.spawn(['pacat', '--playback', '--device=' + self.sink] + options,
                           stdin=filtered.stdout, stdout=subprocess.DEVNULL)
            finally:
                filtered.stdout.close()
            self.warning = ""
            self.active = True
            self.last_probe = 0
        except BaseException:
            self.stop()
            raise

    def check(self):
        if len(self.errors) >= 2:
            diagnostic = os.pread(self.errors[1].fileno(), 4096, 0)
            if b'Input clipping detected' in diagnostic:
                self.warning = 'Input is clipping — lower your microphone hardware gain.'
        for proc in self.children:
            if proc.poll() is not None:
                raise RuntimeError('Audio process exited (code %s). Check microphone and audio server.' % proc.returncode)
        if time.monotonic() - self.last_probe > 1:
            pactl('info')
            self.last_probe = time.monotonic()

    def stop(self):
        self.active = False
        for proc in self.children:
            if proc.poll() is None:
                try:
                    proc.terminate()
                except ProcessLookupError:
                    pass
        for proc in self.children:
            try:
                proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        self.children.clear()
        # Verify owner token before unloading: IDs can be reused after a server restart.
        try:
            live = {line.split('\t')[0]: line for line in pactl('list', 'short', 'modules').splitlines()}
        except (OSError, subprocess.SubprocessError):
            live = {}
        # A signal can interrupt pactl after a module loaded but before its ID
        # reached Python. Discover all modules bearing our unique instance token.
        def owned_line(line):
            fields = line.split('\t')
            if len(fields) < 3:
                return False
            expected = {'module-null-sink': ('sink_name', self.sink),
                        'module-remap-source': ('source_name', self.source)}
            if fields[1] not in expected:
                return False
            key, value = expected[fields[1]]
            return re.search(r'(?:^|\s)' + key + '=' + re.escape(value) + r'(?:\s|$)', fields[2]) is not None
        owned = [module for module, line in live.items() if owned_line(line)]
        for module in reversed(owned):
            if self.token in live.get(module, ''):
                try:
                    pactl('unload-module', module)
                except (OSError, subprocess.SubprocessError):
                    pass
        self.modules.clear()
        for stream in self.errors:
            stream.close()
        self.errors.clear()


def bounded(maximum):
    def parse(value):
        result = float(value)
        if not math.isfinite(result) or not 0 <= result <= maximum:
            raise argparse.ArgumentTypeError('Expected a finite number from 0 to %s' % maximum)
        return result
    return parse


def gui(engine, args):
    import tkinter as tk
    from tkinter import ttk
    root = tk.Tk()
    root.title('Creative Mic')
    root.resizable(False, False)
    box = ttk.Frame(root, padding=20)
    box.grid()
    ttk.Label(box, text='Creative Mic', font=('Sans', 20, 'bold')).grid(sticky='w')
    ttk.Label(box, text='Offline voice cleanup and level control · RNNoise').grid(sticky='w', pady=(0, 16))
    ttk.Label(box, text='Physical microphone').grid(sticky='w')
    device = ttk.Combobox(box, state='readonly', width=52)
    device.grid(sticky='ew')
    choices = []
    status = tk.StringVar(value='Ready')
    def refresh():
        try:
            choices[:] = sources()
            device['values'] = [description + ' [' + name + ']' for name, description in choices]
            if choices:
                device.current(0)
            else:
                device.set('')
                status.set('No physical microphones available')
        except Exception as exc:
            status.set(str(exc))
    refresh_button = ttk.Button(box, text='Refresh devices', command=refresh)
    refresh_button.grid(sticky='w', pady=8)
    ttk.Label(box, text='Voice processing').grid(sticky='w')
    profile = ttk.Combobox(box, state='readonly', values=['podcast', 'clean'], width=20)
    profile.set(args.profile)
    profile.grid(sticky='w')
    ttk.Label(box, text='Podcast: rumble filter, gentle EQ, compressor and peak limiter.').grid(sticky='w')
    mix = tk.DoubleVar(value=args.mix)
    gain = tk.DoubleVar(value=args.gain)
    ttk.Label(box, text='Suppression mix: dry 0% — wet 100%').grid(sticky='w')
    mix_slider = ttk.Scale(box, from_=0, to=1, variable=mix)
    mix_slider.grid(sticky='ew')
    ttk.Label(box, text='Dry/wet mixing; does not change model aggressiveness.').grid(sticky='w')
    ttk.Label(box, text='Gain (start at 1×; high values can distort your microphone)').grid(sticky='w', pady=(12, 0))
    gain_control = ttk.Spinbox(box, from_=0, to=4, increment=0.1, textvariable=gain, width=8)
    gain_control.grid(sticky='w')
    def stop():
        engine.stop()
        device.config(state='readonly')
        profile.config(state='readonly')
        refresh_button.config(state='normal')
        mix_slider.config(state='normal')
        gain_control.config(state='normal')
        start_button.config(state='normal')
        status.set('Stopped')
    def start():
        try:
            idx = device.current()
            if idx < 0:
                raise RuntimeError('Select a physical microphone first')
            engine.start(choices[idx][0], bounded(1)(str(mix.get())), bounded(4)(str(gain.get())), profile.get())
            for control in (device, profile, refresh_button, mix_slider, gain_control, start_button):
                control.config(state='disabled')
            status.set('Running — select Creative Mic in your calling app')
        except Exception as exc:
            stop()
            status.set(str(exc))
    start_button = ttk.Button(box, text='Start', command=start)
    start_button.grid(sticky='w', pady=(16, 0))
    ttk.Button(box, text='Stop', command=stop).grid(sticky='e', row=start_button.grid_info()['row'])
    ttk.Label(box, textvariable=status, wraplength=460).grid(sticky='w', pady=10)
    ttk.Label(box, text='Start Creative Mic → select it as input in Discord, Zoom, OBS\nor your browser → keep this app running.\nUse headphones. Turn off mic monitoring and Discord’s mic test.\nNo speaker isolation, echo cancellation, or dereverberation.',
              wraplength=460).grid(sticky='w')
    root.protocol('WM_DELETE_WINDOW', root.destroy)
    def tick():
        if engine.active:
            try:
                engine.check()
                if engine.warning:
                    status.set(engine.warning)
            except Exception as exc:
                stop()
                status.set(str(exc))
        root.after(100, tick)
    refresh()
    tick()
    if args.gui_smoke_test:
        root.after(250, root.destroy)
    root.mainloop()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', action='version', version='Creative Mic 1.0.2')
    parser.add_argument('--list-sources', action='store_true')
    parser.add_argument('--headless', action='store_true')
    parser.add_argument('--source')
    parser.add_argument('--profile', choices=['podcast', 'clean'], default='podcast',
                        help='podcast adds EQ, compression and a -3 dBFS peak limiter; clean is noise suppression only')
    parser.add_argument('--mix', type=bounded(1), default=1.0)
    parser.add_argument('--gain', type=bounded(4), default=1.0)
    parser.add_argument('--gui-smoke-test', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    engine = Engine()
    lock = None
    def interrupted(signum, frame):
        raise KeyboardInterrupt()
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
    try:
        if args.list_sources:
            for name, description in sources():
                print(name + '\t' + description)
            return 0
        lock = InstanceLock()
        if args.headless:
            if not args.source:
                parser.error('--headless requires --source; use --list-sources')
            engine.start(args.source, args.mix, args.gain, args.profile)
            print('Creative Mic running. Select it in your calling app. Ctrl+C to stop.', flush=True)
            warned = False
            while True:
                engine.check()
                if engine.warning and not warned:
                    print(engine.warning, file=sys.stderr, flush=True)
                    warned = True
                time.sleep(0.1)
        else:
            gui(engine, args)
    except KeyboardInterrupt:
        return 0
    except Exception as exc:
        print('Creative Mic: ' + str(exc), file=sys.stderr)
        return 1
    finally:
        engine.stop()
        if lock:
            lock.close()
    return 0

if __name__ == '__main__':
    sys.exit(main())
