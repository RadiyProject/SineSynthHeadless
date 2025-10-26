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

#include "api.h"

using namespace Steinberg;
using namespace Steinberg::Vst;
using VST3::Hosting::Module;

struct VstHandle {
    std::mutex m;

    Module::Ptr module;                 // shared_ptr из hosting/module.h
    IPluginFactory* factory = nullptr;

    IPtr<IComponent>      component;
    IPtr<IAudioProcessor> processor;
    IPtr<IEditController> controller;   // опционально

    int sr = 48000;
    int block = 512;
    int channels = 2;

    ProcessSetup setup{};

    // Очереди входящих параметров/событий (host-side имплементации из hosting/*)
    std::unique_ptr<ParameterChanges> inParams{ new ParameterChanges() };
    std::unique_ptr<EventList>        inEvents{ new EventList() };

    std::vector<float> chL, chR;

    // Dev-латентность по желанию
    uint32_t latency = 0;

    Steinberg::Vst::ProcessContext ctx{};
};

// Утилита: найти первый класс категории AudioEffect
static bool findFirstAudioEffect(IPluginFactory* f, TUID outCid)
{
    if (!f) return false;
    // Пробуем IPluginFactory3 -> IPluginFactory2 -> IPluginFactory
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
    h->ctx.sampleRate = sampleRate;  // <— достаточно для твоего плагина
    // h->ctx.state = 0;             // опционально

    std::string err;
    std::string path = pluginPath ? pluginPath : "";
    h->module = VST3::Hosting::Module::create(path, err);
    if (!h->module) {
        // можно залогировать err при желании
        delete h;
        return nullptr;
    }

    const auto& pf = h->module->getFactory();
    h->factory = pf.get(); 
    if (!h->factory) { delete h; return nullptr; }

    // находим cid аудио-эффекта
    TUID cid{};
    if (!findFirstAudioEffect(h->factory, cid)) { delete h; return nullptr; }

    // создаём IComponent
    IComponent* rawComp = nullptr;
    if (h->factory->createInstance(cid, IComponent::iid, (void**)&rawComp) != kResultOk || !rawComp) {
        delete h; return nullptr;
    }
    h->component = rawComp; // IPtr возьмёт владение

    // IAudioProcessor
    IAudioProcessor* rawProc = nullptr;
    if (h->component->queryInterface(IAudioProcessor::iid, (void**)&rawProc) != kResultOk || !rawProc) {
        delete h; return nullptr;
    }
    h->processor = rawProc;

    // IEditController (опционально; может не быть)
    IEditController* rawCtl = nullptr;
    if (h->component->queryInterface(IEditController::iid, (void**)&rawCtl) == kResultOk && rawCtl)
        h->controller = rawCtl;

    // initialize + setup
    if (h->component->initialize(nullptr) != kResultOk) { delete h; return nullptr; }

    h->setup.processMode = kRealtime;
    h->setup.symbolicSampleSize = kSample32;
    h->setup.maxSamplesPerBlock = h->block;
    h->setup.sampleRate = sampleRate;

    if (h->processor->setupProcessing(h->setup) != kResultOk) { delete h; return nullptr; }

    // Активируем компонент и основной аудиобас
    h->component->setActive(true);

    // Настраиваем аут-раскладку (без инпутов, стерео аут)
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

// параметры (normalized 0..1)
API void VstSetParam(VstHandle* h, uint32_t id, float norm) {
    if (!h) return;
    std::lock_guard<std::mutex> lk(h->m);

    // В твоём SDK: addParameterData(pid, index_by_ref)
    int32 idx = 0;
    IParamValueQueue* q = h->inParams->addParameterData((ParamID)id, idx);
    if (q) {
        int32 pt = 0; // lvalue обязателен
        q->addPoint(0 /*sampleOffset*/, (ParamValue)norm, pt);
    }
}

// ноты
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

// опционально — dev-латентность на сервере
API void VstSetLatency(VstHandle* h, uint32_t samples) {
    if (!h) return;
    std::lock_guard<std::mutex> lk(h->m);
    h->latency = samples;
}

// процессинг: пишет interleaved float32 (LRLR...) в out, возвращает frames
API int VstProcess(VstHandle* h, float* outInterleaved, int frames) {
    if (!h || !outInterleaved) return 0;
    std::lock_guard<std::mutex> lk(h->m);

    const int todo = std::min(frames, h->block);

    // Подготовим выходные буферы (non-interleaved)
    Sample32* outs[2] = { h->chL.data(), h->chR.data() };
    AudioBusBuffers outBuf{};
    outBuf.numChannels = 2;
    outBuf.channelBuffers32 = outs;

    // Процессинг-запрос (ОДНА декларация!)
    Steinberg::Vst::ProcessData pd{};
    pd.numSamples = todo;
    pd.numOutputs = 1;
    pd.outputs = &outBuf;
    pd.inputParameterChanges = h->inParams.get();
    pd.inputEvents = h->inEvents.get();

    // ВАЖНО: контекст — твой плагин читает sampleRate из него
    h->ctx.sampleRate = h->setup.sampleRate;   // на всякий случай синхронизируем
    // h->ctx.state = 0; // флаги не нужны, если ты используешь только sampleRate
    pd.processContext = &h->ctx;

    // Вызов плагина
    h->processor->process(pd);

    // Очистка одноразовых очередей
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
