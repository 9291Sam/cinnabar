#include "events.hpp"

namespace util
{
    EventHandler::EventHandler()  = default;
    EventHandler::~EventHandler() = default;

    namespace
    {
        const EventHandler* globalEventHandler = nullptr;
    } // namespace

    GlobalEventContext::GlobalEventContext()
    {
        globalEventHandler = new EventHandler {};
    }

    GlobalEventContext::~GlobalEventContext()
    {
        delete globalEventHandler;
    }

    const EventHandler* getGlobalEventHandler()
    {
        return globalEventHandler;
    }
} // namespace util