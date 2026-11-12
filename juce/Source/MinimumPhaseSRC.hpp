#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <stdexcept>

// LadioCast2's Sources/AudioCore/MinimumPhaseSRC.hpp (0.4.2) with its vDSP_fft_zipD replaced by the portable fft()
// below, so that it builds for Android and Linux too; the rest is kept diffable with that copy.
namespace patchsrc {
// In-place radix-2 FFT over re.size() (a power of two) points, unscaled like vDSP_fft_zipD: forward e^-i, inverse e^+i.
inline void fft(std::vector<double> &re,std::vector<double> &im,bool inverse) {
    constexpr double pi=3.14159265358979323846;
    const size_t n=re.size();
    for(size_t i=1,j=0;i<n;++i){
        size_t bit=n>>1;
        for(;j&bit;bit>>=1)j^=bit;
        j^=bit;
        if(i<j){std::swap(re[i],re[j]);std::swap(im[i],im[j]);}
    }
    std::vector<double> c(n/2),s(n/2);
    for(size_t k=0;k<n/2;++k){const double a=2*pi*double(k)/double(n);c[k]=std::cos(a);s[k]=inverse?std::sin(a):-std::sin(a);}
    for(size_t len=2;len<=n;len<<=1){
        const size_t half=len/2,stride=n/len;
        for(size_t i=0;i<n;i+=len)
            for(size_t k=0;k<half;++k){
                const double wr=c[k*stride],wi=s[k*stride];
                const size_t a=i+k,b=a+half;
                const double tr=re[b]*wr-im[b]*wi,ti=re[b]*wi+im[b]*wr;
                re[b]=re[a]-tr;im[b]=im[a]-ti;re[a]+=tr;im[a]+=ti;
            }
    }
}

// Control-thread construction only. One oversampled prototype is spectrally
// factored before phase decomposition; independently factoring phase rows would
// destroy their fractional-time relationship.
struct MinimumPhaseKernel {
    static constexpr int Phases=256;
    int taps=0;
    std::vector<float> coefficients;
    explicit MinimumPhaseKernel(double step) {
        if(!std::isfinite(step)||step<=0||step>24) throw std::invalid_argument("SRC ratio");
        const double scale=std::max(1.0,step*1.003); // clock tracking headroom
        const int half=int(std::ceil(48*scale));
        taps=half*3+1;
        const int oversample=Phases;
        int log2=1;while((1<<log2)<taps*oversample*4)++log2;
        const int n=1<<log2;
        std::vector<double> re(n,0),im(n,0);
        constexpr double pi=3.14159265358979323846;
        const double cutoff=.90/scale;
        for(int j=0;j<=2*half*oversample;++j) {
            const double x=double(j)/oversample-half,u=x/half;
            const double w=.35875+.48829*std::cos(pi*u)+.14128*std::cos(2*pi*u)+.01168*std::cos(3*pi*u);
            re[j]=(std::abs(x)<1e-12?cutoff:std::sin(pi*cutoff*x)/(pi*x))*w/oversample;
        }
        fft(re,im,false);
        for(int j=0;j<n;++j){re[j]=std::log(std::max(std::hypot(re[j],im[j]),1e-12));im[j]=0;}
        fft(re,im,true);
        for(int j=0;j<n;++j){re[j]*=(j==0||j==n/2?1.0:j<n/2?2.0:0.0)/n;im[j]=0;}
        fft(re,im,false);
        for(int j=0;j<n;++j){const double a=std::exp(re[j]),p=im[j];re[j]=a*std::cos(p);im[j]=a*std::sin(p);}
        fft(re,im,true);
        coefficients.resize((Phases+1)*taps);
        for(int phase=0;phase<=Phases;++phase){
            double sum=0;
            for(int k=0;k<taps;++k){const double v=re[k*oversample+phase]*oversample/n;coefficients[phase*taps+k]=float(v);sum+=v;}
            for(int k=0;k<taps;++k)coefficients[phase*taps+k]/=float(sum);
        }
    }
    template<class Read> void sample(double position,Read read,float &left,float &right) const noexcept {
        const auto whole=static_cast<uint64_t>(position);
        const double phase=(position-double(whole))*Phases;
        const int row=std::min(int(phase),Phases-1);
        const float blend=float(phase-row);
        const float *a=coefficients.data()+row*taps,*b=a+taps;
        left=right=0;
        for(int k=0;k<taps;++k){
            if(whole<static_cast<uint64_t>(k))break; // zero history at stream start
            const float weight=a[k]+(b[k]-a[k])*blend;
            left+=read(whole-k,0)*weight;right+=read(whole-k,1)*weight;
        }
    }
};

// MS2000R's own: the kernel streamed. L R interleaved frames in buf, the kernel's history (silence at the start) kept
// in front of the read position pos; at 1:1 frames are copied, not filtered. prepare() builds the kernel (allocates).
struct MinimumPhaseStream {
    std::unique_ptr<const MinimumPhaseKernel> kernel;
    std::vector<float> buf;
    double pos=0,step=1;
    size_t hist=0;
    void prepare(double inRate,double outRate,size_t reserveFrames) {
        step=inRate/outRate;
        kernel.reset(std::abs(step-1)<1e-9?nullptr:new MinimumPhaseKernel(step));
        if(!kernel)step=1;
        hist=kernel?size_t(kernel->taps-1):0;
        buf.assign(hist*2,0.f);buf.reserve((hist+reserveFrames)*2);
        pos=double(hist);
    }
    size_t frames() const noexcept {return buf.size()/2;}
    double ahead() const noexcept {return double(frames())-pos;}   // frames past the read position
    void push(float l,float r) {buf.push_back(l);buf.push_back(r);}
    // One output frame; false (and no advance) when it needs a frame not pushed yet.
    bool next(float &l,float &r) noexcept {
        const size_t at=size_t(pos);
        if(at>=frames())return false;
        if(kernel)kernel->sample(pos,[this](uint64_t i,int c){return buf[size_t(i)*2+size_t(c)];},l,r);
        else{l=buf[at*2];r=buf[at*2+1];}
        pos+=step;
        return true;
    }
    void trim() noexcept {
        const size_t drop=std::min(frames(),size_t(pos)>hist?size_t(pos)-hist:size_t(0));
        buf.erase(buf.begin(),buf.begin()+std::ptrdiff_t(drop*2));
        pos-=double(drop);
    }
};
}
