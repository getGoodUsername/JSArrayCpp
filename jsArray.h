#pragma once

#include <vector>
#include <type_traits>
#include <algorithm>

/**
 * @brief A dynamic array class to emulate key java script array
 * methods like map and reduce. Class inherits publicly from std::vector.
 * 
 * @tparam T                element type of dynamic array. (ex. int, JS<int>, double, std::string)
 * @tparam AllocTemplate    allocator template class accepting only one template paramater "T" element type (ex. std::allocator)
 * 
 * @warning
 * All functions, excepts the sort functions, don't allow
 * callbacks with auto parameters.
 * Why? All the other functions accept three different versions
 * of the callback, and due to this I have to figure out
 * how many arguments the callback takes at compile time before
 * using it. The problem is that I can't seem to figure out a
 * way to extract this information when the callback uses
 * auto in it's parameters. Maybe one day this feature will be
 * added but I just ran out of skill here. Therefore for all
 * the callbacks, you must define types and CANNOT use auto for
 * parameters (return type of auto is fine though).
 */
template<typename T, template<typename> class AllocTemplate = std::allocator>
class JSArray : public std::vector<T, AllocTemplate<T>>
{
    // function_traits meta-programming heavily influenced from: https://stackoverflow.com/a/7943765
private:
    // base
    template <typename F>
    struct function_traits;

    // get info from "normal" callback function
    template <typename R, typename... Args>
    struct function_traits<R(Args...)>
    {
        using return_type = R;
        static constexpr std::size_t argsCount = sizeof...(Args);
    };

    /**
     * get info from a functor or lambda. &F::operator() gets
     * the pointer to the member function named "operator()"
     * which is a thing not only for functors (objects with operator() defined)
     * but also lambdas as they are also basically functors in some ways
     * (look at cpp reference website)
     * 
     * fails with lambdas containing auto parameters since the func "operator()"
     * can now be overloaded (with template arguments) and decltype can't know
     * which types it will be overloaded with, and you can't define types
     * to overload operator() with since there is obviously the case where
     * the lambda does not have auto parameters and that won't work. so yeah
     * just stuck with no auto...
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
        using return_type = R;
        static constexpr std::size_t argsCount = sizeof...(Args);
    };

    // for const member functions.
    template <typename C, typename R, typename... Args>
    struct function_traits<R(C::*)(Args...) const>
    {
        using return_type = R;
        static constexpr std::size_t argsCount = sizeof...(Args);
    };




    template<typename U>
    using makeVectorEligibleType = std::remove_reference_t<U>;

    template<typename U>
    using makeMutableType = std::remove_const_t<U>;

    template<typename F>
    using getCallbackReturnType = typename function_traits<F>::return_type;




    template<typename F>
    inline getCallbackReturnType<F> standardCallbackHandler(F callback, std::size_t currLoopIndex) const noexcept
    {
        constexpr std::size_t argsCount = function_traits<F>::argsCount;
        if constexpr (argsCount == 1)
            return callback((*this)[currLoopIndex]);
        if constexpr (argsCount == 2)
            return callback((*this)[currLoopIndex], currLoopIndex);
        if constexpr (argsCount == 3)
            return callback((*this)[currLoopIndex], currLoopIndex, *this);

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
    inline getCallbackReturnType<F> reduceCallbackHandler(F callback, std::remove_const_t<getCallbackReturnType<F>>& accumulator, std::size_t currLoopIndex) const noexcept
    {
        // I remove const from the call back return type to allow the most permissive type to be passed into
        // callback. Callback will restrict if needed. Also just incase the accumulator type is big/heavy

        constexpr std::size_t argsCount = function_traits<F>::argsCount;
        if constexpr (argsCount == 2)
            return callback(accumulator, (*this)[currLoopIndex]);
        if constexpr (argsCount == 3)
            return callback(accumulator, (*this)[currLoopIndex], currLoopIndex);
        if constexpr (argsCount == 4)
            return callback(accumulator, (*this)[currLoopIndex], currLoopIndex, *this);

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


    /**
     * @brief 
     * 
     * @tparam F 
     * @param callback a function ptr
     * @return JSArray<makeVectorEligibleType<getCallbackReturnType<F>>, AllocTemplate> 
     * 
     * @warning
     * No auto parameters allowed!
     */
    template<typename F>
    inline JSArray<makeVectorEligibleType<getCallbackReturnType<F>>, AllocTemplate> map(F callback) const noexcept
    {
        JSArray<makeVectorEligibleType<getCallbackReturnType<F>>, AllocTemplate> result(this->size());
        for (std::size_t i = 0; i < this->size(); i += 1)
        {
            result[i] = this->standardCallbackHandler(callback, i);
        }

        return result;
    }

    template<typename F>
    inline getCallbackReturnType<F> reduce(F callback, const getCallbackReturnType<F>& initValue) const noexcept
    {
        makeMutableType<getCallbackReturnType<F>> result = initValue;
        for (std::size_t i = 0; i < this->size(); i += 1)
        {
            result = this->reduceCallbackHandler(callback, result, i);
        }

        return result;
    }

    template<typename F>
    inline getCallbackReturnType<F> reduceRight(F callback, const getCallbackReturnType<F>& initValue) const noexcept
    {
        makeMutableType<getCallbackReturnType<F>> result = initValue;
        for (std::size_t i = 0; i < this->size(); i += 1)
        {
            result = this->reduceCallbackHandler(callback, result, this->size() - 1 - i);
        }

        return result;
    }

    template<typename F>
    inline void forEach(F callback)
    {
        for (std::size_t i = 0; i < this->size(); i += 1)
        {
            this->standardCallbackHandler(callback, i);
        }
    }

    template<typename F>
    inline JSArray<T, AllocTemplate> filter(F callback) const noexcept
    {
        static_assert(
            std::is_same_v<std::remove_cv_t<getCallbackReturnType<F>>, bool>,
            "callback return type must be bool!!!"
        );

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
        static_assert(
            std::is_same_v<std::remove_cv_t<getCallbackReturnType<F>>, bool>,
            "callback return type must be bool!!!"
        );

        std::size_t i = 0;
        for (; i < this->size() && this->standardCallbackHandler(callback, i); i += 1) {}
        return i == this->size();
    }

    template<typename F>
    inline bool some(F callback) const noexcept
    {
        static_assert(
            std::is_same_v<std::remove_cv_t<getCallbackReturnType<F>>, bool>,
            "callback return type must be bool!!!"
        );

        bool result = false;
        for (std::size_t i = 0; i < this->size() && !result; i += 1)
        {
            result = this->standardCallbackHandler(callback, i);
        }

        return result;
    }

    inline JSArray<T, AllocTemplate>& sort() noexcept
    {
        std::sort(this->begin(), this->end(), [](const T& a, const T& b){return a < b;});
        return *this;
    }

    template<typename F>
    inline JSArray<T, AllocTemplate>& sort(F compareFunc) noexcept
    {
        std::sort(this->begin(), this->end(), compareFunc);
        return *this;
    }

    inline JSArray<T, AllocTemplate> toSorted() const noexcept
    {
        JSArray<T, AllocTemplate> result = *this;
        std::sort(result.begin(), result.end(), [](const T& a, const T& b){return a < b;});
        return result;
    }

    template<typename F>
    inline JSArray<T, AllocTemplate> toSorted(F compareFunc) const noexcept
    {
        JSArray<T, AllocTemplate> result = *this;
        std::sort(result.begin(), result.end(), compareFunc);
        return result;
    }
};
