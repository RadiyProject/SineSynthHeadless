#include <cstdint>
#include <vector>
#include <memory>
#include <mutex>
#include <cstring>
#include <algorithm>
#include <string>
#include "Env.h"
#include "Log.h"

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ipluginbase.h"               // IPluginFactory(2/3), PClassInfo(2)
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstunits.h"

#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/hosting/eventlist.h"

#include "public.sdk/source/common/memorystream.h"

#include "api.h"

using namespace Steinberg;
using namespace Steinberg::Vst;
using VST3::Hosting::Module;

struct VstHandle {
    std::mutex m;

    Module::Ptr module;                 // shared_ptr from hosting/module.h
    IPluginFactory* factory = nullptr;

    IPtr<IComponent>      component;
    IPtr<IAudioProcessor> processor;
    IPtr<IEditController> controller;   // optional

    int sr = 48000;
    int block = 512;
    int channels = 2;

    int processMode = 0; // 0 = realtime, 1 = offline

    ProcessSetup setup{};

    // Incoming parameter/event queues (host-side implementations from hosting/*).
    std::unique_ptr<ParameterChanges> inParams{ new ParameterChanges() };
    std::unique_ptr<EventList>        inEvents{ new EventList() };

    std::vector<float> chL, chR;

    // Optional development latency
    uint32_t latency = 0;

    Steinberg::Vst::ProcessContext ctx{};
};

// Utility: find the first class in the AudioEffect category.
static bool findFirstAudioEffect(IPluginFactory* f, TUID outCid)
{
    if (!f) return false;
    // Try IPluginFactory3 -> IPluginFactory2 -> IPluginFactory.
    if (auto f3 = FUnknownPtr<IPluginFactory3>(f)) {
        int32 count = f3->countClasses();
        PClassInfo2 ci2{};
        for (int32 i = 0; i < count; ++i) {
            if (f3->getClassInfo2(i, &ci2) == kResultOk) {
                if (std::strcmp(ci2.category, kVstAudioEffectClass) == 0) {
                    std::memcpy(outCid, ci2.cid, sizeof(TUID));
                    return true;
                }
            }
        }
    } else if (auto f2 = FUnknownPtr<IPluginFactory2>(f)) {
        int32 count = f2->countClasses();
        PClassInfo2 ci2{};
        for (int32 i = 0; i < count; ++i) {
            if (f2->getClassInfo2(i, &ci2) == kResultOk) {
                if (std::strcmp(ci2.category, kVstAudioEffectClass) == 0) {
                    std::memcpy(outCid, ci2.cid, sizeof(TUID));
                    return true;
                }
            }
        }
    } else {
        int32 count = f->countClasses();
        PClassInfo ci{};
        for (int32 i = 0; i < count; ++i) {
            if (f->getClassInfo(i, &ci) == kResultOk) {
                if (std::strcmp(ci.category, kVstAudioEffectClass) == 0) {
                    std::memcpy(outCid, ci.cid, sizeof(TUID));
                    return true;
                }
            }
        }
    }
    return false;
}

