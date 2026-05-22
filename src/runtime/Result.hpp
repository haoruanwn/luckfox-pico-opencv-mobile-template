#pragma once

#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

#include "runtime/Error.hpp"

namespace rmg {

    template<typename T>
    class Result;

    namespace detail {

        template<typename T>
        struct is_result : std::false_type {};

        template<typename T>
        struct is_result<Result<T>> : std::true_type {};

        template<typename T>
        using decay_t = typename std::decay<T>::type;

        template<typename T>
        constexpr bool is_result_v = is_result<decay_t<T>>::value;

    } // namespace detail

    template<typename T>
    class Result {
    public:
        static_assert(!std::is_void<T>::value, "Use Result<void> specialization for void results");

        Result(const Result &) = default;
        Result(Result &&) noexcept(std::is_nothrow_move_constructible<std::variant<T, Error>>::value) = default;
        Result &operator=(const Result &) = default;
        Result &operator=(Result &&) noexcept(std::is_nothrow_move_assignable<std::variant<T, Error>>::value) = default;
        ~Result() = default;

        [[nodiscard]] static Result success(T value) noexcept(std::is_nothrow_move_constructible<T>::value) {
            return Result(std::move(value));
        }

        [[nodiscard]] static Result failure(Error error) noexcept(std::is_nothrow_move_constructible<Error>::value) {
            return Result(std::move(error));
        }

        [[nodiscard]] bool ok() const noexcept { return std::holds_alternative<T>(storage_); }
        explicit operator bool() const noexcept { return ok(); }

        template<typename F>
        [[nodiscard]] auto and_then(F &&f) & {
            using NextResult = typename std::invoke_result<F, T &>::type;
            static_assert(detail::is_result_v<NextResult>, "Result::and_then callback must return rmg::Result<U>");

            if (!ok()) {
                return detail::decay_t<NextResult>::failure(error());
            }
            return std::forward<F>(f)(value());
        }

        template<typename F>
        [[nodiscard]] auto and_then(F &&f) const & {
            using NextResult = typename std::invoke_result<F, const T &>::type;
            static_assert(detail::is_result_v<NextResult>, "Result::and_then callback must return rmg::Result<U>");

            if (!ok()) {
                return detail::decay_t<NextResult>::failure(error());
            }
            return std::forward<F>(f)(value());
        }

        template<typename F>
        [[nodiscard]] auto and_then(F &&f) && {
            using NextResult = typename std::invoke_result<F, T &&>::type;
            static_assert(detail::is_result_v<NextResult>, "Result::and_then callback must return rmg::Result<U>");

            if (!ok()) {
                return detail::decay_t<NextResult>::failure(std::move(*this).error());
            }
            return std::forward<F>(f)(std::move(*this).value());
        }

        template<typename F>
        [[nodiscard]] Result or_else(F &&f) & {
            using NextResult = typename std::invoke_result<F, Error &>::type;
            static_assert(std::is_same<detail::decay_t<NextResult>, Result>::value,
                          "Result::or_else callback must return the same rmg::Result<T> type");
            static_assert(
                    std::is_copy_constructible<T>::value,
                    "Result::or_else on an lvalue success requires a copyable T; use std::move(result).or_else()");

            if (ok()) {
                return Result::success(value());
            }
            return std::forward<F>(f)(error());
        }

        template<typename F>
        [[nodiscard]] Result or_else(F &&f) const & {
            using NextResult = typename std::invoke_result<F, const Error &>::type;
            static_assert(std::is_same<detail::decay_t<NextResult>, Result>::value,
                          "Result::or_else callback must return the same rmg::Result<T> type");
            static_assert(
                    std::is_copy_constructible<T>::value,
                    "Result::or_else on an lvalue success requires a copyable T; use std::move(result).or_else()");

            if (ok()) {
                return Result::success(value());
            }
            return std::forward<F>(f)(error());
        }

