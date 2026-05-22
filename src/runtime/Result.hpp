#pragma once

#include <stdexcept>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include "runtime/Error.hpp"

namespace rmg {

    template <typename T>
    class Result {
    public:
        static_assert(!std::is_void<T>::value, "Use Result<void> specialization for void results");

        [[nodiscard]] static Result success(T value) { return Result(std::move(value)); }
        [[nodiscard]] static Result failure(Error error) { return Result(std::move(error)); }

        [[nodiscard]] bool ok() const { return std::holds_alternative<T>(storage_); }
        explicit operator bool() const { return ok(); }

        template <typename F>
        [[nodiscard]] auto and_then(F &&f) & -> decltype(std::forward<F>(f)(std::declval<T &>())) {
            using NextResult = decltype(std::forward<F>(f)(std::declval<T &>()));
            if (!ok()) {
                return NextResult::failure(error());
            }
            return std::forward<F>(f)(value());
        }

        template <typename F>
        [[nodiscard]] auto and_then(F &&f) && -> decltype(std::forward<F>(f)(std::declval<T &&>())) {
            using NextResult = decltype(std::forward<F>(f)(std::declval<T &&>()));
            if (!ok()) {
                return NextResult::failure(std::move(*this).error());
            }
            return std::forward<F>(f)(std::move(*this).value());
        }

        [[nodiscard]] T &value() & {
            if (!ok()) {
                throw std::logic_error("Result does not contain a value");
            }
            return std::get<T>(storage_);
        }

        [[nodiscard]] const T &value() const & {
            if (!ok()) {
                throw std::logic_error("Result does not contain a value");
            }
            return std::get<T>(storage_);
        }

        [[nodiscard]] T &&value() && {
            if (!ok()) {
                throw std::logic_error("Result does not contain a value");
            }
            return std::move(std::get<T>(storage_));
        }

        [[nodiscard]] const Error &error() const & {
            if (ok()) {
                throw std::logic_error("Result does not contain an error");
            }
            return std::get<Error>(storage_);
        }

        [[nodiscard]] Error &&error() && {
            if (ok()) {
                throw std::logic_error("Result does not contain an error");
            }
            return std::move(std::get<Error>(storage_));
        }

    private:
        explicit Result(T value) : storage_(std::move(value)) {}
        explicit Result(Error error) : storage_(std::move(error)) {}

        std::variant<T, Error> storage_;
    };

    template <>
    class Result<void> {
    public:
        [[nodiscard]] static Result success() { return Result(); }
        [[nodiscard]] static Result failure(Error error) { return Result(std::move(error)); }

        [[nodiscard]] bool ok() const { return !error_.has_value(); }
        explicit operator bool() const { return ok(); }

        template <typename F>
        [[nodiscard]] auto and_then(F &&f) const & -> decltype(std::forward<F>(f)()) {
            using NextResult = decltype(std::forward<F>(f)());
            if (!ok()) {
                return NextResult::failure(error());
            }
            return std::forward<F>(f)();
        }

        template <typename F>
        [[nodiscard]] auto and_then(F &&f) && -> decltype(std::forward<F>(f)()) {
            using NextResult = decltype(std::forward<F>(f)());
            if (!ok()) {
                return NextResult::failure(std::move(*this).error());
            }
            return std::forward<F>(f)();
        }

        [[nodiscard]] const Error &error() const & {
            if (ok()) {
                throw std::logic_error("Result does not contain an error");
            }
            return *error_;
        }

        [[nodiscard]] Error &&error() && {
            if (ok()) {
                throw std::logic_error("Result does not contain an error");
            }
            return std::move(*error_);
        }

    private:
        Result() = default;
        explicit Result(Error error) : error_(std::move(error)) {}

        std::optional<Error> error_;
    };

} // namespace rmg
