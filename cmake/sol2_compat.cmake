file(READ "${CMAKE_CURRENT_LIST_DIR}/../external/sol2/include/sol/optional_implementation.hpp" sol2_optional)

set(sol2_old_emplace [=[
		template <class... Args>
		T& emplace(Args&&... args) noexcept {
			static_assert(std::is_constructible<T, Args&&...>::value, "T must be constructible with Args");

			*this = nullopt;
			this->construct(std::forward<Args>(args)...);
		}
]=])
set(sol2_new_emplace [=[
		template <class U = T>
		T& emplace(U&& value) noexcept {
			static_assert(std::is_lvalue_reference<U>::value, "value must be an lvalue");
			m_value = std::addressof(value);
			return *m_value;
		}
]=])

string(FIND "${sol2_optional}" "${sol2_old_emplace}" sol2_emplace_position)
if(sol2_emplace_position EQUAL -1)
	message(FATAL_ERROR "sol2 optional reference emplace patch no longer matches")
endif()
string(REPLACE "${sol2_old_emplace}" "${sol2_new_emplace}" sol2_optional "${sol2_optional}")

# keep the dependency and its license intact; only override the broken header.
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/sol2-compat/sol/optional_implementation.hpp" CONTENT "${sol2_optional}")
target_include_directories(sol2 BEFORE INTERFACE "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/sol2-compat>")
