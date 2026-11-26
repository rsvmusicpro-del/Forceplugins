// Clean-room Butterworth HP/LP cascade inspired by bx_cleansweep Pro's
// selectable 1st–6th-order Butterworth mode. No proprietary code or coefficients.
// Bilinear-transform RBJ sections are designed at control rate.
// SPDX-License-Identifier: MIT
#ifndef STRATTTRACK_CLEANSWEEP_CASCADE_H
#define STRATTTRACK_CLEANSWEEP_CASCADE_H
#include <cmath>
namespace stratt { namespace dsp {
class CleansweepButterworth {
public:
 enum class Mode { Highpass, Lowpass };
 explicit CleansweepButterworth(Mode mode):mode_(mode){}
 void set(double sampleRate,float cutoffHz,int order){
  if(!std::isfinite(sampleRate)||sampleRate<8000.0||sampleRate>384000.0)sampleRate=44100.0;
  const double sr=sampleRate; double fc=std::isfinite(cutoffHz)?cutoffHz:20.0;
  if(fc<1.0)fc=1.0; if(fc>0.45*sr)fc=0.45*sr;
  if(order<1)order=1; if(order>6)order=6;
  firstOrder_=(order&1)!=0; sectionCount_=order/2;
  if(firstOrder_){const double k=std::tan(3.14159265358979323846*fc/sr),inv=1.0/(1.0+k);
   firstB0_=static_cast<float>(mode_==Mode::Lowpass?k*inv:inv);
   firstB1_=static_cast<float>(mode_==Mode::Lowpass?k*inv:-inv);
   firstA1_=static_cast<float>((k-1.0)*inv);}
  const double w=2.0*3.14159265358979323846*fc/sr,c=std::cos(w),s=std::sin(w);
  for(int i=0;i<sectionCount_;++i){const double angle=(2.0*i+1.0)*3.14159265358979323846/(2.0*order);
   sections_[i].set(c,s,1.0/(2.0*std::sin(angle)),mode_);}
 }
 void reset(){firstX1_=firstY1_=0.0f;for(int i=0;i<3;++i)sections_[i].reset();}
 float process(float x){
  if(!std::isfinite(x)){reset();return 0.0f;}
  if(firstOrder_){const float y=firstB0_*x+firstB1_*firstX1_-firstA1_*firstY1_;
   if(!std::isfinite(y)){reset();return 0.0f;}firstX1_=x;firstY1_=y;x=y;}
  for(int i=0;i<sectionCount_;++i)x=sections_[i].process(x);
  return std::isfinite(x)?x:0.0f;
 }
private:
 struct Section{
  float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
  void set(double c,double s,double q,Mode mode){
   const double alpha=s/(2.0*q),a0=1.0+alpha,inv=1.0/a0;double n0,n1,n2;
   if(mode==Mode::Lowpass){n0=(1.0-c)*0.5;n1=1.0-c;n2=n0;}
   else{n0=(1.0+c)*0.5;n1=-(1.0+c);n2=n0;}
   b0=static_cast<float>(n0*inv);b1=static_cast<float>(n1*inv);b2=static_cast<float>(n2*inv);
   a1=static_cast<float>(-2.0*c*inv);a2=static_cast<float>((1.0-alpha)*inv);
  }
  float process(float x){const float y=b0*x+z1,n1=b1*x-a1*y+z2,n2=b2*x-a2*y;
   if(!std::isfinite(y)||!std::isfinite(n1)||!std::isfinite(n2)){reset();return 0.0f;}
   z1=n1;z2=n2;return y;}
  void reset(){z1=z2=0.0f;}
 };
 Mode mode_;bool firstOrder_=false;int sectionCount_=0;
 float firstB0_=1,firstB1_=0,firstA1_=0,firstX1_=0,firstY1_=0;Section sections_[3];
};
}} // namespace stratt::dsp
#endif
