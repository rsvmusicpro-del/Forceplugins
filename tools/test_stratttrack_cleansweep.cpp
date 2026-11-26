#include <cmath>
#include <cstdio>
#include <limits>
#include "cleansweep_cascade.h"
static double measure(stratt::dsp::CleansweepButterworth::Mode mode,int order,double sr,double fc){
 stratt::dsp::CleansweepButterworth f(mode);f.set(sr,static_cast<float>(fc),order);
 const int total=static_cast<int>(sr*0.5),begin=total-8192;double in=0,out=0;
 for(int i=0;i<total;++i){const float x=0.1f*static_cast<float>(std::sin(2.0*3.14159265358979323846*fc*i/sr));
  const float y=f.process(x);
  if(!std::isfinite(y)) return -1;
  if(i>=begin){in+=static_cast<double>(x)*x;out+=static_cast<double>(y)*y;}}
 return std::sqrt(out/in);
}
int main(){const double rates[]={44100,48000,96000};
 for(int r=0;r<3;++r)for(int n=1;n<=6;++n){const double sr=rates[r],fc=1000;
  const double hp=measure(stratt::dsp::CleansweepButterworth::Mode::Highpass,n,sr,fc);
  const double lp=measure(stratt::dsp::CleansweepButterworth::Mode::Lowpass,n,sr,fc);
  if(std::fabs(hp-0.7071067811865476)>0.012||std::fabs(lp-0.7071067811865476)>0.012){
   std::fprintf(stderr,"corner mismatch sr=%.0f order=%d HP=%.8f LP=%.8f\n",sr,n,hp,lp);return 1;}
  stratt::dsp::CleansweepButterworth h(stratt::dsp::CleansweepButterworth::Mode::Highpass),l(stratt::dsp::CleansweepButterworth::Mode::Lowpass);
  h.set(sr,static_cast<float>(0.45*sr),6);l.set(sr,static_cast<float>(0.45*sr),6);
  for(int i=0;i<4096;++i){const float x=0.25f*static_cast<float>(std::sin(0.013*i));if(!std::isfinite(h.process(x))||!std::isfinite(l.process(x)))return 2;}
 }
 stratt::dsp::CleansweepButterworth bad(stratt::dsp::CleansweepButterworth::Mode::Lowpass);bad.set(48000,1000,99);
 if(!std::isfinite(bad.process(std::numeric_limits<float>::quiet_NaN())))return 3;
 std::puts("StrattTrack Cleansweep Butterworth orders 1-6 OK");return 0;
}
