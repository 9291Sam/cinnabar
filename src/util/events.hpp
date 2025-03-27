#pragma once

#include "util.hpp"
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/unordered/concurrent_flat_map_fwd.hpp>
#include <chrono>
#include <concepts>
#include <memory>
#include <optional>
#include <queue>
#include <string_view>
#include <type_traits>

namespace util
{
    /// The event system defines two different types of events

    /// Channel Events are one or many to one event
    /// These use the move constructor and a queue meaning that all events are
    /// received in the same order that they are sent

    template<class T>
    concept ChannelEventCapable = std::is_nothrow_default_constructible_v<T> && std::is_nothrow_move_constructible_v<T>
                               && std::is_nothrow_move_assignable_v<T>;

    class EventHandler
    {
    public:
        explicit EventHandler();
        ~EventHandler();

        EventHandler(const EventHandler&)             = delete;
        EventHandler(EventHandler&&)                  = delete;
        EventHandler& operator= (const EventHandler&) = delete;
        EventHandler& operator= (EventHandler&&)      = delete;

        template<ChannelEventCapable T>
        void send(std::string_view string, T t) const
        {
            this->accessInternalStorage<T>(
                string,
                [&](InternalStorage<T>* storage)
                {
                    storage->storage.push(std::move(t));
                });
        }
        template<ChannelEventCapable T>
        std::optional<T> receive(std::string_view string) const
        {
            std::optional<T> output {};

            this->accessInternalStorage<T>(
                string,
                [&](InternalStorage<T>* storage)
                {
                    if (!storage->storage.empty())
                    {
                        output = std::move(storage->storage.back());

                        storage->storage.pop();
                    }
                });

            return output;
        }

        void shrinkInternalGarbage() const;

    private:
        template<ChannelEventCapable T>
        struct InternalStorage
        {
            std::chrono::time_point<std::chrono::steady_clock> last_time_shrunk;
            std::queue<T>                                      storage;
        };

        template<ChannelEventCapable T, class Fn>
        void accessInternalStorage(std::string_view stringView, Fn&& fn) const
            requires std::invocable<Fn, InternalStorage<T>*>
        {
            const u64 typeId   = util::getIdOfType<T>();
            const u64 stringId = util::hashStringViewConstexpr(stringView);
            const u64 id       = util::hashCombine(typeId, stringId);

            const bool visited = static_cast<bool>(this->type_erased_storage.visit(
                id,
                [&](std::pair<const u64, std::shared_ptr<void>>& erased)
                {
                    std::invoke(std::forward<Fn>(fn), static_cast<InternalStorage<T>*>(erased.second.get()));
                }));

            if (!visited)
            {
                std::shared_ptr<void> newStorage {
                    new InternalStorage<T> {.last_time_shrunk {std::chrono::steady_clock::now()}, .storage {}}};

                std::invoke(std::forward<Fn>(fn), static_cast<InternalStorage<T>*>(newStorage.get()));

                this->type_erased_storage.insert(std::pair<u64, std::shared_ptr<void>> {id, std::move(newStorage)});
            }
        }

        mutable boost::concurrent_flat_map<u64, std::shared_ptr<void>> type_erased_storage;
    };

    class GlobalEventContext
    {
    public:
        GlobalEventContext();
        ~GlobalEventContext();

        GlobalEventContext(const GlobalEventContext&)             = delete;
        GlobalEventContext(GlobalEventContext&&)                  = delete;
        GlobalEventContext& operator= (const GlobalEventContext&) = delete;
        GlobalEventContext& operator= (GlobalEventContext&&)      = delete;
    };

    const EventHandler* getGlobalEventHandler();

    template<ChannelEventCapable T>
    void send(std::string_view string, T&& t)
    {
        getGlobalEventHandler()->send<T>(string, std::forward<T>(t));
    }

    template<ChannelEventCapable T>
    std::optional<T> receive(std::string_view string)
    {
        return getGlobalEventHandler()->receive<T>(string);
    }
} // namespace util