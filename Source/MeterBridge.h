#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace field
{
struct MeterFrame
{
    std::array<float, 2> inputDb { -100.0f, -100.0f };
    std::array<float, 2> outputDb { -100.0f, -100.0f };
    std::array<float, 2> inputRmsDb { -100.0f, -100.0f };
    std::array<float, 2> outputRmsDb { -100.0f, -100.0f };
    float reductionDb = 0.0f;
    std::uint64_t sequence = 0;
    bool bypassed = false;
};
class MeterBridge
{
public:
    // One producer (audio), one consumer (editor). Neither ever writes the
    // other's cursor. Release/acquire publishes both ownership and payload.
    bool push (const MeterFrame& frame) noexcept
    {
        latestSequence.store (frame.sequence, std::memory_order_release);
        const auto write = writeIndex.load (std::memory_order_relaxed);
        const auto next = (write + 1) % capacity;
        if (next == readIndex.load (std::memory_order_acquire))
            return false; // A closed editor must never block the audio thread.
        frames[write] = frame;
        writeIndex.store (next, std::memory_order_release);
        return true;
    }

    bool pop (MeterFrame& frame) noexcept
    {
        auto read = readIndex.load (std::memory_order_relaxed);
        const auto write = writeIndex.load (std::memory_order_acquire);
        const auto latest = latestSequence.load (std::memory_order_acquire);
        while (read != write)
        {
            const auto candidate = frames[read];
            read = (read + 1) % capacity;
            // Discard old UI backlog on the consumer side. A reopening editor
            // resumes within 100 ms of the audio clock instead of replaying
            // seconds of old peaks. Sequence gaps remain visible to the UI.
            if (latest <= candidate.sequence || latest - candidate.sequence <= 6)
            {
                frame = candidate;
                readIndex.store (read, std::memory_order_release);
                return true;
            }
        }
        readIndex.store (read, std::memory_order_release);
        return false;
    }
private:
    static_assert (std::atomic<std::size_t>::is_always_lock_free);
    static_assert (std::atomic<std::uint64_t>::is_always_lock_free);
    static constexpr std::size_t capacity = 128;
    std::array<MeterFrame, capacity> frames {};
    alignas (64) std::atomic<std::size_t> writeIndex { 0 };
    alignas (64) std::atomic<std::size_t> readIndex { 0 };
    std::atomic<std::uint64_t> latestSequence { 0 };
};
}
