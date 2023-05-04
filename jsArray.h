#pragma once

#include <vector>
#include <type_traits>
#include <algorithm>

// nested "template<typename> class" is so when a new JSArray is made by map
// with a different return type, it still uses the correct allocator (std::allocator
// is itself a template class that accepts a type)
template<typename T, template<typename> class AllocTemplate = std::allocator>
class JSArray : public std::vector<T, AllocTemplate<T>>
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
    inline typename function_traits<F>::return_t standardCallbackHandler(F callback, std::size_t index) const noexcept
    {
        constexpr std::size_t argsCount = function_traits<F>::argsCount;
        if constexpr (argsCount == 1)
            return callback((*this)[index]);
        if constexpr (argsCount == 2)
            return callback((*this)[index], index);
        if constexpr (argsCount == 3)
            return callback((*this)[index], index, *this);

        static_assert(
            argsCount <= 3 && argsCount >= 1,
            "\nFunction signature should look like either of these three:\n"
            "return_type (T val)\n"
            "return_type (T val, std::size_t index)\n"
            "return_type (T val, std::size_t index, const JSArray<T, AllocTemplate>& self)\n"
            "auto is not allowed..."
        );
    }

    template<typename F>
    inline typename function_traits<F>::return_t reduceCallbackHandler(F callback, const typename function_traits<F>::return_t& accumulator, std::size_t index) const noexcept
    {
        constexpr std::size_t argsCount = function_traits<F>::argsCount;
        if constexpr (argsCount == 2)
            return callback(accumulator, (*this)[index]);
        if constexpr (argsCount == 3)
            return callback(accumulator, (*this)[index], index);
        if constexpr (argsCount == 4)
            return callback(accumulator, (*this)[index], index, *this);

        static_assert(
            argsCount <= 4 && argsCount >= 2,
            "\nFunction signature should look like either of these three:\n"
            "return_type (T accumulator, T val)\n"
            "return_type (T accumulator, T val, std::size_t index)\n"
            "return_type (T accumulator, T val, std::size_t index, const JSArray<T, AllocTemplate>& self)\n"
            "auto is not allowed..."
        );
    }

public:

    using std::vector<T, AllocTemplate<T>>::vector; // inherit all constructors from std::vector

    template<typename F>
    using vectorEligibleCallbackReturn_t = std::remove_reference_t<typename function_traits<F>::return_t>;

    template<typename F>
    using callBackReturnTypeNonConst_t = std::remove_const_t<typename function_traits<F>::return_t>;

    template<typename F>
    inline JSArray<vectorEligibleCallbackReturn_t<F>, AllocTemplate> map(F callback) const noexcept
    {
        JSArray<vectorEligibleCallbackReturn_t<F>, AllocTemplate> result(this->size());
        for (std::size_t i = 0; i < this->size(); i += 1)
        {
            result[i] = this->standardCallbackHandler(callback, i);
        }

        return result;
    }

    template<typename F>
    inline callBackReturnTypeNonConst_t<F> reduce(F callback, const typename function_traits<F>::return_t& initValue) const noexcept
    {
        callBackReturnTypeNonConst_t<F> result = initValue;
        for (std::size_t i = 0; i < this->size(); i += 1)
        {
            result = this->reduceCallbackHandler(callback, result, i);
        }

        return result;
    }

    template<typename F>
    inline callBackReturnTypeNonConst_t<F> reduceRight(F callback, const typename function_traits<F>::return_t& initValue) const noexcept
    {
        callBackReturnTypeNonConst_t<F> result = initValue;
        for (std::size_t i = 0; i < this->size(); i += 1)
        {
            result = this->reduceCallbackHandler(callback, result, this->size() - 1 - i);
        }

        return result;
    }

    template<typename F>
    inline JSArray<T, AllocTemplate> filter(F callback) const noexcept
    {
        static_assert(std::is_same_v<callBackReturnTypeNonConst_t<F>, bool>, "callback return type must be bool!!!");

        JSArray<T, AllocTemplate> result;
        for (std::size_t i = 0; i < this->size(); i += 1)
        {
            if (this->standardCallbackHandler(callback, i))
                result.push_back((*this)[i]);
        }

        return result;
    }

    template<typename F>
    inline bool every(F callback) const noexcept
    {
        static_assert(std::is_same_v<callBackReturnTypeNonConst_t<F>, bool>, "callback return type must be bool!!!");
        std::size_t i = 0;
        for (; i < this->size() && this->standardCallbackHandler(callback, i); i += 1) {}
        return i == this->size();
    }

    template<typename F>
    inline bool some(F callback) const noexcept
    {
        static_assert(std::is_same_v<callBackReturnTypeNonConst_t<F>, bool>, "callback return type must be bool!!!");

        bool result = false;
        for (std::size_t i = 0; i < this->size() && !result; i += 1)
        {
            result = this->standardCallbackHandler(callback, i);
        }

        return result;
    }

    template<typename F>
    inline JSArray<T, AllocTemplate>& sort(F compareFunc) noexcept
    {
        std::sort(this->begin(), this->end(), compareFunc);
        return *this;
    }

    template<typename F>
    inline JSArray<T, AllocTemplate> toSorted(F compareFunc) const noexcept
    {
        JSArray<T, AllocTemplate> result = *this;
        std::sort(result.begin(), result.end(), compareFunc);
        return result;
    }
};
