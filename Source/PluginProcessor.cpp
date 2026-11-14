#include "PluginProcessor.h"
#include "PluginEditor.h"

static const char* kParamIds[] = { "length", "density", "tone", "wet", "only", "dry" };

static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "length", 1 }, "Length",
                 NormalisableRange<float> (0.5f, 6.0f, 0.01f), 2.2f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "density", 1 }, "Density",
                 NormalisableRange<float> (1.0f, 8.0f, 0.01f), 3.5f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tone", 1 }, "Tone",
                 NormalisableRange<float> (1500.0f, 16000.0f, 1.0f, 0.5f), 9000.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "wet", 1 }, "Wet",
                 NormalisableRange<float> (-18.0f, 6.0f, 0.1f), 0.0f));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "only", 1 }, "Only before line", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "dry", 1 }, "Include dry", true));
    return { p.begin(), p.end() };
}

//==============================================================================
void NeptuniumProcessor::Worker::run()
{
    while (! threadShouldExit())
    {
        wait (-1);
        if (threadShouldExit()) break;
        while (wait (250) && ! threadShouldExit()) {}   // debounce knob moves
        if (threadShouldExit()) break;
        owner.doRender();
    }
}

NeptuniumProcessor::NeptuniumProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Neptunium", makeLayout()),
      worker (*this)
{
    for (auto* id : kParamIds)
        apvts.addParameterListener (id, this);
    worker.startThread();
}

NeptuniumProcessor::~NeptuniumProcessor()
{
    for (auto* id : kParamIds)
        apvts.removeParameterListener (id, this);
    worker.signalThreadShouldExit();
    worker.notify();
    worker.stopThread (4000);
}

void NeptuniumProcessor::prepareToPlay (double sampleRate, int)
{
    const double old = hostRate.exchange (sampleRate);
    if (std::abs (old - sampleRate) > 1.0)
        worker.notify();   // re-render at the project's sample rate
}

bool NeptuniumProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

void NeptuniumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;   // audio passes through untouched

    if (! playing.load())
        return;

    const juce::ScopedTryLock tl (previewLock);
    if (! tl.isLocked() || previewBuf.getNumSamples() == 0)
        return;

    const int pos   = playPos.load();
    const int n     = buffer.getNumSamples();
    const int todo  = juce::jmin (n, previewBuf.getNumSamples() - pos);
    if (todo > 0)
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addFrom (ch, 0, previewBuf, juce::jmin (ch, 1), pos, todo);

    playPos.store (pos + juce::jmax (0, todo));
    if (todo < n)
        playing.store (false);
}

//==============================================================================
bool NeptuniumProcessor::loadFile (const juce::File& f)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 0)
        return false;

    const int len = (int) juce::jmin<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 60.0));
    juce::AudioBuffer<float> b (2, len);
    b.clear();
    r->read (&b, 0, len, 0, true, true);
    if (r->numChannels == 1)
        b.copyFrom (1, 0, b, 0, 0, len);

    {
        const juce::ScopedLock sl (lock);
        source = std::move (b);
        sourceRate = r->sampleRate;
        sourceName = f.getFileNameWithoutExtension();
        info.name = sourceName;
        info.hasFile = true;
    }
    playing = false;
    ++version;
    worker.notify();
    return true;
}

void NeptuniumProcessor::setPreview (bool on)
{
    if (on) playPos = 0;
    playing = on;
}

//==============================================================================
static std::vector<float> makeIR (double sr, int M, float dens, float tone, int ch)
{
    std::vector<float> ir ((size_t) M);
    juce::Random rnd (1234 + ch * 77);
    float y = 0.0f;
    double energy = 0.0;
    const double fadeIn = sr * 0.004;
    for (int i = 0; i < M; ++i)
    {
        const double t   = (double) i / (double) M;
        const double env = std::exp (-dens * t) * juce::jmin (1.0, (double) i / fadeIn);
        const double fc  = tone * (1.0 - 0.75 * t);
        const float  a   = (float) (1.0 - std::exp (-6.2831853 * fc / sr));
        y += a * ((rnd.nextFloat() * 2.0f - 1.0f) - y);
        ir[(size_t) i] = y * (float) env;
        energy += (double) ir[(size_t) i] * ir[(size_t) i];
    }
    const float scale = (float) (1.0 / std::sqrt (energy + 1.0e-12));
    for (auto& v : ir) v *= scale;
    return ir;
}

static std::vector<float> convolveFFT (const std::vector<float>& a, const std::vector<float>& b)
{
    using C = juce::dsp::Complex<float>;
    const size_t n = a.size() + b.size() - 1;
    int order = 1;
    while ((size_t (1) << order) < n) ++order;
    const size_t size = size_t (1) << order;

    juce::dsp::FFT fft (order);
    std::vector<C> A (size), B (size), X (size);
    for (size_t i = 0; i < a.size(); ++i) A[i] = C (a[i], 0.0f);
    for (size_t i = 0; i < b.size(); ++i) B[i] = C (b[i], 0.0f);

    fft.perform (A.data(), X.data(), false);   // X = FFT(a)
    fft.perform (B.data(), A.data(), false);   // A = FFT(b)
    for (size_t i = 0; i < size; ++i) X[i] *= A[i];
    fft.perform (X.data(), B.data(), true);    // B = IFFT

    std::vector<float> out (n);
    for (size_t i = 0; i < n; ++i) out[i] = B[i].real();
    return out;
}

