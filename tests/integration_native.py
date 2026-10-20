#!/usr/bin/env python3
"""Real native application + isolated PulseAudio, no physical device needed."""
import array
import json
import math
import os
from pathlib import Path
import signal
import subprocess as sp
import sys
import tempfile
import time

ROOT=Path(__file__).resolve().parents[1]
BIN=Path(os.environ.get('VOICECRAFT_BIN', ROOT/'build-native/voicecraft-studio'))

def wait(fn, timeout=12):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        try:
            result=fn()
            if result:return result
        except (sp.SubprocessError,OSError):pass
        time.sleep(.1)
    raise AssertionError('Readiness timed out')

def main():
    results=[]
    def passed(name):results.append(name);print('PASS:',name,flush=True)
    with tempfile.TemporaryDirectory(prefix='vc-native-') as tmp:
        temp=Path(tmp);env=os.environ.copy();env['PULSE_SERVER']='unix:'+str(temp/'pulse.sock');env['LC_ALL']='C'
        env['XDG_CONFIG_HOME']=str(temp/'config');env['XDG_RUNTIME_DIR']=str(temp/'run');Path(env['XDG_RUNTIME_DIR']).mkdir(mode=0o700)
        log=open(temp/'pulse.log','w+')
        processes=[];server=None;app=None
        def pactl(*args):return sp.check_output(['pactl',*args],env=env,text=True,stderr=sp.DEVNULL)
        def start_server():
            nonlocal server
            server=sp.Popen(['pulseaudio','-n','--daemonize=no','--exit-idle-time=-1','--log-target=stderr','--load=module-native-protocol-unix socket='+str(temp/'pulse.sock')+' auth-anonymous=1'],env=env,stdout=log,stderr=log);processes.append(server)
            wait(lambda:pactl('info'))
            pactl('load-module','module-null-sink','sink_name=vc_test_input','rate=48000','channels=1')
            return pactl('load-module','module-remap-source','master=vc_test_input.monitor','source_name=vc_test_mic','channels=1','source_properties=device.description=Test_Microphone').strip()
        def source_name():
            for row in pactl('list','short','sources').splitlines():
                name=row.split('\t')[1]
                if name.startswith('voicecraft_source_'):return name
        def start_app(extra=()):
            nonlocal app
            app=sp.Popen([str(BIN),'--headless','--source','vc_test_mic',*extra],env=env,stdout=metrics,stderr=app_log);processes.append(app)
            wait(source_name);wait(lambda:'VoiceCraft microphone input' in pactl('list','source-outputs'))
            return app
        tone=array.array('f',[.12*math.sin(i*2*math.pi*440/48000) for i in range(48000*12)])
        tonefile=temp/'tone.f32';tonefile.write_bytes(tone.tobytes())
        def producer():
            f=open(tonefile,'rb');p=sp.Popen(['pacat','--playback','--device=vc_test_input','--raw','--rate=48000','--channels=1','--format=float32le','--latency-msec=20'],stdin=f,env=env,stdout=sp.DEVNULL,stderr=log);f.close();processes.append(p);return p
        app_log=open(temp/'app.log','w+');metrics=open(temp/'metrics.jsonl','w+')
        try:
            module=start_server();baseline=pactl('list','short','modules');default_before=[x for x in pactl('info').splitlines() if x.startswith('Default Source:')]
            app=start_app();virtual=source_name();assert 'Description: VoiceCraft Studio Microphone\n' in pactl('list','sources')
            listed=sp.check_output([str(BIN),'--list-sources'],env=env,text=True)
            assert virtual not in listed and '.monitor' not in listed and 'vc_test_mic' in listed
            passed('actual virtual source name, physical-source enumeration and feedback exclusion')
            capture=open(temp/'received.f32','wb');reader=sp.Popen(['parec','--device='+virtual,'--raw','--rate=48000','--channels=1','--format=float32le','--latency-msec=20'],env=env,stdout=capture,stderr=log);processes.append(reader);toneprocess=producer();time.sleep(4);reader.terminate();reader.wait(timeout=5);capture.close()
            data=array.array('f');data.frombytes((temp/'received.f32').read_bytes());peak=max(map(abs,data));assert len(data)>4800 and peak>.02 and peak<=.708
            print('Delivered %d float PCM samples; peak %.6f'%(len(data),peak),flush=True);passed('processed audio delivered through native PulseAudio virtual microphone')
            duplicate=sp.run([str(BIN),'--headless','--source','vc_test_mic','--seconds','1'],env=env,capture_output=True);assert duplicate.returncode==1;passed('per-user duplicate-instance protection')
            app.send_signal(signal.SIGINT);assert app.wait(timeout=8)==0;assert pactl('list','short','modules')==baseline;passed('SIGINT removes only owned modules')
            app=start_app();app.terminate();assert app.wait(timeout=8)==0;assert pactl('list','short','modules')==baseline;passed('repeated START/STOP and SIGTERM cleanup')
            toneprocess.terminate();toneprocess.wait(timeout=5)
            app=start_app();pactl('unload-module',module);time.sleep(3);assert app.poll() is None
            module=pactl('load-module','module-remap-source','master=vc_test_input.monitor','source_name=vc_test_mic','channels=1','source_properties=device.description=Test_Microphone').strip()
            wait(source_name);wait(lambda:'VoiceCraft microphone input' in pactl('list','source-outputs'));passed('selected source removal/reappearance automatically reconnects')
            server.terminate();server.wait(timeout=5);time.sleep(2);module=start_server();wait(source_name);wait(lambda:'VoiceCraft microphone input' in pactl('list','source-outputs'));passed('audio server restart and module-ID reuse recovery')
            # Keep load representative and verify bounded memory after warmup.
            producer();time.sleep(3)
            def rss():return int(next(x.split()[1] for x in Path('/proc/%d/status'%app.pid).read_text().splitlines() if x.startswith('VmRSS:')))
            before=rss();time.sleep(float(os.environ.get('VC_SOAK_SECONDS','15')));after=rss();assert after-before<8192 and after<102400
            print('Headless RSS warm %d KiB -> %d KiB (host CPU, not J1800)'%(before,after),flush=True);passed('bounded-memory synthetic soak below 100 MB')
            assert [x for x in pactl('info').splitlines() if x.startswith('Default Source:')]==default_before
            passed('system default microphone unchanged')
            if os.environ.get('VC_TEST_OBS')=='1':
                from obs_capture import verify_obs
                verify_obs(temp,env,virtual=source_name(),producer=producer,pactl=pactl)
                passed('OBS PulseAudio source records processed audio into an actual MKV')
            app.terminate();assert app.wait(timeout=8)==0;assert 'voicecraft_' not in pactl('list','short','modules')
            passed('final owned-module cleanup')
            metrics.flush();app_log.flush();(Path(os.environ.get('VC_EVIDENCE_DIR',ROOT/'evidence'))/'native-metrics.jsonl').write_text((temp/'metrics.jsonl').read_text());(Path(os.environ.get('VC_EVIDENCE_DIR',ROOT/'evidence'))/'native-runtime.log').write_text((temp/'app.log').read_text())
            (Path(os.environ.get('VC_EVIDENCE_DIR',ROOT/'evidence'))/'native-results.json').write_text(json.dumps({'passed':results,'pcm_samples':len(data),'sample_peak':peak,'rss_kib':[before,after],'limitations':['Synthetic source; no USB hardware, target J1800, or physical round-trip measurement.']},indent=2)+'\n')
        except BaseException:
            log.flush();app_log.flush();print((temp/'pulse.log').read_text(),file=sys.stderr);print((temp/'app.log').read_text(),file=sys.stderr);raise
        finally:
            for p in reversed(processes):
                if p.poll() is None:
                    p.terminate()
                    try:p.wait(timeout=5)
                    except sp.TimeoutExpired:p.kill();p.wait()
            log.close();app_log.close();metrics.close()
if __name__=='__main__':main()
