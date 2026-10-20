/* Creative Mic, MIT license. RNNoise remains BSD licensed. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "rnnoise.h"
#include "voice.h"
static int number(const char *s, float *v, float max) {
 char *end; *v=strtof(s,&end); return *s && !*end && isfinite(*v) && *v>=0 && *v<=max;
}
int main(int argc,char **argv) {
 int input_clipped=0;
 int podcast=0; Voice voice; voice_init(&voice);
 float mix=1,gain=1,in[480],wet[480],dry[480]={0}; unsigned char bytes[960];
 for(int a=1;a<argc;a+=2) {
  if(a+1>=argc) goto usage;
  if(!strcmp(argv[a],"--mix")) { if(!number(argv[a+1],&mix,1)) goto usage; }
  else if(!strcmp(argv[a],"--gain")) { if(!number(argv[a+1],&gain,4)) goto usage; }
  else if(!strcmp(argv[a],"--profile")) {
   if(!strcmp(argv[a+1],"podcast")) podcast=1;
   else if(!strcmp(argv[a+1],"clean")) podcast=0;
   else goto usage;
  }
  else goto usage;
 }
 DenoiseState *st=rnnoise_create(); if(!st) return 3;
 for(;;) {
  size_t n=0,k; while(n<960 && (k=fread(bytes+n,1,960-n,stdin))) n+=k;
  if(!n) { rnnoise_destroy(st); return ferror(stdin)?3:0; }
  if(n!=960) { fprintf(stderr,"Truncated PCM frame\n"); rnnoise_destroy(st); return 2; }
  for(int i=0;i<480;i++) in[i]=(int16_t)((unsigned)bytes[2*i]|((unsigned)bytes[2*i+1]<<8));
  if(!input_clipped) for(int i=0;i<480;i++) {
   if(in[i]>=32760 || in[i]<=-32760) {
    fprintf(stderr,"Input clipping detected: lower microphone hardware gain.\n");
    fflush(stderr); input_clipped=1; break;
   }
  }
  rnnoise_process_frame(st,wet,in);
  for(int i=0;i<480;i++) {
   /* RNNoise v0.1 overlap-add emits the previous 480-sample window. */
   float v=gain*((1-mix)*dry[i]+mix*wet[i]);
   if(podcast) v=voice_process(&voice,v);
   v=fmaxf(-32768,fminf(32767,v));
   int16_t s=(int16_t)lrintf(v); bytes[2*i]=(uint16_t)s&255; bytes[2*i+1]=(uint16_t)s>>8;
   dry[i]=in[i];
  }
  if(fwrite(bytes,1,960,stdout)!=960 || fflush(stdout)) {rnnoise_destroy(st); return 3;}
 }
usage: fprintf(stderr,"Usage: creative-mic-filter [--mix 0..1] [--gain 0..4] [--profile clean|podcast]\n"); return 2;
}
