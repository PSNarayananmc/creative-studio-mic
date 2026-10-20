/* Creative Mic voice shaping, MIT license. No echo or reverb effects. */
#include <stdint.h>
#define LOOKAHEAD 240
#define CEILING 23197.9f /* -3 dBFS sample-peak ceiling */
typedef struct { float b0,b1,b2,a1,a2,z1,z2; } Biquad;
static float biquad(Biquad *f,float x) {
 float y=f->b0*x+f->z1;
 f->z1=f->b1*x-f->a1*y+f->z2;
 f->z2=f->b2*x-f->a2*y;
 return y;
}
static void eq_init(Biquad *f,float frequency,float q,float db,int highpass) {
 float w=2*M_PI*frequency/48000, c=cosf(w), alpha=sinf(w)/(2*q);
 float a=powf(10,db/40), a0;
 if(highpass) {
  a0=1+alpha; f->b0=(1+c)/2; f->b1=-(1+c); f->b2=(1+c)/2;
  f->a1=-2*c; f->a2=1-alpha;
 } else {
  a0=1+alpha/a; f->b0=1+alpha*a; f->b1=-2*c; f->b2=1-alpha*a;
  f->a1=-2*c; f->a2=1-alpha/a;
 }
 f->b0/=a0; f->b1/=a0; f->b2/=a0; f->a1/=a0; f->a2/=a0;
}
typedef struct {
 Biquad rumble, mud, presence;
 float power, compressor, limiter;
 float delay[LOOKAHEAD+1];
 float peaks[LOOKAHEAD+1];
 uint64_t position;
} Voice;
static void voice_init(Voice *v) {
 memset(v,0,sizeof(*v));
 eq_init(&v->rumble,80,.707f,0,1);
 eq_init(&v->mud,250,.8f,-2,0);
 eq_init(&v->presence,3000,.8f,1.5f,0);
 v->compressor=v->limiter=1;
}
static float voice_process(Voice *v,float input) {
 float x=biquad(&v->presence,biquad(&v->mud,biquad(&v->rumble,input)));
 float squared=x*x;
 float power_coeff=squared>v->power ? .995842f : .999861f; /* 5 / 150 ms */
 v->power=power_coeff*v->power+(1-power_coeff)*squared;
 float level=10*log10f(fmaxf(v->power,1e-12f)/(32768.f*32768.f));
 float over=level+18, reduction=0;
 /* 3:1 compressor, -18 dBFS threshold and 6 dB soft knee. */
 if(over>3) reduction=over*(2.f/3);
 else if(over>-3) reduction=(over+3)*(over+3)/18;
 float target=powf(10,-reduction/20);
 float coeff=target<v->compressor ? .989637f : .999861f;
 v->compressor=coeff*v->compressor+(1-coeff)*target;
 x*=v->compressor*1.2589254f; /* fixed 2 dB makeup, never adaptive amplification */
 unsigned index=v->position%(LOOKAHEAD+1);
 v->delay[index]=x; v->peaks[index]=fabsf(x);
 float peak=0;
 for(unsigned i=0;i<LOOKAHEAD+1;i++) peak=fmaxf(peak,v->peaks[i]);
 target=peak>CEILING ? CEILING/peak : 1;
 /* Immediate safety reduction with 80 ms recovery; 5 ms lookahead. */
 v->limiter=fminf(target,.99974f*v->limiter+.00026f);
 float y=v->position<LOOKAHEAD ? 0 : v->delay[(v->position-LOOKAHEAD)%(LOOKAHEAD+1)]*v->limiter;
 v->position++;
 return fmaxf(-CEILING,fminf(CEILING,y));
}
