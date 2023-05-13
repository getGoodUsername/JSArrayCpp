#pragma once

#include <vector>
#include <type_traits>
#include <algorithm>

/**
 * @brief A dynamic array class to emulate key javascript array
 * methods like map and reduce. Class inherits publicly from std::vector.
 * 
 * @tparam T                element type of dynamic array. (ex. int, JSArray<int>, double, std::string)
 * @tparam AllocTemplate    allocator template class accepting only one template paramater "T" element type (ex. std::allocator)
 * 
 * @note AllocTemplate is the way it is so you are allowed to return different types from .map();
 */
template<typename T, template<typename> class AllocTemplate = std::allocator>
class JSArray : public std::vector<T, AllocTemplate<T>>
{
private:
// START OF FUNCTION TRAITS META PROGRAMMING CODE
    // just to convey intention in code
    using element_t = T;
    using index_t = std::size_t;
    using self_t = JSArray<T, AllocTemplate>;

    // the programer should not use this directly. use StandardCallbackTraits<F>::return_t
    template<typename F, std::size_t arity>
    struct GetStandardCallBackReturnType;

    template<typename F>
    struct GetStandardCallBackReturnType<F, 1> {using type = std::invoke_result_t<F, element_t&>;};

    template<typename F>
    struct GetStandardCallBackReturnType<F, 2> {using type = std::invoke_result_t<F, element_t&, index_t>;};

    template<typename F>
    struct GetStandardCallBackReturnType<F, 3> {using type = std::invoke_result_t<F, element_t&, index_t, self_t&>;};




    // the programer should not use this directly. use ReduceCallbackTraits<F, Accumulator_t>::return_t
    template<typename F, typename Accumulator_t, std::size_t arity>
    struct GetReduceCallBackReturnType;

    template<typename F, typename Accumulator_t>
    struct GetReduceCallBackReturnType<F, Accumulator_t, 2> {using type = std::invoke_result_t<F, Accumulator_t&, element_t&>;};

    template<typename F, typename Accumulator_t>
    struct GetReduceCallBackReturnType<F, Accumulator_t, 3> {using type = std::invoke_result_t<F, Accumulator_t&, element_t&, index_t>;};

    template<typename F, typename Accumulator_t>
    struct GetReduceCallBackReturnType<F, Accumulator_t, 4> {using type = std::invoke_result_t<F, Accumulator_t&, element_t&, index_t, self_t&>;};


    // do this to make it more obvious where char(&)[*number of parameters*] comes from in callback traits structs
    template<typename... Args>
    using Arity_Return_t = char(&)[sizeof...(Args)]; // can't return an array from a function, but can return a reference to an array;


#if __cplusplus >= 202002L // I am most confident in the version that requires c++20.
    template<typename F>
    struct StandardCallbackTraits
    {
        /**
         * this struct (along with ReduceCallbackTraits) was made to allow for auto lambdas,
         * aka [](auto a, auto b){}; previously the function_traits struct forced defined types,
         * and due to the method of meta programming, the callbacks were not allowed to contain auto
         * parameters. This fixes that, and makes the interfaces just that much easier to use.
         * 
         * Note about the following type sequences:
         * element_t&
         * element_t& index_t
         * element_t& index_t self_t&
         * 
         * In std::invoke_result_t && std::invocable, as far as I understand it,
         * these types are the ones that are "passed" into the function, and therefore
         * I choose the most unconstrained types that the callback function itself
         * can later choose to constrain with const, volatile, or just straight make a
         * copy without any reference. There might be the question of "well all the methods
         * that are being added (map, reduce, etc) are const and therefore I should actually
         * pass in const element_t& and const self_t&", but I would prefer for the error to be
         * thrown closer to the calling method than here at the compile time meta programming stage.
         *
         * Oh also, index_t is not passed as a reference as I don't want the callback mucking
         * up my index variable. So it must always be passed by value. Having the callback
         * modify the index can result in very undefined behavior so we won't allow for that.
         * And in addition there is no later mechanism from the const methods to say that
         * a reference to index_t is not ok so I have to put my foot down here unfortunately.
         * 
         * A note though. r value references are not allowed as types here
         * tbh I don't feel like I know them well enough to implement them so I'll
         * stay away.
         */


        // please notice that the template parameters for both the return type and requires clause are the same other than the
        // the function type "F" in std::invocable. Make sure this is always true.
        static Arity_Return_t<element_t&>                   test() requires std::invocable<F, element_t&>;
        static Arity_Return_t<element_t&, index_t>          test() requires std::invocable<F, element_t&, index_t>;
        static Arity_Return_t<element_t&, index_t, self_t&> test() requires std::invocable<F, element_t&, index_t, self_t&>;

        static constexpr std::size_t arity = sizeof(test());
        using return_t = typename GetStandardCallBackReturnType<F, arity>::type;
    };

