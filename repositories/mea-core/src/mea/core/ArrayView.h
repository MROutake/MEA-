#pragma once

/// @file ArrayView.h
/// @brief Nicht besitzender, unveränderlicher Array-View (C++17-Ersatz für std::span).
///        Das referenzierte Array muss länger leben als der View (ADR 0001).

#include <cstddef>

namespace mea {

template <typename T>
class ArrayView {
public:
    constexpr ArrayView() noexcept : data_(nullptr), size_(0) {}

    /// Vorbedingung: @p data zeigt auf mindestens @p size gültige Elemente
    /// (data darf nur bei size == 0 nullptr sein).
    constexpr ArrayView(const T* data, const std::size_t size) noexcept
        : data_(data), size_(size) {}

    template <std::size_t N>
    explicit constexpr ArrayView(const T (&array)[N]) noexcept : data_(array), size_(N) {}

    [[nodiscard]] constexpr const T* data() const noexcept { return data_; }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

    /// Kein Bounds-Check; @p index muss < size() sein.
    [[nodiscard]] constexpr const T& operator[](const std::size_t index) const noexcept {
        return data_[index];
    }

    /// Sicherer Zugriff: nullptr bei Bereichsüberschreitung (keine Exceptions).
    [[nodiscard]] constexpr const T* at(const std::size_t index) const noexcept {
        return index < size_ ? &data_[index] : nullptr;
    }

private:
    const T* data_;
    std::size_t size_;
};

}  // namespace mea
