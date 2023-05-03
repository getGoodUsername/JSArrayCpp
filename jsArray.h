#pragma once

#include <vector>
#include <type_traits>

//nested "template<typename> class" is so when a new JSArray is made by map
// with a different return type, it still uses the correct allocator
template<typename T, template<typename> class Alloc = std::allocator>
class JSArray : public std::vector<T, Alloc<T>>
{
private:
    // base
    template <typename F>
    struct function_traits;

    // get info from "normal" callback function
    template <typename R, typename... Args>
    struct function_traits<R(Args...)>
    {
        using return_t = R;
        static constexpr std::size_t argsCount = sizeof...(Args);
    };

    /**
     * get info from a functor or lambda. &F::operator() gets
     * the pointer to the member function named "operator()"
     * which is a thing not only for functors (objects with operator() defined)
     * but also lambdas as they are also basically functors in some ways
     * (look at cpp reference website)
     */
    template <typename F>
    struct function_traits : public function_traits<decltype(&F::operator())> {};

    /**
     * get info from a pointer to the member function
     * NEEDED even if don't want to support pointer to the member functions
     * as a suitable callback as the lambda/functor specialization
     * use the pointer to the member function mechanism
     * btw, (C::*) is just like a normal function pointer
     * type, but it is also saying that the function comes from class C.
     */
    template <typename C, typename R, typename... Args>
    struct function_traits<R(C::*)(Args...)>
    {
        using return_t = R;
        static constexpr std::size_t argsCount = sizeof...(Args);
    };

    // for const member functions.
    template <typename C, typename R, typename... Args>
    struct function_traits<R(C::*)(Args...) const>
    {
        using return_t = R;
        static constexpr std::size_t argsCount = sizeof...(Args);
    };

    template<typename F>
    using vectorEligibleCallbackReturn_t = std::remove_reference_t<std::remove_cv_t<typename function_traits<F>::return_t>>;

    template<typename F>
    inline typename function_traits<F>::return_t callbackHandler(F callback, const T& val, std::size_t index) const
    {
        constexpr std::size_t argsCount = function_traits<F>::argsCount;
        if constexpr (argsCount == 1)
            return callback(val);
        if constexpr (argsCount == 2)
            return callback(val, index);
        if constexpr (argsCount == 3)
            return callback(val, index, *this);

        static_assert(
            argsCount <= 3 && argsCount >= 1,
            "Function signature should look like either of these three:\n"
            "return_type (T val)\n"
            "return_type (T val, std::size_t index)\n"
            "return_type (T val, std::size_t index, const JSArray<T, Alloc>& self)\n"
            "auto is not allowed..."
        );
    }

public:

    using std::vector<T, Alloc<T>>::vector; // inherit all constructors from Base

    template<typename F>
    JSArray<vectorEligibleCallbackReturn_t<F>, Alloc> map(F callback) const
    {
        JSArray<vectorEligibleCallbackReturn_t<F>, Alloc> result(this->size());
        for (std::size_t i = 0; i < this->size(); i += 1)
        {
            result[i] = this->callbackHandler(callback, (*this)[i], i);
        }

        return result;
    }
};