    template<typename F, typename Accumulator_t>
    struct ReduceCallbackTraits
    {
        // virtually the same as StandardCallbackTraits except the range of acceptable
        // arity is [2, 4] and there needs to be an Accumulator_t
        static Arity_Return_t<Accumulator_t&, element_t&>                   test() requires std::invocable<F, Accumulator_t&, element_t&>;
        static Arity_Return_t<Accumulator_t&, element_t&, index_t>          test() requires std::invocable<F, Accumulator_t&, element_t&, index_t>;
        static Arity_Return_t<Accumulator_t&, element_t&, index_t, self_t&> test() requires std::invocable<F, Accumulator_t&, element_t&, index_t, self_t&>;

        static constexpr std::size_t arity = sizeof(test());
        using return_t = typename GetReduceCallBackReturnType<F, Accumulator_t, arity>::type;
    };

// +++++++++++++++++++++++++++++++++++ I am not as confident on the c++17 version, use std++20 if possible +++++++++++++++++++++++++++++++++++ //
#elif __cplusplus >= 201703

    template<typename F_copy, typename... Ts>
    using GetNormalFuncType_t = std::invoke_result_t<F_copy, Ts...>(Ts...);

    template<typename F>
    struct StandardCallbackTraits
    {
        /**
         * as far as I understand, using std::function and overloading
         * with different types ends up having the same effect in practice
         * as requires std::invocable. Still I am more confident about the
         * c++ 20 version that uses "requires std::invocable"
         */

        // templates on test functions are needed to avoid getting error. This lazy evaluates, with "F_copy",
        // GetNormalFuncType_t and only when the right overload is chosen, instead of eager evaluating with "F"
        // and having an error when arity doesn't match.
        template<typename F_copy>
        static Arity_Return_t<element_t&>                   test(std::function<GetNormalFuncType_t<F_copy, element_t&>>);

        template<typename F_copy>
        static Arity_Return_t<element_t&, index_t>          test(std::function<GetNormalFuncType_t<F_copy, element_t&, index_t>>);

        template<typename F_copy>
        static Arity_Return_t<element_t&, index_t, self_t&> test(std::function<GetNormalFuncType_t<F_copy, element_t&, index_t, self_t&>>);

        static constexpr std::size_t arity = sizeof(test<F>(std::declval<F>()));
        using return_t = typename GetStandardCallBackReturnType<F, arity>::type;
    };

    template<typename F, typename Accumulator_t>
    struct ReduceCallbackTraits
    {
        template<typename F_copy>
        static Arity_Return_t<Accumulator_t&, element_t&>                   test(std::function<GetNormalFuncType_t<F_copy, Accumulator_t&, element_t&>>);

        template<typename F_copy>
        static Arity_Return_t<Accumulator_t&, element_t&, index_t>          test(std::function<GetNormalFuncType_t<F_copy, Accumulator_t&, element_t&, index_t>>);

        template<typename F_copy>
        static Arity_Return_t<Accumulator_t&, element_t&, index_t, self_t&> test(std::function<GetNormalFuncType_t<F_copy, Accumulator_t&, element_t&, index_t, self_t&>>);

        static constexpr std::size_t arity = sizeof(test<F>(std::declval<F>()));
        using return_t = typename GetReduceCallBackReturnType<F, Accumulator_t, arity>::type;
    };
#endif
// END OF FUNCTION TRAITS META PROGRAMMING CODE









    template<typename U>
    using makeVectorEligibleType = std::remove_reference_t<U>;

    template<typename U>
    using makeMutableType = std::remove_const_t<U>;


    template<typename F>
    inline typename StandardCallbackTraits<F>::return_t standardCallbackHandler(F callback, std::size_t currLoopIndex) const noexcept
    {
        constexpr std::size_t argsCount = StandardCallbackTraits<F>::arity;
        if constexpr (argsCount == 1)
            return callback((*this)[currLoopIndex]);
        if constexpr (argsCount == 2)
            return callback((*this)[currLoopIndex], currLoopIndex);
        if constexpr (argsCount == 3)
            return callback((*this)[currLoopIndex], currLoopIndex, *this);

        static_assert(
            argsCount <= 3 && argsCount >= 1,
            "\nFunction signature should look like either of these three: (1 to 3 params max)\n"
            "return_type (auto&& val)\n"
            "return_type (auto&& val, auto&& index)\n"
            "return_type (auto&& val, auto&& index, auto&& self)\n"
            "you can also define explicitly the types if you want. NOTE, the index type must be an integral type and NOT a reference\n"
        );
    }