void NeptuniumProcessor::doRender()
{
    juce::AudioBuffer<float> src;
    double srcRate = 44100.0;
    juce::String name;
    {
        const juce::ScopedLock sl (lock);
        if (source.getNumSamples() < 16) return;
        src = source;
        srcRate = sourceRate;
        name = sourceName;
    }

    busy = true;

    const float lenSec = *apvts.getRawParameterValue ("length");
    const float dens   = *apvts.getRawParameterValue ("density");
    const float tone   = *apvts.getRawParameterValue ("tone");
    const float wetDb  = *apvts.getRawParameterValue ("wet");
    const bool  only   = *apvts.getRawParameterValue ("only") > 0.5f;
    const bool  dry    = *apvts.getRawParameterValue ("dry")  > 0.5f;

    const double host = hostRate.load();
    const double sr   = host > 0.0 ? host : srcRate;

    // 1) bring the vocal to the project's sample rate
    std::vector<std::vector<float>> x (2);
    const int inLen = src.getNumSamples();
    for (int ch = 0; ch < 2; ++ch)
    {
        if (std::abs (srcRate - sr) > 1.0)
        {
            std::vector<float> in ((size_t) inLen + 8, 0.0f);
            std::copy (src.getReadPointer (ch), src.getReadPointer (ch) + inLen, in.begin());
            const double ratio = srcRate / sr;
            const int outN = (int) std::ceil ((double) inLen / ratio);
            x[(size_t) ch].assign ((size_t) outN, 0.0f);
            juce::LagrangeInterpolator li;
            li.process (ratio, in.data(), x[(size_t) ch].data(), outN);
        }
        else
        {
            x[(size_t) ch].assign (src.getReadPointer (ch), src.getReadPointer (ch) + inLen);
        }
    }

    // 2) reverse -> reverb -> reverse: the tail becomes a swell BEFORE the line
    const int L = (int) x[0].size();
    const int M = juce::jmax (64, (int) (sr * lenSec));
    const int N = L + M - 1;
    std::vector<std::vector<float>> wet (2);
    for (int ch = 0; ch < 2; ++ch)
    {
        std::vector<float> rev (x[(size_t) ch].rbegin(), x[(size_t) ch].rend());
        wet[(size_t) ch] = convolveFFT (rev, makeIR (sr, M, dens, tone, ch));
    }

    // 3) mix
    const int offset = M - 1;
    const int fade   = juce::jmax (1, (int) (sr * 0.06));
    const float wetGain = 0.7f * juce::Decibels::decibelsToGain (wetDb);
    juce::AudioBuffer<float> out (2, N);
    float peak = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        float* o = out.getWritePointer (ch);
        const auto& w = wet[(size_t) ch];
        for (int i = 0; i < N; ++i)
        {
            float g = wetGain;
            if (only)
            {
                const int k = i - offset;
                if (k > 0)
                    g = k >= fade ? 0.0f : g * std::cos ((float) k / (float) fade * 1.5707963f);
            }
            o[i] = w[(size_t) (N - 1 - i)] * g;
        }
        if (dry)
            for (int i = 0; i < L; ++i)
                o[offset + i] += x[(size_t) ch][(size_t) i];
        for (int i = 0; i < N; ++i)
            peak = juce::jmax (peak, std::abs (o[i]));
    }
    if (peak > 0.0f)
        out.applyGain (0.89f / peak);

    // 4) write the WAV that can be dragged into the playlist
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Neptunium");
    dir.createDirectory();
    auto outFile = dir.getChildFile (name + "_neptunium.wav");
    outFile.deleteFile();
    {
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::FileOutputStream> fos (outFile.createOutputStream());
        if (fos != nullptr)
        {
            if (auto* w = wavFormat.createWriterFor (fos.get(), sr, 2u, 24, juce::StringPairArray(), 0))
            {
                fos.release();
                std::unique_ptr<juce::AudioFormatWriter> writer (w);
                writer->writeFromAudioSampleBuffer (out, 0, N);
            }
        }
    }

    // 5) waveform for the UI
    const int bins = 640;
    std::vector<float> peaks ((size_t) bins, 0.0f);
    for (int i = 0; i < N; ++i)
    {
        const size_t b = (size_t) ((int64_t) i * bins / N);
        peaks[b] = juce::jmax (peaks[b], std::abs (out.getSample (0, i)), std::abs (out.getSample (1, i)));
    }

    {
        const juce::ScopedLock sl (lock);
        info.peaks = std::move (peaks);
        info.offsetFrac = (float) offset / (float) N;
        info.offsetSeconds = (double) offset / sr;
        info.name = name;
        info.outFile = outFile;
        info.hasFile = true;
    }
    {
        const juce::ScopedLock pl (previewLock);
        previewBuf = std::move (out);
        playPos = 0;
        playing = false;
    }
    ++version;
    busy = false;
}

//==============================================================================
juce::AudioProcessorEditor* NeptuniumProcessor::createEditor() { return new NeptuniumEditor (*this); }

void NeptuniumProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void NeptuniumProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NeptuniumProcessor(); }
