#!/usr/bin/env python3
"""Independent ffmpeg ebur128 oversampled peak measurement, synthetic signals."""
import array,json,math,os,random,re,subprocess as sp,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
rng=random.Random(97)
signals={
 'near_nyquist':[int(32760*math.sin(i*2*math.pi*17777/48000)) for i in range(48000*3)],
 'fullscale_random':[rng.randint(-32768,32767) for i in range(48000*3)],
 'alternating_fullscale':[32767 if i%2 else -32768 for i in range(48000*3)],
 'transients':[32767 if i%997==0 else 0 for i in range(48000*3)],
}
results={}
for name,samples in signals.items():
    output=sp.check_output([str(root/'build-native/voicecraft-pcm'),'--dry'],input=array.array('h',samples).tobytes())
    measured=sp.run(['ffmpeg','-hide_banner','-f','s16le','-ar','48000','-ac','1','-i','pipe:0','-af','ebur128=peak=true','-f','null','-'],input=output,stdout=sp.PIPE,stderr=sp.PIPE,check=True).stderr.decode()
    peak=float(re.findall(r'Peak:\s+(-?[\d.]+) dBFS',measured)[-1]);assert peak<=-2.9,(name,peak)
    results[name]={'independent_true_peak_dbfs':peak,'frames':len(samples)//480}
    print(name,peak,'dBTP')
(root/'evidence/limiter-peaks.json').write_text(json.dumps(results,indent=2)+'\n')
