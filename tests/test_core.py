import array
import ast
import math
from pathlib import Path
import random
import subprocess
import sys
import unittest
from unittest.mock import Mock, patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'src'))
import creative_mic as cm
ROOT = Path(__file__).resolve().parents[1]
BIN = str(ROOT / 'build/creative-mic-filter')

def pcm(samples):
    data = array.array('h', samples)
    if sys.byteorder != 'little':
        data.byteswap()
    return data.tobytes()

def decode(data):
    result = array.array('h')
    result.frombytes(data)
    if sys.byteorder != 'little':
        result.byteswap()
    return result

def run(samples, *args):
    return subprocess.run([BIN] + list(args), input=pcm(samples), capture_output=True, check=True).stdout

class PCMTests(unittest.TestCase):
    def test_counts_silence(self):
        for frames in (0, 1, 2, 100):
            output = run([0] * (480 * frames))
            self.assertEqual(output, bytes(960 * frames))

    def test_dry_alignment(self):
        signal = [int(10000 * math.sin(i * .13)) for i in range(480 * 20)]
        self.assertEqual(list(decode(run(signal, '--mix', '0'))), [0]*480 + signal[:-480])

    def test_mixture_alignment(self):
        samples = [int(15000 * math.sin(i * .1)) for i in range(480*60)]
        wet = decode(run(samples))
        mixed = decode(run(samples, '--mix', '.5'))
        dry = [0]*480 + samples[:-480]
        self.assertLessEqual(max(abs(m - .5*(w+d)) for m,w,d in zip(mixed,wet,dry)), 1)

    def test_wet_processing_delay(self):
        rng = random.Random(6)
        samples = [rng.randint(-8000, 8000) for _ in range(480*100)]
        output = decode(run(samples))
        correlations = {lag: sum(output[i]*samples[i-lag] for i in range(480*30,len(output)))
                        for lag in range(470,491)}
        self.assertEqual(max(correlations, key=correlations.get), 480)

    def test_clipping(self):
        output = decode(run([32767, -32768]*480, '--mix', '0', '--gain', '4'))
        self.assertEqual(list(output[480:]), [32767, -32768]*240)

    def test_invalid_and_truncated(self):
        for args in (['--mix','nan'], ['--gain','inf'], ['--mix','1.1'], ['--gain','-1'],
                     ['--bad','0'], ['--mix'], ['--mix','0x'], ['--gain','5']):
            self.assertEqual(subprocess.run([BIN]+args, capture_output=True).returncode, 2)
        for n in (1, 2, 959, 961):
            self.assertEqual(subprocess.run([BIN], input=bytes(n), capture_output=True).returncode, 2)

    def test_deterministic_noise(self):
        rng = random.Random(50)
        samples = [rng.randint(-2000, 2000) for _ in range(480*300)]
        first = run(samples)
        self.assertEqual(first, run(samples))
        out = decode(first)[480*100:]
        inp = samples[480*99:-480]
        ratio = sum(x*x for x in out)/sum(x*x for x in inp)
        print('Synthetic white-noise output/input power ratio: %.6f' % ratio)
        self.assertLess(ratio, .5)

class VoiceTests(unittest.TestCase):
    def test_silence_and_profile_arguments(self):
        self.assertEqual(run([0]*4800, '--profile','podcast'), bytes(9600))
        self.assertEqual(subprocess.run([BIN,'--profile','unknown'], capture_output=True).returncode, 2)

    def test_peak_limiter_on_sustained_and_transient_audio(self):
        rng = random.Random(20)
        inputs = ([32767,-32768]*4800,
                  [0]*960+[32767]+[0]*4800,
                  [rng.randint(-32768,32767) for _ in range(480*40)])
        for samples in inputs:
            samples = samples + [0]*((-len(samples))%480)
            output = decode(run(samples, '--mix','0','--gain','4','--profile','podcast'))
            self.assertEqual(len(output),len(samples))
            self.assertLessEqual(max(abs(x) for x in output),23198)

    def test_loud_voice_is_compressed(self):
        def rms(amplitude):
            samples = [int(amplitude*math.sin(i*2*math.pi*220/48000)) for i in range(48000)]
            output = decode(run(samples,'--mix','0','--profile','podcast'))[24000:]
            return math.sqrt(sum(x*x for x in output)/len(output))
        quiet, loud = rms(1000), rms(16000)
        difference = 20*math.log10(loud/quiet)
        self.assertLess(difference,21) # Input level difference is 24.08 dB.
        self.assertGreater(difference,5) # Preserve dynamics; do not flatten everything.

    def test_rumble_is_reduced(self):
        def power(frequency):
            samples = [int(500*math.sin(i*2*math.pi*frequency/48000)) for i in range(48000)]
            output = decode(run(samples,'--mix','0','--profile','podcast'))[24000:]
            return sum(x*x for x in output)/len(output)
        self.assertLess(power(20)/power(1000), .02)

    def test_input_clip_warning(self):
        result = subprocess.run([BIN,'--profile','podcast'],input=pcm([32767]*480),capture_output=True,check=True)
        self.assertIn(b'Input clipping detected', result.stderr)
        result = subprocess.run([BIN,'--profile','podcast'],input=pcm([1000]*480),capture_output=True,check=True)
        self.assertNotIn(b'Input clipping detected', result.stderr)

    def test_podcast_delay(self):
        samples = [1000]+[0]*(480*4-1)
        output = decode(run(samples,'--mix','0','--profile','podcast'))
        self.assertEqual(list(output[:720]), [0]*720)
        self.assertNotEqual(output[720],0)

