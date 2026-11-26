// Antiderivative antialiasing for StrattTrack's Drive/Circuit saturator.
// The static transfer preserves the previous rational curve; ADAA evaluates
// its interval average without oversampling, allocation or sample-rate latency.
#pragma once
#include <algorithm>
#include <cmath>
namespace stratt { namespace dsp {
class CircuitAdaa {
public:
    void reset(){previous_=0.0;hasPrevious_=false;}
    static double staticTransfer(double x,double drive){
        const double d=clampDrive(drive), k=0.22*d*(1.0+5.0*d);
        return x/(1.0+k*std::fabs(x));
    }
    static double antiderivative(double x,double drive){
        const double d=clampDrive(drive), k=0.22*d*(1.0+5.0*d);
        if(k<=1.0e-12) return 0.5*x*x;
        const double a=std::fabs(x), z=k*a;
        if(z<0.01){
            const double g=0.5+z*(-1.0/3.0+z*(0.25+z*(-0.2+z*(1.0/6.0))));
            return a*a*g;
        }
        return (z-fastLog1p(z))/(k*k);
    }
    float process(float input,float drive){
        if(!std::isfinite(input)){reset();return 0.0f;}
        const double x=std::max(-64.0,std::min(64.0,static_cast<double>(input)));
        const double d=clampDrive(drive), k=0.22*d*(1.0+5.0*d);
        if(k<=1.0e-12){previous_=x;hasPrevious_=true;return static_cast<float>(x);}
        const double previous=hasPrevious_?previous_:x, delta=x-previous;
        const double scale=std::max(1.0,std::max(std::fabs(x),std::fabs(previous)));
        const double y=std::fabs(delta)<=1.0e-5*scale
            ?staticTransfer(0.5*(x+previous),d)
            :(antiderivative(x,d)-antiderivative(previous,d))/delta;
        previous_=x;hasPrevious_=true;
        if(!std::isfinite(y)){reset();return 0.0f;}
        return static_cast<float>(y);
    }
private:
    static double clampDrive(double d){return std::isfinite(d)?std::max(0.0,std::min(1.0,d)):0.0;}
    // frexp range reduction keeps |t| <= 1/3; atanh terms through t^13
    // approximate log1p to better than 1.3e-8 across the supported range.
    static double fastLog1p(double z){
        int e=0; double m=std::frexp(1.0+z,&e); m*=2.0; --e;
        const double t=(m-1.0)/(m+1.0), t2=t*t;
        const double lm=2.0*t*(1.0+t2*(1.0/3.0+t2*(1.0/5.0+t2*(1.0/7.0
            +t2*(1.0/9.0+t2*(1.0/11.0+t2*(1.0/13.0)))))));
        return static_cast<double>(e)*0.69314718055994530942+lm;
    }
    double previous_=0.0; bool hasPrevious_=false;
};
}} // namespace stratt::dsp
