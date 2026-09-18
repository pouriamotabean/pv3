#include "PVProcessor.h"
#include "PVEditor.h"

float PVAudioProcessor::PVocoder::process(float x, double sr)
{
    inRing[(size_t)pos] = x;
    const int read = pos;
    float y = outRing[(size_t)read];
    outRing[(size_t)read] = 0.0f;

    if (pos % H == 0)
    {
        for (int i=0;i<N;++i)
        {
            int idx = (pos - i + N) % N;
            fftIn[2*i] = inRing[(size_t)idx] * window[(size_t)i];
            fftIn[2*i+1] = 0.0f;
        }

        fft->performRealOnlyForwardTransform(fftIn.data());

        std::fill(fftOut.begin(), fftOut.end(), 0.0f);
        const double expected = juce::MathConstants<double>::twoPi * H / N;

        for (int k=0;k<=N/2;++k)
        {
            const double re=fftIn[2*k], im=fftIn[2*k+1];
            const double mag=std::sqrt(re*re+im*im);
            double phase=std::atan2(im,re);
            double delta=phase-prevPhase[(size_t)k]-expected*k;
            delta=juce::jlimit(-juce::MathConstants<double>::pi,
                               juce::MathConstants<double>::pi, delta);
            const double trueFreq=juce::MathConstants<double>::twoPi*k/N + delta/H;
            const double newBin=k*ratio;
            if (newBin <= N/2-2)
            {
                const int b=(int)std::floor(newBin);
                const double frac=newBin-b;
                const double targetPhase = sumPhase[(size_t)k] + trueFreq*H*ratio;
                sumPhase[(size_t)k]=targetPhase;
                prevPhase[(size_t)k]=phase;

                const float a=(float)(mag*std::cos(targetPhase));
                const float q=(float)(mag*std::sin(targetPhase));
                fftOut[2*b] += a*(float)(1-frac); fftOut[2*b+1] += q*(float)(1-frac);
                fftOut[2*(b+1)] += a*(float)frac; fftOut[2*(b+1)+1] += q*(float)frac;
            }
        }

        fft->performRealOnlyInverseTransform(fftOut.data());
        const float norm = 1.0f / (float)(N * 0.5);
        for (int i=0;i<N;++i)
        {
            int idx=(pos+i)%N;
            outRing[(size_t)idx] += fftOut[2*i]*window[(size_t)i]*norm;
        }
    }

    pos=(pos+1)%N;
    juce::ignoreUnused(sr);
    return y;
}

PVAudioProcessor::PVAudioProcessor()
 : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::mono(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
   apvts(*this,nullptr,"PV",createParameterLayout()) {}

juce::AudioProcessorValueTreeState::ParameterLayout PVAudioProcessor::createParameterLayout()
{
    using P=juce::AudioParameterFloat;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterBool>("bypass","Bypass",false));
    // FIX (requested): Magic is no longer an on/off switch - it's a knob controlling the volume of
    // JUST the two pitched/panned voices (0 = silent, i.e. audibly identical to bypass; 1.0 = those
    // two voices reach full/unity level with the centre line). Default 0, per the explicit request
    // that a freshly-opened plugin makes no audible difference until you turn something up.
    p.push_back(std::make_unique<P>("magic","Magic",juce::NormalisableRange<float>(0.f,1.f,0.001f),0.f));
    p.push_back(std::make_unique<P>("tape","Tape",juce::NormalisableRange<float>(0,.20f,.001f),0.f));
    p.push_back(std::make_unique<P>("chorus","Chorus",juce::NormalisableRange<float>(0,.20f,.001f),0.f));
    return {p.begin(),p.end()};
}

void PVAudioProcessor::prepareToPlay(double sampleRate,int)
{
    sr=sampleRate;
    up.prepare(std::pow(2.0,10.0/1200.0));
    down.prepare(std::pow(2.0,-10.0/1200.0));
    up.reset(); down.reset();
    tapeHpStateL = 0.f; tapeHpStateR = 0.f;
    const int chorusBufSize = (int)(sampleRate*0.05) + 16;
    chorusBufL.assign((size_t)chorusBufSize, 0.f);
    chorusBufR.assign((size_t)chorusBufSize, 0.f);
    chorusWriteL = 0; chorusWriteR = 0;
    // The phase vocoder needs a full analysis window (N samples) before its output is musically
    // meaningful, so the plugin has real inherent latency - reported so the host time-aligns (PDC)
    // this track against unprocessed ones, which matters a lot for a doubling effect specifically.
    setLatencySamples(up.N);
}

float PVAudioProcessor::chorusVoice(std::vector<float>& buf, int& writePos, float input, double lfoPhase)
{
    const float centerMs = 18.0f, depthMs = 6.0f;
    const float delayMs = centerMs + depthMs * (float)std::sin(lfoPhase);
    const float delaySamples = delayMs * 0.001f * (float)sr;
    const int bufSize = (int)buf.size();

    buf[(size_t)writePos] = input;

    float readPos = (float)writePos - delaySamples;
    while (readPos < 0.f) readPos += (float)bufSize;
    const int i0 = (int)readPos;
    const int i1 = (i0 + 1) % bufSize;
    const float frac = readPos - (float)i0;
    const float out = buf[(size_t)i0] * (1.f - frac) + buf[(size_t)i1] * frac;

    writePos = (writePos + 1) % bufSize;
    return out;
}