        template<typename F>
        [[nodiscard]] Result or_else(F &&f) && {
            using NextResult = typename std::invoke_result<F, Error &&>::type;
            static_assert(std::is_same<detail::decay_t<NextResult>, Result>::value,
                          "Result::or_else callback must return the same rmg::Result<T> type");

            if (ok()) {
                return Result::success(std::move(*this).value());
            }
            return std::forward<F>(f)(std::move(*this).error());
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

        [[nodiscard]] Error &error() & {
            if (ok()) {
                throw std::logic_error("Result does not contain an error");
            }
            return std::get<Error>(storage_);
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

        [[nodiscard]] Error error_or(Error fallback) const & {
            if (ok()) {
                return fallback;
            }
            return error();
        }

        [[nodiscard]] Error error_or(Error fallback) && {
            if (ok()) {
                return fallback;
            }
            return std::move(*this).error();
        }

    private:
        explicit Result(T value) noexcept(std::is_nothrow_move_constructible<T>::value) : storage_(std::move(value)) {}
        explicit Result(Error error) noexcept(std::is_nothrow_move_constructible<Error>::value) :
            storage_(std::move(error)) {}

        std::variant<T, Error> storage_;
    };

    template<>
    class Result<void> {
    public:
        Result(const Result &) = default;
        Result(Result &&) noexcept(std::is_nothrow_move_constructible<std::optional<Error>>::value) = default;
        Result &operator=(const Result &) = default;
        Result &operator=(Result &&) noexcept(std::is_nothrow_move_assignable<std::optional<Error>>::value) = default;
        ~Result() = default;

        [[nodiscard]] static Result success() noexcept { return Result(); }

        [[nodiscard]] static Result failure(Error error) noexcept(std::is_nothrow_move_constructible<Error>::value) {
            return Result(std::move(error));
        }

        [[nodiscard]] bool ok() const noexcept { return !error_.has_value(); }
        explicit operator bool() const noexcept { return ok(); }

        template<typename F>
        [[nodiscard]] auto and_then(F &&f) & {
            using NextResult = typename std::invoke_result<F>::type;
            static_assert(detail::is_result_v<NextResult>, "Result::and_then callback must return rmg::Result<U>");

            if (!ok()) {
                return detail::decay_t<NextResult>::failure(error());
            }
            return std::forward<F>(f)();
        }

        template<typename F>
        [[nodiscard]] auto and_then(F &&f) const & {
            using NextResult = typename std::invoke_result<F>::type;
            static_assert(detail::is_result_v<NextResult>, "Result::and_then callback must return rmg::Result<U>");

            if (!ok()) {
                return detail::decay_t<NextResult>::failure(error());
            }
            return std::forward<F>(f)();
        }

        template<typename F>
        [[nodiscard]] auto and_then(F &&f) && {
            using NextResult = typename std::invoke_result<F>::type;
            static_assert(detail::is_result_v<NextResult>, "Result::and_then callback must return rmg::Result<U>");

            if (!ok()) {
                return detail::decay_t<NextResult>::failure(std::move(*this).error());
            }
            return std::forward<F>(f)();
        }

        template<typename F>
        [[nodiscard]] Result or_else(F &&f) & {
            using NextResult = typename std::invoke_result<F, Error &>::type;
            static_assert(std::is_same<detail::decay_t<NextResult>, Result>::value,
                          "Result::or_else callback must return rmg::Result<void>");

            if (ok()) {
                return Result::success();
            }
            return std::forward<F>(f)(error());
        }

        template<typename F>
        [[nodiscard]] Result or_else(F &&f) const & {
            using NextResult = typename std::invoke_result<F, const Error &>::type;
            static_assert(std::is_same<detail::decay_t<NextResult>, Result>::value,
                          "Result::or_else callback must return rmg::Result<void>");

            if (ok()) {
                return Result::success();
            }
            return std::forward<F>(f)(error());
        }

        template<typename F>
        [[nodiscard]] Result or_else(F &&f) && {
            using NextResult = typename std::invoke_result<F, Error &&>::type;
            static_assert(std::is_same<detail::decay_t<NextResult>, Result>::value,
                          "Result::or_else callback must return rmg::Result<void>");

            if (ok()) {
                return Result::success();
            }
            return std::forward<F>(f)(std::move(*this).error());
        }

        [[nodiscard]] Error &error() & {
            if (ok()) {
                throw std::logic_error("Result does not contain an error");
            }
            return *error_;
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

        [[nodiscard]] Error error_or(Error fallback) const & {
            if (ok()) {
                return fallback;
            }
            return error();
        }

        [[nodiscard]] Error error_or(Error fallback) && {
            if (ok()) {
                return fallback;
            }
            return std::move(*this).error();
        }

    private:
        Result() = default;
        explicit Result(Error error) noexcept(std::is_nothrow_move_constructible<Error>::value) :
            error_(std::move(error)) {}

        std::optional<Error> error_;
    };

} // namespace rmg
