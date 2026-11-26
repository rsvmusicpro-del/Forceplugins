#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include "circuit_adaa.h"
static double ref(double x,double d){d=std::max(0.0,std::min(1.0,d));const double k=0.22*d*(1+5*d);return x/(1+k*std::fabs(x));}
static double simpson(double lo,double hi,double d){
    const int n=4000;const double h=(hi-lo)/n;double s=ref(lo,d)+ref(hi,d);
    for(int i=1;i<n;++i){s+=(i&1?4.0:2.0)*ref(lo+i*h,d);}
    return s*h/(3*(hi-lo));
}
int main(){
    const double ds[]={0,0.001,0.05,0.2,0.5,0.75,1};
    for(double d:ds)for(int i=0;i<=1000;++i){
        const double x=-64+128.0*i/1000,h=1e-5*std::max(1.0,std::fabs(x));
        const double deriv=(stratt::dsp::CircuitAdaa::antiderivative(x+h,d)-stratt::dsp::CircuitAdaa::antiderivative(x-h,d))/(2*h);
        if(std::fabs(deriv-ref(x,d))>2e-6*std::max(1.0,std::fabs(ref(x,d)))){std::fprintf(stderr,"primitive mismatch d=%g x=%g\n",d,x);return 1;}
    }
    for(int j=0;j<7;++j)for(int i=0;i<120;++i){
        const double a=i*0.6180339887498949+0.13*j,b=i*0.414213562373095+0.21*j;
        const double x=-64+128*(a-std::floor(a)),p=-64+128*(b-std::floor(b)),d=ds[j];
        if(std::fabs(x-p)<1e-5)continue;
        stratt::dsp::CircuitAdaa stage;stage.process(static_cast<float>(p),static_cast<float>(d));
        const double actual=stage.process(static_cast<float>(x),static_cast<float>(d));
        const double expected=simpson(std::min(x,p),std::max(x,p),d);
        if(std::fabs(actual-expected)>2e-5){std::fprintf(stderr,"ADAA mismatch d=%g x=%g p=%g\n",d,x,p);return 2;}
    }
    for(double d:ds){stratt::dsp::CircuitAdaa s;const float x=.375f;s.process(x,static_cast<float>(d));
        const float y=s.process(x+1e-7f,static_cast<float>(d));if(!std::isfinite(y)||std::fabs(y-ref(x,d))>2e-5)return 3;}
    stratt::dsp::CircuitAdaa s;if(s.process(.375f,0)!=.375f)return 4;
    if(s.process(std::numeric_limits<float>::quiet_NaN(),1)!=0)return 5;
    if(!std::isfinite(s.process(.25f,1)))return 6;
    std::puts("StrattTrack Drive/Circuit ADAA and numerical stability OK");return 0;
}