class RoutingTests(unittest.TestCase):
    def test_pa13_text(self):
        text = '''Source #0
    Name: alsa_input.usb_1
    Description: USB Microphone
    Monitor of Sink: n/a
Source #1
    Name: alsa_output.monitor
    Description: Monitor
    Monitor of Sink: 0
Source #2
    Name: creative_mic_source_deadbeef
    Description: Creative Mic
    Monitor of Sink: n/a
Source #3
    Name: another_monitor
    Monitor of Sink: 9
'''
        self.assertEqual(cm.parse_sources(text), [('alsa_input.usb_1','USB Microphone')])
        self.assertEqual(cm.ENV['LC_ALL'], 'C')

    @patch.object(cm, 'pactl')
    @patch.object(cm, 'sources', return_value=[('physical','Mic')])
    def test_rollback(self, sources, pactl):
        engine = cm.Engine()
        def call(*args):
            if args[0] == 'load-module':
                if len(engine.modules) == 1:
                    raise RuntimeError('startup failure')
                return '42'
            if args[:3] == ('list','short','modules'):
                return '42\tmodule-null-sink\tsink_name=' + engine.sink
            return ''
        pactl.side_effect = call
        with self.assertRaises(RuntimeError):
            engine.start('physical',1,1)
        self.assertFalse(engine.modules)
        pactl.assert_any_call('unload-module','42')

    @patch.object(cm, 'pactl')
    def test_cleanup_ownership_and_children(self, pactl):
        engine = cm.Engine()
        proc = Mock()
        proc.poll.return_value = None
        engine.children = [proc]
        engine.modules = ['1','2','3']
        pactl.side_effect = lambda *a: ('1\tmodule-null-sink\tsink_name='+engine.sink+'\n2\tmodule-remap-source\tmaster='+engine.sink+'.monitor source_name=unrelated' if a[0]=='list' else '')
        engine.stop()
        proc.terminate.assert_called_once()
        proc.wait.assert_called_once_with(timeout=2)
        pactl.assert_any_call('unload-module','1')
        self.assertNotIn(unittest.mock.call('unload-module','2'), pactl.call_args_list)
        self.assertNotIn(unittest.mock.call('unload-module','3'), pactl.call_args_list)
        engine.stop()
        self.assertEqual(proc.terminate.call_count,1)

    @patch.object(cm, 'pactl')
    @patch.object(cm, 'sources', return_value=[('physical','Mic')])
    def test_process_start_rollback(self, sources, pactl):
        for fail_at in range(3):
            engine = cm.Engine()
            def call(*args):
                if args[0] == 'load-module':
                    return str(len(engine.modules)+1)
                if args[:3] == ('list','short','modules'):
                    return '1\tmodule-null-sink\tsink_name='+engine.sink+'\n2\tmodule-remap-source\tsource_name='+engine.source
                return ''
            pactl.side_effect = call
            processes = []
            def spawn(*args, **kwargs):
                if len(processes) == fail_at:
                    raise OSError('spawn failed')
                proc = Mock()
                proc.stdout = Mock()
                proc.poll.return_value = None
                processes.append(proc)
                return proc
            with patch.object(cm.subprocess, 'Popen', side_effect=spawn):
                with self.assertRaises(OSError):
                    engine.start('physical',1,1)
            for proc in processes:
                proc.terminate.assert_called_once()
                proc.wait.assert_called_once()
            self.assertFalse(engine.children)
            self.assertFalse(engine.modules)
            self.assertFalse(engine.errors)

    @patch.object(cm, 'pactl')
    def test_interrupted_load_cleanup(self, pactl):
        engine = cm.Engine()
        pactl.side_effect = lambda *a: ('77\tmodule-remap-source\tsource_name='+engine.source if a[0]=='list' else '')
        engine.stop()
        pactl.assert_any_call('unload-module','77')

    def test_child_failure(self):
        engine = cm.Engine()
        proc = Mock()
        proc.poll.return_value = 1
        proc.returncode = 1
        engine.children = [proc]
        with self.assertRaisesRegex(RuntimeError,'Audio process exited'):
            engine.check()

    @patch.object(cm,'pactl',side_effect=OSError('disconnected'))
    def test_disconnect(self, pactl):
        engine = cm.Engine()
        with self.assertRaises(OSError):
            engine.check()
        engine.modules = ['1']
        engine.stop()
        self.assertEqual(engine.modules, [])

    def test_single_instance(self):
        lock = cm.InstanceLock()
        try:
            with self.assertRaises(RuntimeError):
                cm.InstanceLock()
        finally:
            lock.close()

    def test_python38_syntax(self):
        ast.parse((ROOT/'src/creative_mic.py').read_text(), feature_version=(3,8))

if __name__ == '__main__':
    unittest.main()
