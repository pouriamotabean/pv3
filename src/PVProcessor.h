#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

class PVAudioProcessor final : public juce::AudioProcessor
{
public:
    PVAudioProcessor();
    ~PVAudioProcessor() override = default;
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PV"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct PVocoder
    {
        int N = 2048, H = 512, pos = 0;
        double ratio = 1.0;
        std::vector<float> inRing, outRing, window, prevPhase, sumPhase, fftIn, fftOut;
        std::unique_ptr<juce::dsp::FFT> fft;

        void prepare(double r)
        {
            ratio = r;
            inRing.assign(N,0); outRing.assign(N,0);
            window.resize(N); prevPhase.assign(N/2+1,0); sumPhase.assign(N/2+1,0);
            fftIn.resize(2*N); fftOut.resize(2*N);
            fft = std::make_unique<juce::dsp::FFT>((int)std::log2(N));
            for(int i=0;i<N;++i) window[i]=0.5f-0.5f*std::cos(juce::MathConstants<float>::twoPi*i/(N-1));
        }
        void reset()
        {
            std::fill(inRing.begin(),inRing.end(),0); std::fill(outRing.begin(),outRing.end(),0);
            std::fill(prevPhase.begin(),prevPhase.end(),0); std::fill(sumPhase.begin(),sumPhase.end(),0);
            pos=0;
        }
        float process(float x, double sr);
    };

    double sr = 44100.0;
    double chorusPhase = 0.0;
    PVocoder up, down;

    // UPGRADE (tape saturation): one-pole lowpass state used to split the signal into a low/mid
    // band and a high band, so the two can be saturated differently (see processBlock) - this is
    // what lets the added harmonics sit "in the highs" instead of reacting mostly to bass energy.
    // FIX (real bug - separate per channel): Tape is now applied to the full stereo (post-Magic)
    // signal, and L/R can genuinely differ once Magic>0 - sharing one state would leak filter memory
    // between channels, same class of bug as the chorus buffers below (which already are per-channel).
    float tapeHpStateL = 0.f, tapeHpStateR = 0.f;

    // UPGRADE (real chorus): true modulated-delay chorus replacing the old amplitude-modulation
    // approximation. One delay line per output channel; sized for sample rate in prepareToPlay.
    std::vector<float> chorusBufL, chorusBufR;
    int chorusWriteL = 0, chorusWriteR = 0;
    float chorusVoice(std::vector<float>& buf, int& writePos, float input, double lfoPhase);

    // FIX (real bug found in review - this is very likely the actual cause of "bypass doesn't work /
    // a bad noise appears"): the bypass branch in processBlock returns BEFORE the phase vocoders ever
    // get fed a sample - exactly the same "goes stale while not being fed audio" problem already fixed
    // for the Magic knob, just triggered by BYPASS instead. While bypassed, up/down's internal ring
    // buffers and accumulated phase sit untouched; resuming from that stale state when bypass turns
    // off again is a classic phase-vocoder glitch source, and in the worst case (extended silence
    // while stale) can drift into NaN/Inf, which would show up as a *loud, sustained* noise, exactly
    // matching what was reported - and would explain the noise appearing "even at Magic=0", since
    // NaN*0 is still NaN, not 0. Tracking the bypass transition lets processBlock fully reset
    // everything the instant bypass turns off, rather than resuming from whatever state was left over.
    bool wasBypassed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PVAudioProcessor)
};
