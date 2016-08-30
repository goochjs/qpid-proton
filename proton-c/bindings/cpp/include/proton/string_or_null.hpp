#ifndef PROTON_STRING_OR_NULL_HPP
#define PROTON_STRING_OR_NULL_HPP

/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

#include "./scalar_base.hpp"
#include <string>

namespace proton {

/// An string_or_null can contain a string or be empty() representing a missing property.
class string_or_null : public scalar_base {
  public:
    /// An empty annotation key.
    string_or_null() {}

    /// @name Construct from a string or null
    /// @{
    string_or_null(const std::string& x) { put_(x); }
    string_or_null(const char* x) { put_(x); }
    string_or_null(const null& x) { put_(x); }
    ///@}

    /// @name Assign from a string or null.
    /// @{
    string_or_null& operator=(const std::string& x) { put_(x); return *this; }
    string_or_null& operator=(const char* x) { put_(x); return *this; }
    string_or_null& operator=(const null& x) { put_(x); return *this; }
    /// @}
};

/// @cond INTERNAL
/// Primary template for get<T>(message_id), specialized for legal types.
template <class T> T get(const string_or_null& x);
/// @endcond

/// Get the uint64_t value or throw conversion_error.
/// @related string_or_null
template<> inline std::string get<std::string>(const string_or_null& x) { return internal::get<std::string>(x); }

/// Get the null value or throw conversion_error.
/// @related string_or_null
template<> inline null get<null>(const string_or_null& x) { return internal::get<null>(x); }

/// @copydoc scalar::coerce
/// @related string_or_null
template<class T> T coerce(const string_or_null& x) { return internal::coerce<T>(x); }

} // proton

#endif // PROTON_STRING_OR_NULL_HPP
