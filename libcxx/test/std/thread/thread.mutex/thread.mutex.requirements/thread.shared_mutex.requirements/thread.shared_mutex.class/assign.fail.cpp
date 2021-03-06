//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// UNSUPPORTED: libcpp-has-no-threads
// UNSUPPORTED: c++98, c++03, c++11, c++14

// <shared_mutex>

// class shared_mutex;

// shared_mutex& operator=(const shared_mutex&) = delete;

#include <shared_mutex>

int main()
{
    std::shared_mutex m0;
    std::shared_mutex m1;
    m1 = m0; // expected-error {{overload resolution selected deleted operator '='}}
}
