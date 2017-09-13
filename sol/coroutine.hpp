// The MIT License (MIT)

// Copyright (c) 2013-2017 Rapptz, ThePhD and contributors

// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SOL_COROUTINE_HPP
#define SOL_COROUTINE_HPP

#include "reference.hpp"
#include "stack.hpp"
#include "function_result.hpp"
#include "thread.hpp"

namespace sol {
	class coroutine : public reference {
	private:
		call_status stats = call_status::yielded;

		void luacall(std::ptrdiff_t argcount, std::ptrdiff_t) {
			stats = static_cast<call_status>(lua_resume(lua_state(), nullptr, static_cast<int>(argcount)));
		}

		template <std::size_t... I, typename... Ret>
		auto invoke(types<Ret...>, std::index_sequence<I...>, std::ptrdiff_t n) {
			luacall(n, sizeof...(Ret));
			return stack::pop<std::tuple<Ret...>>(lua_state());
		}

		template <std::size_t I, typename Ret>
		Ret invoke(types<Ret>, std::index_sequence<I>, std::ptrdiff_t n) {
			luacall(n, 1);
			return stack::pop<Ret>(lua_state());
		}

		template <std::size_t I>
		void invoke(types<void>, std::index_sequence<I>, std::ptrdiff_t n) {
			luacall(n, 0);
		}

		protected_function_result invoke(types<>, std::index_sequence<>, std::ptrdiff_t n) {
			int stacksize = lua_gettop(lua_state());
			int firstreturn = (std::max)(1, stacksize - static_cast<int>(n));
			luacall(n, LUA_MULTRET);
			int poststacksize = lua_gettop(lua_state());
			int returncount = poststacksize - (firstreturn - 1);
			if (error()) {
				return protected_function_result(lua_state(), lua_absindex(lua_state(), -1), 1, returncount, status());
			}
			return protected_function_result(lua_state(), firstreturn, returncount, returncount, status());
		}

	public:
		coroutine() noexcept = default;
		coroutine(const coroutine&) noexcept = default;
		coroutine(coroutine&&) noexcept = default;
		coroutine& operator=(const coroutine&) noexcept = default;
		coroutine& operator=(coroutine&&) noexcept = default;
		template <typename T, meta::enable<meta::neg<std::is_same<meta::unqualified_t<T>, coroutine>>, std::is_base_of<reference, meta::unqualified_t<T>>> = meta::enabler>
		coroutine(T&& r)
		: reference(std::forward<T>(r)) {
		}
		coroutine(lua_nil_t r)
		: reference(r) {
		}
		coroutine(const stack_reference& r) noexcept
		: coroutine(r.lua_state(), r.stack_index()) {
		}
		coroutine(stack_reference&& r) noexcept
		: coroutine(r.lua_state(), r.stack_index()) {
		}
		template <typename T, meta::enable<meta::neg<std::is_integral<meta::unqualified_t<T>>>, meta::neg<std::is_same<T, ref_index>>> = meta::enabler>
		coroutine(lua_State* L, T&& r)
		: coroutine(L, sol::ref_index(r.registry_index())) {
		}
		coroutine(lua_State* L, int index = -1)
		: reference(L, index) {
#ifdef SOL_CHECK_ARGUMENTS
			constructor_handler handler{};
			stack::check<coroutine>(L, index, handler);
#endif // Safety
		}
		coroutine(lua_State* L, ref_index index)
		: reference(L, index) {
#ifdef SOL_CHECK_ARGUMENTS
			auto pp = stack::push_pop(*this);
			constructor_handler handler{};
			stack::check<coroutine>(L, -1, handler);
#endif // Safety
		}

		call_status status() const noexcept {
			return stats;
		}

		bool error() const noexcept {
			call_status cs = status();
			return cs != call_status::ok && cs != call_status::yielded;
		}

		bool runnable() const noexcept {
			return valid()
				&& (status() == call_status::yielded);
		}

		explicit operator bool() const noexcept {
			return runnable();
		}

		template <typename... Args>
		protected_function_result operator()(Args&&... args) {
			return call<>(std::forward<Args>(args)...);
		}

		template <typename... Ret, typename... Args>
		decltype(auto) operator()(types<Ret...>, Args&&... args) {
			return call<Ret...>(std::forward<Args>(args)...);
		}

		template <typename... Ret, typename... Args>
		decltype(auto) call(Args&&... args) {
			push();
			int pushcount = stack::multi_push(lua_state(), std::forward<Args>(args)...);
			return invoke(types<Ret...>(), std::make_index_sequence<sizeof...(Ret)>(), pushcount);
		}
	};
} // namespace sol

#endif // SOL_COUROUTINE_HPP
