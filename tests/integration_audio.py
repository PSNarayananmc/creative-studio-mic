#!/usr/bin/python3
"""Real PulseAudio server integration, isolated from the user's server."""
import array
import math
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'src'))
import creative_mic as cm

def wait_until(callback, seconds=5):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        try:
            if callback():
                return
        except (OSError, subprocess.SubprocessError):
            pass
        time.sleep(.05)
    raise RuntimeError('Timed out waiting for audio server')

def main():
    with tempfile.TemporaryDirectory(prefix='creative-mic-test-') as temp:
        socket = str(Path(temp)/'pulse.sock')
        cm.ENV['PULSE_SERVER'] = 'unix:' + socket
        os.environ['PULSE_SERVER'] = cm.ENV['PULSE_SERVER']
        log = open(Path(temp)/'server.log', 'w+')
        server = subprocess.Popen(['pulseaudio','-n','--daemonize=no','--exit-idle-time=-1',
                                   '--log-target=stderr','--load=module-native-protocol-unix socket='+socket+' auth-anonymous=1'],
                                  stdout=log, stderr=log)
        engine = cm.Engine()
        other = []
        processes = []
        try:
            wait_until(lambda: cm.pactl('info'))
            print(cm.pactl('info'))
            sink = cm.pactl('load-module','module-null-sink','sink_name=test_input_sink','rate=48000','channels=1').strip()
            other.append(sink)
            source = cm.pactl('load-module','module-remap-source','master=test_input_sink.monitor',
                              'source_name=test_physical','channels=1','source_properties=device.description=Test_Microphone').strip()
            other.append(source)
            wait_until(lambda: 'test_physical' in dict(cm.sources()))
            engine.start('test_physical', 0, 1, 'podcast')
            wait_until(lambda: engine.source in cm.pactl('list','short','sources'))
            assert engine.source not in dict(cm.sources()), 'Own virtual microphone was offered as input'
            assert 'Description: Creative Mic\n' in cm.pactl('list','sources'), 'Incorrect display name'
            with open(Path(temp)/'received.pcm','w+b') as received, open(Path(temp)/'tone.pcm','w+b') as tone:
                samples = array.array('h', [int(10000*math.sin(i*2*math.pi*440/48000)) for i in range(48000*5)])
                if sys.byteorder != 'little':
                    samples.byteswap()
                tone.write(samples.tobytes())
                tone.seek(0)
                recorder = subprocess.Popen(['parec','--device='+engine.source,'--raw','--rate=48000','--channels=1','--format=s16le','--latency-msec=20'],
                                            stdout=received, stderr=log, env=cm.ENV)
                processes.append(recorder)
                producer = subprocess.Popen(['pacat','--playback','--device=test_input_sink','--raw','--rate=48000','--channels=1','--format=s16le','--latency-msec=20'],
                                            stdin=tone, stderr=log, env=cm.ENV)
                processes.append(producer)
                end = time.monotonic()+3
                while time.monotonic()<end:
                    engine.check()
                    assert recorder.poll() is None
                    time.sleep(.1)
                recorder.terminate()
                recorder.wait(timeout=3)
                received.seek(0)
                data = received.read()
                output = array.array('h')
                output.frombytes(data[:len(data)//2*2])
                assert len(output)>4800, 'No delivered PCM'
                assert max(abs(x) for x in output)>1000, 'Only silence delivered'
                print('Virtual microphone delivered %d PCM samples, peak %d' % (len(output), max(abs(x) for x in output)))
            engine.stop()
            modules = cm.pactl('list','short','modules')
            assert engine.token not in modules, 'Owned modules remained'
            assert all(any(line.startswith(m+'\t') for line in modules.splitlines()) for m in other), 'Unrelated modules removed'
            # Exercise the actual CLI signal handlers and per-user lock cleanup.
            baseline = cm.pactl('list','short','modules')
            for sig in (signal.SIGINT, signal.SIGTERM):
                cli = subprocess.Popen([sys.executable, str(Path(cm.__file__)), '--headless',
                                        '--source', 'test_physical'], env=cm.ENV,
                                       stdout=log, stderr=log)
                processes.append(cli)
                wait_until(lambda: 'creative_mic_source_' in cm.pactl('list','short','sources'))
                assert cli.poll() is None
                cli.send_signal(sig)
                assert cli.wait(timeout=10) == 0
                assert cm.pactl('list','short','modules') == baseline, 'CLI signal cleanup left modules'
            print('PASS: CLI SIGINT and SIGTERM cleanup')
            # Native process failure is detected and cleanup still succeeds.
            engine.start('test_physical', 1, 1)
            engine.children[1].kill()
            engine.children[1].wait(timeout=3)
            try:
                engine.check()
                raise AssertionError('Child failure was not detected')
            except RuntimeError:
                pass
            engine.stop()
            assert engine.token not in cm.pactl('list','short','modules')
            # Server disconnection is observed; shutdown tolerates missing server.
            engine.start('test_physical',1,1)
            server.terminate()
            server.wait(timeout=5)
            try:
                engine.last_probe = 0
                engine.check()
                raise AssertionError('Server disconnection was not detected')
            except (RuntimeError, OSError, subprocess.SubprocessError):
                pass
            engine.stop()
            print('PASS: routing, source exclusion, owned cleanup, child failure, server disconnection')
        except BaseException:
            log.flush()
            log.seek(0)
            print(log.read(), file=sys.stderr)
            raise
        finally:
            engine.stop()
            for proc in processes + [server]:
                if proc.poll() is None:
                    proc.terminate()
                    try:
                        proc.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.wait()
            log.close()

if __name__ == '__main__':
    main()
