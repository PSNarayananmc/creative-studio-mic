"""Actual OBS Studio 25 PulseAudio source, Xvfb software renderer, MKV output."""
import json
import os
from pathlib import Path
import subprocess as sp
import time

def verify_obs(temp,env,virtual,producer,pactl):
    config=Path(env['XDG_CONFIG_HOME'])/'obs-studio';profile=config/'basic/profiles/VoiceCraftTest';scenes=config/'basic/scenes';profile.mkdir(parents=True);scenes.mkdir(parents=True)
    (config/'global.ini').write_text('[General]\nEnableAutoUpdates=false\nFirstRun=true\n\n[Basic]\nProfile=VoiceCraftTest\nProfileDir=VoiceCraftTest\nSceneCollection=VoiceCraftTest\nSceneCollectionFile=VoiceCraftTest\n\n[BasicWindow]\nPreviewEnabled=false\n')
    (profile/'basic.ini').write_text('[General]\nName=VoiceCraftTest\n\n[Video]\nBaseCX=640\nBaseCY=360\nOutputCX=640\nOutputCY=360\nFPSType=0\nFPSCommon=10\n\n[Audio]\nSampleRate=48000\nChannelSetup=Mono\n\n[Output]\nMode=Simple\n\n[SimpleOutput]\nFilePath='+str(temp)+'\nRecFormat=mkv\nRecQuality=Stream\nStreamEncoder=x264\nVBitrate=300\nABitrate=128\nUseAdvanced=false\n\n[Hotkeys]\nOBSBasic.StopRecording={"bindings":[{"key":"OBS_KEY_F10"}]}\n')
    source={'name':'VoiceCraft Studio Microphone','id':'pulse_input_capture','settings':{'device_id':virtual},'mixers':1,'volume':1.0,'enabled':True}
    scene={'name':'VoiceCraft test scene','id':'scene','settings':{'items':[{'name':source['name'],'visible':True,'id':1,'pos':{'x':0,'y':0},'scale':{'x':1,'y':1}}]}}
    (scenes/'VoiceCraftTest.json').write_text(json.dumps({'name':'VoiceCraftTest','current_scene':scene['name'],'current_program_scene':scene['name'],'scene_order':[{'name':scene['name']}],'sources':[source,scene]}))
    obsenv=env.copy();obsenv.update(DISPLAY=':88',LIBGL_ALWAYS_SOFTWARE='1')
    logpath=Path(os.environ.get('VC_EVIDENCE_DIR',Path(__file__).resolve().parents[1]/'evidence'))/'obs-console.log'
    with open(logpath,'w') as log:
        x=sp.Popen(['Xvfb',':88','-screen','0','1280x800x24'],env=env,stdout=log,stderr=log);obs=None
        try:
            time.sleep(1)
            obs=sp.Popen(['obs','--startrecording','--profile','VoiceCraftTest','--collection','VoiceCraftTest','--disable-updater'],env=obsenv,stdout=log,stderr=log)
            time.sleep(8)
            print('OBS source-outputs:',pactl('list','short','source-outputs'),flush=True)
            assert obs.poll() is None,'OBS exited early'
            producer();time.sleep(7)
            sp.run(['xdotool','search','--onlyvisible','--class','obs','windowfocus','key','F10'],env=obsenv,check=True,stdout=sp.DEVNULL)
            time.sleep(3)
            videos=list(temp.glob('*.mkv'));assert videos,'OBS did not create an MKV; inspect evidence/obs-console.log'
            recording=max(videos,key=lambda p:p.stat().st_mtime)
            decoded=sp.check_output(['ffmpeg','-v','error','-i',str(recording),'-vn','-f','f32le','-ac','1','-ar','48000','pipe:1'])
            import array
            samples=array.array('f');samples.frombytes(decoded);peak=max(map(abs,samples));assert len(samples)>48000 and peak>.02 and peak<.72,(len(samples),peak)
            root=Path(__file__).resolve().parents[1]
            (Path(os.environ.get('VC_EVIDENCE_DIR',root/'evidence'))/'obs-recording.json').write_text(json.dumps({'obs_version':sp.check_output(['obs','--version'],text=True).strip(),'samples':len(samples),'decoded_peak':peak,'source':virtual,'recording_bytes':recording.stat().st_size,'synthetic_test':True},indent=2)+'\n')
            import shutil
            shutil.copyfile(recording,Path(os.environ.get('VC_EVIDENCE_DIR',root/'evidence'))/'obs-synthetic.mkv')
            print('OBS recorded %d samples; decoded peak %.6f'%(len(samples),peak),flush=True)
        finally:
            if obs and obs.poll() is None:
                obs.terminate()
                try:obs.wait(timeout=5)
                except sp.TimeoutExpired:obs.kill();obs.wait()
            x.terminate();x.wait(timeout=5)
