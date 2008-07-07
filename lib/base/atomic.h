
//  Copyright (C) 2007, 2008 Tim Blechmann & Thomas Grill
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  Disclaimer: Not a Boost library.

/* Copyright (c) 2008, Stefan Eilemann <eile@equalizergraphics.com> 
   Modifications to use within eq::base namespace and naming conventions.
   Original at http://tim.klingt.org/git?p=boost_lockfree.git;a=tree
*/

#ifndef EQBASE_ATOMIC_H
#define EQBASE_ATOMIC_H

#include <eq/base/nonCopyable.h>    // base class
#include <eq/base/compareAndSwap.h> // used in inline methods
#include <eq/base/spinLock.h>       // used in inline methods

namespace eq
{
namespace base
{
#if defined(__GNUC__) && ( (__GNUC__ > 4) || ((__GNUC__ >= 4) && (__GNUC_MINOR__ >= 1)) )

template <typename T>
class Atomic: public NonCopyable
{
public:
    explicit Atomic( T v = 0 )
            : _value(v)
    {}

    operator T(void) const
    {
        return __sync_fetch_and_add(&_value, 0);
    }

    void operator =(T v)
    {
        _value = v;
        __sync_synchronize();
    }

    T operator +=(T v)
    {
        return __sync_add_and_fetch(&_value, v);
    }

    T operator -=(T v)
    {
        return __sync_sub_and_fetch(&_value, v);
    }

    /* prefix operator */
    T operator ++(void)
    {
        return __sync_add_and_fetch(&_value, 1);
    }

    /* prefix operator */
    T operator --(void)
    {
        return __sync_sub_and_fetch(&_value, 1);
    }

    /* postfix operator */
    T operator ++(int)
    {
        return __sync_fetch_and_add(&_value, 1);
    }

    /* postfix operator */
    T operator --(int)
    {
        return __sync_fetch_and_sub(&_value, 1);
    }

private:
    mutable T _value;
};

#elif defined(__GLIBCPP__) || defined(__GLIBCXX__)

template <typename T>
class Atomic: public NonCopyable
{
public:
    explicit Atomic(T v = 0)
        : _value(v)
    {}

    operator T(void) const
    {
        return __gnu_cxx::__exchange_and_add(&_value, 0);
    }

    void operator =(T v)
    {
        _value = v;
    }

    T operator +=(T v)
    {
        return __gnu_cxx::__exchange_and_add(&_value, v) + v;
    }

    T operator -=(T v)
    {
        return __gnu_cxx::__exchange_and_add(&_value, -v) - v;
    }

    /* prefix operator */
    T operator ++(void)
    {
        return operator+=(1);
    }

    /* prefix operator */
    T operator --(void)
    {
        return operator-=(1);
    }

    /* postfix operator */
    T operator ++(int)
    {
        return __gnu_cxx::__exchange_and_add(&_value, 1);
    }

    /* postfix operator */
    T operator --(int)
    {
        return __gnu_cxx::__exchange_and_add(&_value, -1);
    }

private:
    mutable _Atomic_word _value;
};

#elif defined (EQ_HAS_COMPARE_AND_SWAP) /* emulate via compareAndSwap */

template <typename T>
class Atomic: public NonCopyable
{
public:
    explicit Atomic(T v = 0)
    {
        *this = v;
    }

    operator T(void) const
    {
        memoryBarrier();
        return _value;
    }

    void operator =(T v)
    {
        _value = v;
        memoryBarrier();
    }

    /* prefix operator */
    T operator ++()
    {
        return *this += 1;
    }

    /* prefix operator */
    T operator --()
    {
        return *this -= 1;
    }

    T operator +=(T v)
    {
        for(;;)
        {
            T newv = _value+v;
            if(compareAndSwap(&_value,_value,newv))
                return newv;
        }
    }

    T operator -=(T v)
    {
        for(;;)
        {
            T newv = _value-v;
            if(compareAndSwap(&_value,_value,newv))
                return newv;
        }
    }

    /* postfix operator */
    T operator ++(int)
    {
        for(;;)
        {
            T oldv = _value;
            if(compareAndSwap(&_value,oldv,oldv+1))
                return oldv;
        }
    }

    /* postfix operator */
    T operator --(int)
    {
        for(;;)
        {
            T oldv = _value;
            if(compareAndSwap(&_value,oldv,oldv-1))
                return oldv;
        }
    }

private:
    T _value;
};

#else

template <typename T>
class Atomic: public NonCopyable
{
public:
    explicit Atomic(T v = 0)
    {
        *this = v;
    }

    operator T(void) const
    {
        return _value;
    }

    void operator =(T v)
    {
        _lock.set();
        _value = v;
        _lock.unset();
    }

    /* prefix operator */
    T operator ++()
    {
        return *this += 1;
    }

    /* prefix operator */
    T operator --()
    {
        return *this -= 1;
    }

    T operator +=(T v)
    {
        _lock.set();
        _value += v;
        T newv = _value;
        _lock.unset();

        return newv;
    }

    T operator -=(T v)
    {
        _lock.set();
        _value -= v;
        T newv = _value;
        _lock.unset();

        return newv;
    }

    /* postfix operator */
    T operator ++(int)
    {
        _lock.set();
        T oldv = _value;
        ++_value;
        _lock.unset();
        
        return oldv;
    }

    /* postfix operator */
    T operator --(int)
    {
        _lock.set();
        T oldv = _value;
        --_value;
        _lock.unset();

        return oldv;
    }

private:
    T        _value;
    SpinLock _lock;
};

#endif
}

}
#endif  // EQBASE_ATOMIC_H