    template<typename F, typename Accumulator_t>
    inline typename ReduceCallbackTraits<F, Accumulator_t>::return_t reduceCallbackHandler(F callback, std::remove_const_t<Accumulator_t>& accumulator, std::size_t currLoopIndex) const noexcept
    {
        // I remove const from Accumulator_t to allow the most permissive type to be passed into
        // callback. Remember this is the "actual" accumulator variable and it's declared and defined internally.
        // The accumulator declared by the callback acts as nothing more than accessor. The callback will
        // restrict with cv qualifiers if needed. Also make sure its a ref just incase the accumulator
        // type is big and heavy to minimize copying.

        constexpr std::size_t argsCount = ReduceCallbackTraits<F, Accumulator_t>::arity;
        if constexpr (argsCount == 2)
            return callback(accumulator, (*this)[currLoopIndex]);
        if constexpr (argsCount == 3)
            return callback(accumulator, (*this)[currLoopIndex], currLoopIndex);
        if constexpr (argsCount == 4)
            return callback(accumulator, (*this)[currLoopIndex], currLoopIndex, *this);

        static_assert(
            argsCount <= 4 && argsCount >= 2,
            "\nFunction signature should look like either of these three (2 to 4 params max):\n"
            "return_type (auto&& accumulator, auto&& val)\n"
            "return_type (auto&& accumulator, auto&& val, auto&& index)\n"
            "return_type (auto&& accumulator, auto&& val, auto&& index, auto&& self)\n"
            "you can also define explicitly the types if you want. NOTE, the index type must be an integral type and NOT a reference\n"
        );
    }

public:

    // if confused about this line go here: https://en.cppreference.com/w/cpp/language/using_declaration
    using std::vector<element_t, AllocTemplate<element_t>>::vector; // inherit all constructors from std::vector


    /**
     * @brief creates a new array populated with the results of calling a provided function on every element in the calling array
     * 
     * @tparam F callback type
     * @param callback a lambda, a function ptr, or a functor (an object with operator() overloaded). Can be 1, 2, or 3 arguments (value, index, self)
     * @return JSArray<makeVectorEligibleType<typename StandardCallbackTraits<F>::return_t>, AllocTemplate>
     * 
     * @note
     * Look here for more information on callback parameters: @ref https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/map#parameters
     */
    template<typename F>
    inline JSArray<makeVectorEligibleType<typename StandardCallbackTraits<F>::return_t>, AllocTemplate> map(F callback) const noexcept
    {
        JSArray<makeVectorEligibleType<typename StandardCallbackTraits<F>::return_t>, AllocTemplate> result(this->size());
        for (std::size_t i = 0; i < this->size(); i += 1)
        {
            result[i] = this->standardCallbackHandler(callback, i);
        }

        return result;
    }

    /**
     * @brief executes a user-supplied "reducer" callback function on each element of the array, in order,
     * passing in the return value from the calculation on the preceding element. The final result of
     * running the reducer across all elements of the array is a single value.
     * 
     * @tparam F callback type
     * @param callback a lambda, a function ptr, or a functor (an object with operator() overloaded). Can be 2, 3, or 4 arguments (accumulator, value, index, self)
     * @param initValue initial value for the accumulator param (0th paramater)
     * @return Accumulator_t
     *
     * @note
     * Look here for more information on callback parameters: @ref https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/reduce#parameters
     */
    template<typename Accumulator_t, typename F> // flipped Accumulator_t as first template param for when Accumulator_t can't be deduced don't have to put the type of F
    inline Accumulator_t reduce(F callback, const Accumulator_t& initValue) const noexcept
    {
        makeMutableType<Accumulator_t> result = initValue;
        for (std::size_t i = 0; i < this->size(); i += 1)
        {
            result = this->reduceCallbackHandler<F, Accumulator_t>(callback, result, i);
        }

        return result;
    }

    /**
     * @brief applies a function against an accumulator and each value of the array (from right-to-left) to reduce it to a single value. 
     * 
     * @tparam F callback type
     * @param callback a lambda, a function ptr, or a functor (an object with operator() overloaded). Can be 2, 3, or 4 arguments (accumulator, value, index, self)
     * @param initValue initial value for the accumulator param (0th paramater)
     * @return Accumulator_t
     *
     * @note
     * Look here for more information on callback parameters: @ref https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/reduceRight#parameters
     */
    template<typename Accumulator_t, typename F>
    inline Accumulator_t reduceRight(F callback, const Accumulator_t& initValue) const noexcept
    {
        makeMutableType<Accumulator_t> result = initValue;
        for (std::size_t i = 0; i < this->size(); i += 1)
        {
            result = this->reduceCallbackHandler<F, Accumulator_t>(callback, result, this->size() - 1 - i);
        }

        return result;
    }

