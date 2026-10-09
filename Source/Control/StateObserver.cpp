#include "Control/StateObserver.h"
#include "Audio/AudioEngine.h"

namespace dsd::control
{
    StateObserver::StateObserver(AudioEngine& engine, int timerHz)
        : engine_(engine), timerHz_(std::max(1, timerHz))
    {
        lastSnapshot_ = captureSnapshot(engine_);
    }

    StateObserver::~StateObserver()
    {
        stopTimer();
    }

    void StateObserver::start()
    {
        startTimerHz(timerHz_);
    }

    void StateObserver::stop()
    {
        stopTimer();
    }

    void StateObserver::setTimerHz(int hz)
    {
        timerHz_ = std::max(1, hz);
        if (isTimerRunning())
            startTimerHz(timerHz_);
    }

    void StateObserver::addListener(Listener* l)
    {
        if (l != nullptr && std::find(listeners_.begin(), listeners_.end(), l) == listeners_.end())
            listeners_.push_back(l);
    }

    void StateObserver::removeListener(Listener* l)
    {
        listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), l), listeners_.end());
    }

    std::vector<juce::String> StateObserver::pollState()
    {
        auto current = captureSnapshot(engine_);
        auto events  = diffSnapshots(lastSnapshot_, current);

        if (!events.empty())
        {
            lastSnapshot_ = current;
            for (auto* l : listeners_)
            {
                if (l != nullptr)
                    l->onStateEvents(events);
            }
        }

        return events;
    }

    juce::String StateObserver::pollMeters()
    {
        auto metersJson = makeMetersEventJson(engine_);
        for (auto* l : listeners_)
        {
            if (l != nullptr)
                l->onMeterEvent(metersJson);
        }
        return metersJson;
    }

    juce::String StateObserver::getFullSyncJson() const
    {
        return makeSyncJson(lastSnapshot_);
    }

    juce::var StateObserver::getFullSyncVar() const
    {
        return makeSyncVar(lastSnapshot_);
    }

    void StateObserver::timerCallback()
    {
        pollState();

        if (meteringEnabled_)
        {
            const auto now = juce::Time::getMillisecondCounter();
            if (now - lastMeterTimeMs_ >= static_cast<juce::uint32>(meterIntervalMs_))
            {
                lastMeterTimeMs_ = now;
                pollMeters();
            }
        }
    }

} // namespace dsd::control