bool PVAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    auto in=l.getMainInputChannelSet(), out=l.getMainOutputChannelSet();
    return in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo();
}

void PVAudioProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n=b.getNumSamples();

    // FIX (real bug - BYPASS did nothing to the buffer): this plugin is mono-in/stereo-out, so
    // channel 1 of the buffer handed to processBlock has no real input source behind it at all - it
    // can legitimately contain stale/uninitialised memory depending on the host. The old bypass just
    // `return`ed without touching the buffer, so that channel's leftover content went straight to the
    // output - exactly matching "bypass doesn't work / sometimes a loud noise until I turn it off".
    // Bypass now explicitly builds a correct, clean mono-to-stereo passthrough every time.
    if(apvts.getRawParameterValue("bypass")->load()>.5f)
    {
        for(int i=0;i<n;++i){ float v=b.getSample(0,i); b.setSample(0,i,v); if(b.getNumChannels()>1) b.setSample(1,i,v); }
        wasBypassed=true;
        return;
    }
    if(wasBypassed)
    {
        // Coming out of bypass - everything that has internal state gets a clean restart instead of
        // resuming from however stale it got while it wasn't being fed audio.
        up.reset(); down.reset();
        tapeHpStateL=0.f; tapeHpStateR=0.f;
        std::fill(chorusBufL.begin(),chorusBufL.end(),0.f);
        std::fill(chorusBufR.begin(),chorusBufR.end(),0.f);
        chorusWriteL=0; chorusWriteR=0;
        wasBypassed=false;
    }

    const float magic=apvts.getRawParameterValue("magic")->load();
    const float tape=apvts.getRawParameterValue("tape")->load();
    const float chorus=apvts.getRawParameterValue("chorus")->load();
    std::vector<float> x((size_t)n);
    for(int i=0;i<n;++i) x[(size_t)i]=b.getSample(0,i);

    // FIX (real bug - click/noise when raising Magic from 0): the two Phase Vocoders used to only be
    // fed samples while Magic was on, so their internal state (ring-buffer position, accumulated
    // phase) went stale the instant Magic was off, then resumed from that stale, discontinuous state
    // the instant it was turned back on - a classic source of a phase-vocoder glitch/transient. They
    // now ALWAYS process every sample, unconditionally, keeping their internal state continuously
    // "warm" - Magic only controls how much of their (always valid) output gets mixed in below, which
    // is a completely safe place to scale by a plain multiply, no discontinuity risk at all.
    auto tapeFx=[&](float v, float& hpState){
        const float drive = 1.f + 5.0f*tape;
        const float hpCoeff = 1.f - std::exp(-2.f*juce::MathConstants<float>::pi*3000.f/(float)sr);
        hpState += hpCoeff * (v - hpState);
        const float lowPart = hpState;
        const float highPart = v - hpState;

        const float lowSat = std::tanh(lowPart*drive) / std::tanh(drive);

        const float hiDrive = drive * 1.8f;
        const float hiSat = std::tanh(highPart*hiDrive + 0.15f*highPart*highPart) / std::tanh(hiDrive);

        const float wet = lowSat + hiSat*1.4f;
        return juce::jlimit(-1.f, 1.f, v*(1.f-tape) + wet*tape);
    };

    juce::AudioBuffer<float> o(2,n); o.clear();
    double ph = chorusPhase;
    for(int i=0;i<n;++i)
    {
        // FIX (requested - chain order): Magic first (centre + the two pitched/panned voices, scaled
        // by the Magic knob, 0=silent up to 1.0=unity with centre) -> Tape applied to that FULL
        // stereo result (not just the centre, like before) -> Chorus last.
        const float l=down.process(x[(size_t)i],sr), r=up.process(x[(size_t)i],sr);
        float outL = x[(size_t)i] + l*magic;
        float outR = x[(size_t)i] + r*magic;

        outL = tapeFx(outL, tapeHpStateL);
        outR = tapeFx(outR, tapeHpStateR);

        ph += juce::MathConstants<double>::twoPi*0.35/sr;
        const float wetL = chorusVoice(chorusBufL, chorusWriteL, outL, ph);
        const float wetR = chorusVoice(chorusBufR, chorusWriteR, outR, ph+1.7);
        outL = outL*(1.f-chorus) + wetL*chorus;
        outR = outR*(1.f-chorus) + wetR*chorus;

        if(!std::isfinite(outL)) outL=0.f;
        if(!std::isfinite(outR)) outR=0.f;
        o.setSample(0,i,outL); o.setSample(1,i,outR);
    }
    chorusPhase = ph;
    o.applyGain(.78f);
    b.makeCopyOf(o,false);
}

void PVAudioProcessor::getStateInformation(juce::MemoryBlock& d)
{
    if(auto x=apvts.copyState().createXml()) copyXmlToBinary(*x,d);
}
void PVAudioProcessor::setStateInformation(const void* data,int size)
{
    if(auto x=getXmlFromBinary(data,size))
        if(x->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*x));
}
juce::AudioProcessorEditor* PVAudioProcessor::createEditor(){return new PVAudioProcessorEditor(*this);}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PVAudioProcessor();
}
