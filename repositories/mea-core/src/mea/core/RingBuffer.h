#pragma once

/// @file RingBuffer.h
/// @brief Begrenzter Ringpuffer mit Compile-Time-Kapazität, ohne dynamische
///        Allokation (ADR 0001). T muss trivial kopierbar sein.

#include <cstddef>
#include <type_traits>

namespace mea {

template <typename T, std::size_t Capacity>
class RingBuffer {
    static_assert(Capacity > 0, "RingBuffer benötigt Kapazität > 0");
    static_assert(std::is_trivially_copyable<T>::value,
                  "RingBuffer erwartet trivial kopierbare Elemente");

public:
    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }
    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] bool empty() const noexcept { return count_ == 0; }
    [[nodiscard]] bool full() const noexcept { return count_ == Capacity; }

    /// Fügt ein Element hinten an. false, wenn der Puffer voll ist (Element verworfen).
    [[nodiscard]] bool push(const T& item) noexcept {
        if (full()) {
            return false;
        }
        items_[writeIndex_] = item;
        writeIndex_ = nextIndex(writeIndex_);
        ++count_;
        return true;
    }

    /// Entnimmt das älteste Element. false, wenn der Puffer leer ist.
    [[nodiscard]] bool pop(T& out) noexcept {
        if (empty()) {
            return false;
        }
        out = items_[readIndex_];
        readIndex_ = nextIndex(readIndex_);
        --count_;
        return true;
    }

    /// Ältestes Element ohne Entnahme; nullptr, wenn leer.
    [[nodiscard]] const T* front() const noexcept {
        return empty() ? nullptr : &items_[readIndex_];
    }

    void clear() noexcept {
        readIndex_ = 0;
        writeIndex_ = 0;
        count_ = 0;
    }

private:
    [[nodiscard]] static constexpr std::size_t nextIndex(
        const std::size_t index) noexcept {
        return (index + 1U) % Capacity;
    }

    T items_[Capacity]{};
    std::size_t readIndex_{0};
    std::size_t writeIndex_{0};
    std::size_t count_{0};
};

}  // namespace mea