// create / destroy
API VstHandle* VstCreate(const char* pluginPath, double sampleRate, int blockSize, int channels) {
    radiyx::Log::Instance().Push("Create started");
    if (!pluginPath) return nullptr;

    auto* h = new VstHandle();
    h->sr = (int)sampleRate;
    h->block = blockSize > 0 ? blockSize : 512;
    h->channels = std::max(1, channels);
    
    h->ctx = {};                     // zero-init
    h->ctx.sampleRate = sampleRate;  // Sufficient for this plug-in.
    // h->ctx.state = 0;             // optional

    std::string err;
    std::string path = pluginPath ? pluginPath : "";
    h->module = VST3::Hosting::Module::create(path, err);
    if (!h->module) {
        // Optionally log err.
        delete h;
        return nullptr;
    }

    const auto& pf = h->module->getFactory();
    h->factory = pf.get(); 
    if (!h->factory) { delete h; return nullptr; }

    // Find the audio effect class ID.
    TUID cid{};
    if (!findFirstAudioEffect(h->factory, cid)) { delete h; return nullptr; }

    // Create IComponent.
    IComponent* rawComp = nullptr;
    if (h->factory->createInstance(cid, IComponent::iid, (void**)&rawComp) != kResultOk || !rawComp) {
        delete h; return nullptr;
    }
    h->component = rawComp; // IPtr takes ownership.

    // IAudioProcessor
    IAudioProcessor* rawProc = nullptr;
    if (h->component->queryInterface(IAudioProcessor::iid, (void**)&rawProc) != kResultOk || !rawProc) {
        delete h; return nullptr;
    }
    h->processor = rawProc;

    // IEditController (optional; may be absent).
    IEditController* rawCtl = nullptr;
    if (h->component->queryInterface(IEditController::iid, (void**)&rawCtl) == kResultOk && rawCtl)
        h->controller = rawCtl;

    // initialize + setup
    if (h->component->initialize(nullptr) != kResultOk) { delete h; return nullptr; }

    h->processMode = 0;
    h->setup.processMode = kRealtime;
    h->setup.symbolicSampleSize = kSample32;
    h->setup.maxSamplesPerBlock = h->block;
    h->setup.sampleRate = sampleRate;

    if (h->processor->setupProcessing(h->setup) != kResultOk) { delete h; return nullptr; }

    // Activate the component and main audio bus.
    h->component->setActive(true);

    // Configure the output layout (no inputs, stereo output).
    SpeakerArrangement outArr = SpeakerArr::kStereo;
    h->processor->setBusArrangements(nullptr, 0, &outArr, 1);

    h->component->activateBus(kAudio, kOutput, 0, true);
    h->processor->setProcessing(true);

    h->chL.assign(h->block, 0.f);
    h->chR.assign(h->block, 0.f);
    radiyx::Log::Instance().Push("Create finished");

    return h;
}

API void VstDestroy(VstHandle* h) {
    if (!h) return;
    std::lock_guard<std::mutex> lk(h->m);

    if (h->processor) h->processor->setProcessing(false);
    if (h->component) {
        h->component->setActive(false);
        h->component->terminate();
    }
    delete h;
}

// Parameters (normalized 0..1).
API void VstSetParam(VstHandle* h, uint32_t id, float norm) {
    if (!h) return;
    std::lock_guard<std::mutex> lk(h->m);

    // SDK signature: addParameterData(pid, index_by_ref).
    int32 idx = 0;
    IParamValueQueue* q = h->inParams->addParameterData((ParamID)id, idx);
    if (q) {
        int32 pt = 0; // An lvalue is required.
        q->addPoint(0 /*sampleOffset*/, (ParamValue)norm, pt);
    }
}

// Notes
API void VstNoteOn (VstHandle* h, int note, float vel) {
    if (!h) return;
    std::lock_guard<std::mutex> lk(h->m);
    Event e{}; e.type = Event::kNoteOnEvent;
    e.sampleOffset = 0; e.noteOn.pitch = (int8)note; e.noteOn.velocity = vel;
    h->inEvents->addEvent(e);
}
API void VstNoteOff(VstHandle* h, int note) {
    if (!h) return;
    std::lock_guard<std::mutex> lk(h->m);
    Event e{}; e.type = Event::kNoteOffEvent;
    e.sampleOffset = 0; e.noteOff.pitch = (int8)note; e.noteOff.velocity = 0.f;
    h->inEvents->addEvent(e);
}

// Optional server-side development latency.
API void VstSetLatency(VstHandle* h, uint32_t samples) {
    if (!h) return;
    std::lock_guard<std::mutex> lk(h->m);
    h->latency = samples;
}

