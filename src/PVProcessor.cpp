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
        // FIX (bug #2 - broken overlap-add): outRing is now sized N (was 2N) and this write uses
        // the SAME modulus (N) as the single-sample read at the top of process() (idx=pos, which
        // only ever ranges 0..N-1). Previously this wrote with modulo 2N, so for any analysis frame
        // that didn't start exactly at pos==0, part (often most) of its contribution landed at
        // indices >= N that the read side could never reach - silently discarding a large chunk of
        // the synthesized signal every hop. A same-modulus circular buffer is the standard, correct
        // way to do overlap-add here; the read-then-zero at the top of process() (outRing[read]=0)
        // is what makes it safe to keep re-accumulating into the same wrapped buffer.
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
    p.push_back(std::make_unique<juce::AudioParameterBool>("magic","Magic",true));
    p.push_back(std::make_unique<P>("tape","Tape",juce::NormalisableRange<float>(0,.20f,.001f),.08f));
    p.push_back(std::make_unique<P>("chorus","Chorus",juce::NormalisableRange<float>(0,.20f,.001f),.06f));
    return {p.begin(),p.end()};
}

void PVAudioProcessor::prepareToPlay(double sampleRate,int)
{
    sr=sampleRate;
    up.prepare(std::pow(2.0,10.0/1200.0));
    down.prepare(std::pow(2.0,-10.0/1200.0));
    up.reset(); down.reset();
    tapeHpState = 0.f;
    // Enough headroom for the chorus delay (center + depth, see chorusVoice) at any sample rate.
    const int chorusBufSize = (int)(sampleRate*0.05) + 16;
    chorusBufL.assign((size_t)chorusBufSize, 0.f);
    chorusBufR.assign((size_t)chorusBufSize, 0.f);
    chorusWriteL = 0; chorusWriteR = 0;
    // FIX (latency not reported): the phase vocoder needs a full analysis window (N samples)
    // before its output is musically meaningful, so the plugin has real inherent latency. Without
    // reporting it, the host won't time-align (PDC) this track against unprocessed ones - for a
    // doubling effect specifically, that misalignment is very audible. up.N and down.N are always
    // equal (both PVocoder instances use the same fixed N), so either can be used here.
    setLatencySamples(up.N);
}

// UPGRADE (real chorus): a single modulated-delay voice - writes the input into a circular buffer,
// reads it back from a delay time that oscillates sinusoidally (lfoPhase), with linear interpolation
// between samples for a smooth (click-free) modulated delay. Two of these (one per channel, driven
// with a phase-offset LFO) is the standard, classic way to build a chorus, replacing the previous
// amplitude-modulation approximation.
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
    if(apvts.getRawParameterValue("bypass")->load()>.5f) return;
    const bool magic=apvts.getRawParameterValue("magic")->load()>.5f;
    const float tape=apvts.getRawParameterValue("tape")->load();
    const float chorus=apvts.getRawParameterValue("chorus")->load();
    const int n=b.getNumSamples();
    std::vector<float> x((size_t)n);
    for(int i=0;i<n;++i) x[(size_t)i]=b.getSample(0,i);

    // UPGRADE (tape saturation with nicer high-end harmonics): the signal is split into a low/mid
    // band and a high band (~3kHz crossover) using a simple one-pole lowpass. The low/mid band gets
    // a gentle classic soft-clip (like before). The high band is driven harder with a touch of
    // asymmetry, so the extra harmonics it generates are themselves high-frequency content - a
    // pleasant "shimmer/air" rather than the old full-band tanh, which (being fed one mixed signal)
    // mostly reacted to low-frequency energy and gave comparatively little distinct top-end character.
    auto tapeFx=[&](float v){
        const float drive = 1.f + 5.0f*tape;
        const float hpCoeff = 1.f - std::exp(-2.f*juce::MathConstants<float>::pi*3000.f/(float)sr);
        tapeHpState += hpCoeff * (v - tapeHpState);
        const float lowPart = tapeHpState;
        const float highPart = v - tapeHpState;

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
        float c=tapeFx(x[(size_t)i]), l=0,r=0;
        if(magic){ l=down.process(x[(size_t)i],sr); r=up.process(x[(size_t)i],sr); }
        float outL = c + l*.30f;
        float outR = c + r*.30f;

        // UPGRADE (real chorus): true modulated-delay chorus on the finished stereo pair, replacing
        // the old amplitude-modulation approximation. The two channels' LFOs are offset in phase
        // (ph vs ph+1.7) exactly like before, which is what gives the effect stereo movement/width
        // rather than both channels wobbling identically.
        ph += juce::MathConstants<double>::twoPi*0.35/sr;
        const float wetL = chorusVoice(chorusBufL, chorusWriteL, outL, ph);
        const float wetR = chorusVoice(chorusBufR, chorusWriteR, outR, ph+1.7);
        outL = outL*(1.f-chorus) + wetL*chorus;
        outR = outR*(1.f-chorus) + wetR*chorus;

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

// FIX (bug #1 - missing link-time symbol): JUCE's plugin wrapper code calls this factory function
// to create the processor instance; without a definition anywhere in the linked sources, the build
// fails at the link stage with an unresolved external symbol error. This was missing entirely.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PVAudioProcessor();
}