    /**
     * @brief method executes a provided callback function once for each array element.
     * 
     * @tparam F callback type
     * @param callback a lambda, a function ptr, or a functor (an object with operator() overloaded). Can be 1, 2, or 3 arguments (value, index, self)
     * 
     * @note
     * Look here for more information on callback parameters: @ref https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/forEach#parameters
     */
    template<typename F>
    inline void forEach(F callback)
    {
        for (std::size_t i = 0; i < this->size(); i += 1)
        {
            this->standardCallbackHandler(callback, i);
        }
    }

    /**
     * @brief creates a copy of a portion of a given array, filtered down to just the elements from the given array that pass the test implemented by the provided function.
     * 
     * @tparam F callback type
     * @param callback a lambda, a function ptr, or a functor (an object with operator() overloaded). Can be 1, 2, or 3 arguments (value, index, self)
     * @return JSArray<T, AllocTemplate> 
     * 
     * @note
     * Look here for more information on callback parameters: @ref https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/filter#parameters
     */
    template<typename F>
    inline JSArray<element_t, AllocTemplate> filter(F callback) const noexcept
    {
        static_assert(
            std::is_same_v<std::remove_cv_t<typename StandardCallbackTraits<F>::return_t>, bool>,
            "callback return type must be bool!!!"
        );

        JSArray<element_t, AllocTemplate> result;
        for (std::size_t i = 0; i < this->size(); i += 1)
        {
            if (this->standardCallbackHandler(callback, i))
                result.push_back((*this)[i]);
        }

        return result;
    }

    /**
     * @brief tests whether all elements in the array pass the test implemented by the provided function.
     * 
     * @tparam F callback type
     * @param callback a lambda, a function ptr, or a functor (an object with operator() overloaded). Can be 1, 2, or 3 arguments (value, index, self)
     * @return bool
     * 
     * @note
     * Look here for more information on callback parameters: @ref https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/every#parameters
     */
    template<typename F>
    inline bool every(F callback) const noexcept
    {
        static_assert(
            std::is_same_v<std::remove_cv_t<typename StandardCallbackTraits<F>::return_t>, bool>,
            "callback return type must be bool!!!"
        );

        std::size_t i = 0;
        for (; i < this->size() && this->standardCallbackHandler(callback, i); i += 1) {}
        return i == this->size();
    }

    /**
     * @brief tests whether at least one element in the array passes the test implemented by the provided function.
     * It returns true if, in the array, it finds an element for which the provided function returns true;
     * otherwise it returns false. It doesn't modify the array. 
     * 
     * @tparam F callback type
     * @param callback a lambda, a function ptr, or a functor (an object with operator() overloaded). Can be 1, 2, or 3 arguments (value, index, self)
     * @return bool
     * 
     * @note
     * Look here for more information on callback parameters: @ref https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/some#parameters
     */
    template<typename F>
    inline bool some(F callback) const noexcept
    {
        static_assert(
            std::is_same_v<std::remove_cv_t<typename StandardCallbackTraits<F>::return_t>, bool>,
            "callback return type must be bool!!!"
        );

        bool result = false;
        for (std::size_t i = 0; i < this->size() && !result; i += 1)
        {
            result = this->standardCallbackHandler(callback, i);
        }

        return result;
    }

    /**
     * @brief sort all the elements inplace in ascending order
     * 
     * @return JSArray<T, AllocTemplate>& 
     */
    inline JSArray<element_t, AllocTemplate>& sort() noexcept
    {
        std::sort(this->begin(), this->end(), [](const element_t& a, const element_t& b){return a < b;});
        return *this;
    }

    /**
     * @brief sort all the elements inplace according to the callback function
     * 
     * @tparam F callback type
     * @param compareFunc a lambda, a function ptr, or a functor (an object with operator() overloaded) with two arguments
     * @return JSArray<T, AllocTemplate>& 
     * 
     */
    template<typename F>
    inline JSArray<element_t, AllocTemplate>& sort(F compareFunc) noexcept
    {
        std::sort(this->begin(), this->end(), compareFunc);
        return *this;
    }

    /**
     * @brief make a copy of the current array and sort in ascending order.
     * 
     * @return JSArray<T, AllocTemplate> 
     */
    inline JSArray<element_t, AllocTemplate> toSorted() const noexcept
    {
        JSArray<element_t, AllocTemplate> result = *this;
        std::sort(result.begin(), result.end(), [](const element_t& a, const element_t& b){return a < b;});
        return result;
    }

    /**
     * @brief make a copy of the current array and sort according to the callback function
     * 
     * @tparam F callback type
     * @param compareFunc a lambda, a function ptr, or a functor (an object with operator() overloaded) with two arguments
     * @return JSArray<T, AllocTemplate> 
     * 
     */
    template<typename F>
    inline JSArray<element_t, AllocTemplate> toSorted(F compareFunc) const noexcept
    {
        JSArray<element_t, AllocTemplate> result = *this;
        std::sort(result.begin(), result.end(), compareFunc);
        return result;
    }
};
