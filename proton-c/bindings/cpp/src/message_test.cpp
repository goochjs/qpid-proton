/*
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
 */

#include "proton/message.hpp"
#include "proton/scalar.hpp"
#include "proton/message_id.hpp"
#include "proton/types_fwd.hpp"
#include "test_bits.hpp"
#include <string>
#include <fstream>
#include <streambuf>
#include <iosfwd>

namespace {

using namespace std;
using namespace proton;

// Check default value, set & get property.
#define CHECK_PROP(PROP, DEFAULT, VALUE)       \
    do {                                        \
        ASSERT_EQUAL(DEFAULT, m.PROP());        \
        m.PROP(VALUE);                          \
        ASSERT_EQUAL(VALUE, m.PROP());          \
    } while(0)

void test_message_properties() {
    message m;
    CHECK_PROP(id, message_id(), message_id("id"));
    CHECK_PROP(user, "", "user");
    CHECK_PROP(to, "", "to");
    CHECK_PROP(reply_to, "", "reply_to");
    CHECK_PROP(correlation_id, message_id(), message_id("correlation_id"));
    CHECK_PROP(body, value(), "body");
    CHECK_PROP(subject, "", "subject");
    CHECK_PROP(content_type, "", "content_type");
    CHECK_PROP(content_encoding, "", "content_encoding");
    CHECK_PROP(expiry_time, timestamp(), timestamp(42));
    CHECK_PROP(creation_time, timestamp(), timestamp(4242));
    CHECK_PROP(durable, false, true);
    CHECK_PROP(inferred, false, true);
    CHECK_PROP(ttl, duration(), duration(30));
    CHECK_PROP(priority, message::default_priority, 17);
    CHECK_PROP(first_acquirer, false, true);
    CHECK_PROP(delivery_count, 0u, 33u);
    CHECK_PROP(group_id, "", "group_id");
    CHECK_PROP(reply_to_group_id, "", "reply_to_group_id");
    CHECK_PROP(group_sequence, 0, 12);

    message m2(m);
    ASSERT_EQUAL(message_id("id"), m2.id());
    ASSERT_EQUAL("user", m2.user());
    ASSERT_EQUAL("to", m2.to());
    ASSERT_EQUAL("reply_to", m2.reply_to());
    ASSERT_EQUAL(message_id("correlation_id"), m2.correlation_id());
    ASSERT_EQUAL(value("body"), m2.body());
    ASSERT_EQUAL("subject", m2.subject());
    ASSERT_EQUAL("content_type", m2.content_type());
    ASSERT_EQUAL("content_encoding", m2.content_encoding());
    ASSERT_EQUAL(42, m2.expiry_time().milliseconds());
    ASSERT_EQUAL(4242, m2.creation_time().milliseconds());
    ASSERT_EQUAL(true, m2.durable());
    ASSERT_EQUAL(true, m2.inferred());
    ASSERT_EQUAL(duration(30), m2.ttl());
    ASSERT_EQUAL(17, m2.priority());
    ASSERT_EQUAL(true, m2.first_acquirer());
    ASSERT_EQUAL(33u, m2.delivery_count());
    ASSERT_EQUAL("group_id", m2.group_id());
    ASSERT_EQUAL("reply_to_group_id", m2.reply_to_group_id());
    ASSERT_EQUAL(12, m2.group_sequence());
}

void test_string_or_null(
    const std::string& name,
    std::string  (message::*getter)() const,
    void (message::*setter)(const std::string&),
    string_or_null (message::*value_getter)() const,
    void (message::*value_setter)(const string_or_null&))
{
    message m;
    string_or_null sn = (m.*value_getter)();
    ASSERT((m.*value_getter)().empty());

    (m.*setter)(name);
    ASSERT_EQUAL(name, (m.*getter)());
    ASSERT_EQUAL(name, get<std::string>((m.*value_getter)()));

    (m.*value_setter)(name);
    ASSERT_EQUAL(name, (m.*getter)());
    ASSERT_EQUAL(name, get<std::string>((m.*value_getter)()));

    (m.*value_setter)(null());  // Reset to null
    ASSERT((m.*value_getter)().empty());
    message m2(m);              // Ensure it copies as null
    ASSERT((m2.*getter)().empty());
    FAIL("Test with all set, knock out one by one");
}

#define TEST_STRING_OR_NULL(PROPERTY)                                   \
    test_string_or_null(#PROPERTY,                                      \
                        &message::PROPERTY, &message::PROPERTY,         \
                        &message::PROPERTY##_value, &message::PROPERTY##_value)

void test_message_body() {
    std::string s("hello");
    message m1(s.c_str());
    ASSERT_EQUAL(s, get<std::string>(m1.body()));
    message m2(s);
    ASSERT_EQUAL(s, coerce<std::string>(m2.body()));
    message m3;
    m3.body(s);
    ASSERT_EQUAL(s, coerce<std::string>(m3.body()));
    ASSERT_EQUAL(5, coerce<int64_t>(message(5).body()));
    ASSERT_EQUAL(3.1, coerce<double>(message(3.1).body()));
}

void test_message_maps() {
    message m;

    ASSERT(m.properties().empty());
    ASSERT(m.message_annotations().empty());
    ASSERT(m.delivery_annotations().empty());

    m.properties().put("foo", 12);
    m.delivery_annotations().put("bar", "xyz");

    m.message_annotations().put(23, "23");
    ASSERT_EQUAL(m.properties().get("foo"), scalar(12));
    ASSERT_EQUAL(m.delivery_annotations().get("bar"), scalar("xyz"));
    ASSERT_EQUAL(m.message_annotations().get(23), scalar("23"));

    message m2(m);

    ASSERT_EQUAL(m2.properties().get("foo"), scalar(12));
    ASSERT_EQUAL(m2.delivery_annotations().get("bar"), scalar("xyz"));
    ASSERT_EQUAL(m2.message_annotations().get(23), scalar("23"));

    m.properties().put("foo","newfoo");
    m.delivery_annotations().put(24, 1000);
    m.message_annotations().erase(23);

    m2 = m;
    ASSERT_EQUAL(1u, m2.properties().size());
    ASSERT_EQUAL(m2.properties().get("foo"), scalar("newfoo"));
    ASSERT_EQUAL(2u, m2.delivery_annotations().size());
    ASSERT_EQUAL(m2.delivery_annotations().get("bar"), scalar("xyz"));
    ASSERT_EQUAL(m2.delivery_annotations().get(24), scalar(1000));
    ASSERT(m2.message_annotations().empty());
}

}

int main(int, char**) {
    int failed = 0;
    RUN_TEST(failed, test_message_properties());
    RUN_TEST(failed, test_message_body());
    RUN_TEST(failed, test_message_maps());
    // FIXME aconway 2016-08-30:
    // Nullable string tests
    RUN_TEST(failed, TEST_STRING_OR_NULL(reply_to));
    return failed;
}