// Processing: write interleaved float32 (LRLR...) to out and return the frame count.
API int VstProcess(VstHandle* h, float* outInterleaved, int frames) {
    if (!h || !outInterleaved) return 0;
    std::lock_guard<std::mutex> lk(h->m);

    const int todo = std::min(frames, h->block);

    if ((int)h->chL.size() < todo) h->chL.resize(todo, 0.f);
    std::fill(h->chL.begin(), h->chL.begin() + todo, 0.f);
    if ((int)h->chR.size() < todo) h->chR.resize(todo, 0.f);
    std::fill(h->chR.begin(), h->chR.begin() + todo, 0.f);

    // Prepare non-interleaved output buffers.
    Sample32* outs[2] = { h->chL.data(), h->chR.data() };
    AudioBusBuffers outBuf{};
    outBuf.numChannels = 2;
    outBuf.channelBuffers32 = outs;

    // Processing request (declare only once).
    Steinberg::Vst::ProcessData pd{};
    pd.numSamples = todo;
    pd.numOutputs = 1;
    pd.outputs = &outBuf;
    pd.inputParameterChanges = h->inParams.get();
    pd.inputEvents = h->inEvents.get();

    // Important: the plug-in reads sampleRate from the context.
    h->ctx.sampleRate = h->setup.sampleRate;   // Keep the sample rate synchronized.
    // h->ctx.state = 0; // Flags are unnecessary when only sampleRate is used.
    pd.processContext = &h->ctx;

    // Invoke the plug-in.
    h->processor->process(pd);

    // Clear one-shot queues.
#if 1
    h->inParams->clearQueue();
#else
    h->inParams->clear();
#endif
    h->inEvents->clear();

    // Interleave -> out
    for (int i = 0; i < todo; ++i) {
        outInterleaved[2*i + 0] = h->chL[i];
        outInterleaved[2*i + 1] = h->chR[i];
    }
    return todo;
}

API bool VstReconfigure(VstHandle* h, double sampleRate, int blockSize, int channels, int processMode) {
    if (!h) return false;
    std::lock_guard<std::mutex> lk(h->m);

    // 0) Stop processing.
    if (h->processor) h->processor->setProcessing(false);
    if (h->component) h->component->setActive(false);

    // 1) Update host parameters.
    h->sr       = (int)sampleRate;
    h->block    = blockSize  > 0 ? blockSize  : h->block;
    h->channels = channels   > 0 ? channels   : h->channels;

    // 2) Processing mode (0=realtime, 1=offline).
    h->processMode = processMode == 1 ? 1 : 0;
    h->setup.processMode = h->processMode == 1 ? kOffline : kRealtime;
    h->setup.sampleRate = sampleRate;
    h->setup.maxSamplesPerBlock = h->block;
    h->setup.symbolicSampleSize = kSample32;

    // 3) Configure mono or stereo output.
    SpeakerArrangement outArr = (h->channels == 1) ? SpeakerArr::kMono : SpeakerArr::kStereo;
    h->processor->setBusArrangements(nullptr, 0, &outArr, 1);

    // 4) Apply the new setup.
    if (h->processor->setupProcessing(h->setup) != kResultOk)
        return false;

    // 5) Update the context and buffers.
    h->ctx.sampleRate = sampleRate;
    h->chL.assign(h->block, 0.f);
    h->chR.assign(h->block, 0.f);

    // 6) Reactivate and resume processing.
    h->component->activateBus(kAudio, kOutput, 0, true);
    h->component->setActive(true);
    h->processor->setProcessing(true);

    return true;
}

API bool VstGetState(VstHandle* h, void* buffer, uint32_t* size) {
    if (!h || !size) return false;
    std::lock_guard<std::mutex> lk(h->m);

    // 1) Retrieve component state.
    Steinberg::MemoryStream compMs;
    uint32_t compSize = 0;
    if (h->component) {
        if (h->component->getState(&compMs) != kResultOk)
            return false;
        compSize = static_cast<uint32_t>(compMs.getSize());
    }

    // 2) Retrieve controller state, if available.
    Steinberg::MemoryStream ctlMs;
    uint32_t ctlSize = 0;
    if (h->controller) {
        if (h->controller->getState(&ctlMs) != kResultOk)
            return false;
        ctlSize = static_cast<uint32_t>(ctlMs.getSize());
    }

    // 3) Total size: 4 + comp + 4 + ctl.
    const uint32_t total = 4u + compSize + 4u + ctlSize;

    if (!buffer) {
        // Only report the required size.
        *size = total;
        return true;
    }

    if (*size < total) {
        // The buffer is too small; report the required size.
        *size = total;
        return false;
    }

    uint8_t* out = static_cast<uint8_t*>(buffer);

    auto writeU32 = [&](uint32_t v) {
        out[0] = (uint8_t)(v & 0xFF);
        out[1] = (uint8_t)((v >> 8) & 0xFF);
        out[2] = (uint8_t)((v >> 16) & 0xFF);
        out[3] = (uint8_t)((v >> 24) & 0xFF);
        out += 4;
    };

    // 4) Write component state.
    writeU32(compSize);
    if (compSize > 0) {
        auto* data = reinterpret_cast<const uint8_t*>(compMs.getData());
        std::memcpy(out, data, compSize);
        out += compSize;
    }

    // 5) Write controller state.
    writeU32(ctlSize);
    if (ctlSize > 0) {
        auto* data = reinterpret_cast<const uint8_t*>(ctlMs.getData());
        std::memcpy(out, data, ctlSize);
        out += ctlSize;
    }

    *size = total;
    return true;
}

API bool VstSetState(VstHandle* h, const void* buffer, uint32_t size) {
    if (!h)
        return false;

    std::lock_guard<std::mutex> lk(h->m);

    // 1. Always clear host-side runtime data before restoring state.
    if (h->inParams) {
#if 1
        h->inParams->clearQueue();
#else
        h->inParams->clear();
#endif
    } else {
        h->inParams.reset(new ParameterChanges());
    }

    if (h->inEvents) {
        h->inEvents->clear();
    } else {
        h->inEvents.reset(new EventList());
    }

    std::fill(h->chL.begin(), h->chL.end(), 0.f);
    std::fill(h->chR.begin(), h->chR.end(), 0.f);

    h->ctx = {};
    h->ctx.sampleRate = h->setup.sampleRate;

    // 2. Empty state only clears runtime data.
    // This matters for a new pluginId with no saved state.
    if (!buffer || size == 0) {
        Steinberg::MemoryStream emptyState;

        if (h->component) {
            return h->component->setState(&emptyState) == kResultOk;
        }

        return true;
    }

    // 3. Nonempty state must contain at least:
    // uint32 componentSize + uint32 controllerSize.
    if (size < 8) {
        return false;
    }

    const uint8_t* in = static_cast<const uint8_t*>(buffer);

    auto readU32 = [&]() -> uint32_t {
        uint32_t v = (uint32_t)in[0] |
                     ((uint32_t)in[1] << 8) |
                     ((uint32_t)in[2] << 16) |
                     ((uint32_t)in[3] << 24);
        in += 4;
        return v;
    };

    // 4. Component state.
    uint32_t compSize = readU32();

    if (4u + compSize + 4u > size) {
        return false;
    }

    if (compSize > 0 && h->component) {
        Steinberg::MemoryStream compMs(
            const_cast<uint8_t*>(in),
            static_cast<int32>(compSize)
        );

        if (h->component->setState(&compMs) != kResultOk) {
            return false;
        }
    }

    in += compSize;

    // 5. Controller state.
    uint32_t ctlSize = readU32();

    const uint32_t used = 4u + compSize + 4u + ctlSize;
    if (used > size) {
        return false;
    }

    if (ctlSize > 0 && h->controller) {
        Steinberg::MemoryStream ctlMs(
            const_cast<uint8_t*>(in),
            static_cast<int32>(ctlSize)
        );

        if (h->controller->setState(&ctlMs) != kResultOk) {
            return false;
        }
    }

    return true;
}

API int VstGetProcessMode(VstHandle* h) {
    if (!h)
        return -1;

    std::lock_guard<std::mutex> lk(h->m);

    return h->processMode;
}
