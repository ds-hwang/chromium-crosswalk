// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/inspector_protocol/Parser.h"

#include "platform/inspector_protocol/Values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/StringBuilder.h"

namespace blink {
namespace protocol {

TEST(ParserTest, Reading)
{
    RefPtr<protocol::Value> tmpValue;
    RefPtr<protocol::Value> root;
    RefPtr<protocol::Value> root2;
    String strVal;
    int intVal = 0;

    // some whitespace checking
    root = parseJSON("    null    ");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNull, root->type());

    // Invalid JSON string
    root = parseJSON("nu");
    EXPECT_FALSE(root.get());

    // Simple bool
    root = parseJSON("true  ");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeBoolean, root->type());

    // Embedded comment
    root = parseJSON("40 /*/");
    EXPECT_FALSE(root.get());
    root = parseJSON("/* comment */null");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNull, root->type());
    root = parseJSON("40 /* comment */");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    EXPECT_TRUE(root->asNumber(&intVal));
    EXPECT_EQ(40, intVal);
    root = parseJSON("/**/ 40 /* multi-line\n comment */ // more comment");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    EXPECT_TRUE(root->asNumber(&intVal));
    EXPECT_EQ(40, intVal);
    root = parseJSON("true // comment");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeBoolean, root->type());
    root = parseJSON("/* comment */\"sample string\"");
    ASSERT_TRUE(root.get());
    EXPECT_TRUE(root->asString(&strVal));
    EXPECT_EQ("sample string", strVal);
    root = parseJSON("[1, /* comment, 2 ] */ \n 3]");
    ASSERT_TRUE(root.get());
    RefPtr<protocol::ListValue> list = ListValue::cast(root);
    ASSERT_TRUE(list);
    EXPECT_EQ(2u, list->length());
    tmpValue = list->get(0);
    ASSERT_TRUE(tmpValue.get());
    EXPECT_TRUE(tmpValue->asNumber(&intVal));
    EXPECT_EQ(1, intVal);
    tmpValue = list->get(1);
    ASSERT_TRUE(tmpValue.get());
    EXPECT_TRUE(tmpValue->asNumber(&intVal));
    EXPECT_EQ(3, intVal);
    root = parseJSON("[1, /*a*/2, 3]");
    ASSERT_TRUE(root.get());
    list = ListValue::cast(root);
    ASSERT_TRUE(list);
    EXPECT_EQ(3u, list->length());
    root = parseJSON("/* comment **/42");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    EXPECT_TRUE(root->asNumber(&intVal));
    EXPECT_EQ(42, intVal);
    root = parseJSON(
        "/* comment **/\n"
        "// */ 43\n"
        "44");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    EXPECT_TRUE(root->asNumber(&intVal));
    EXPECT_EQ(44, intVal);

    // Test number formats
    root = parseJSON("43");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    EXPECT_TRUE(root->asNumber(&intVal));
    EXPECT_EQ(43, intVal);

    // According to RFC4627, oct, hex, and leading zeros are invalid JSON.
    root = parseJSON("043");
    EXPECT_FALSE(root.get());
    root = parseJSON("0x43");
    EXPECT_FALSE(root.get());
    root = parseJSON("00");
    EXPECT_FALSE(root.get());

    // Test 0 (which needs to be special cased because of the leading zero
    // clause).
    root = parseJSON("0");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    intVal = 1;
    EXPECT_TRUE(root->asNumber(&intVal));
    EXPECT_EQ(0, intVal);

    // Numbers that overflow ints should succeed, being internally promoted to
    // storage as doubles
    root = parseJSON("2147483648");
    ASSERT_TRUE(root.get());
    double doubleVal;
    EXPECT_EQ(Value::TypeNumber, root->type());
    doubleVal = 0.0;
    EXPECT_TRUE(root->asNumber(&doubleVal));
    EXPECT_DOUBLE_EQ(2147483648.0, doubleVal);
    root = parseJSON("-2147483649");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    doubleVal = 0.0;
    EXPECT_TRUE(root->asNumber(&doubleVal));
    EXPECT_DOUBLE_EQ(-2147483649.0, doubleVal);

    // Parse a double
    root = parseJSON("43.1");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    doubleVal = 0.0;
    EXPECT_TRUE(root->asNumber(&doubleVal));
    EXPECT_DOUBLE_EQ(43.1, doubleVal);

    root = parseJSON("4.3e-1");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    doubleVal = 0.0;
    EXPECT_TRUE(root->asNumber(&doubleVal));
    EXPECT_DOUBLE_EQ(.43, doubleVal);

    root = parseJSON("2.1e0");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    doubleVal = 0.0;
    EXPECT_TRUE(root->asNumber(&doubleVal));
    EXPECT_DOUBLE_EQ(2.1, doubleVal);

    root = parseJSON("2.1e+0001");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    doubleVal = 0.0;
    EXPECT_TRUE(root->asNumber(&doubleVal));
    EXPECT_DOUBLE_EQ(21.0, doubleVal);

    root = parseJSON("0.01");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    doubleVal = 0.0;
    EXPECT_TRUE(root->asNumber(&doubleVal));
    EXPECT_DOUBLE_EQ(0.01, doubleVal);

    root = parseJSON("1.00");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    doubleVal = 0.0;
    EXPECT_TRUE(root->asNumber(&doubleVal));
    EXPECT_DOUBLE_EQ(1.0, doubleVal);

    // Fractional parts must have a digit before and after the decimal point.
    root = parseJSON("1.");
    EXPECT_FALSE(root.get());
    root = parseJSON(".1");
    EXPECT_FALSE(root.get());
    root = parseJSON("1.e10");
    EXPECT_FALSE(root.get());

    // Exponent must have a digit following the 'e'.
    root = parseJSON("1e");
    EXPECT_FALSE(root.get());
    root = parseJSON("1E");
    EXPECT_FALSE(root.get());
    root = parseJSON("1e1.");
    EXPECT_FALSE(root.get());
    root = parseJSON("1e1.0");
    EXPECT_FALSE(root.get());

    // INF/-INF/NaN are not valid
    root = parseJSON("1e1000");
    EXPECT_FALSE(root.get());
    root = parseJSON("-1e1000");
    EXPECT_FALSE(root.get());
    root = parseJSON("NaN");
    EXPECT_FALSE(root.get());
    root = parseJSON("nan");
    EXPECT_FALSE(root.get());
    root = parseJSON("inf");
    EXPECT_FALSE(root.get());

    // Invalid number formats
    root = parseJSON("4.3.1");
    EXPECT_FALSE(root.get());
    root = parseJSON("4e3.1");
    EXPECT_FALSE(root.get());

    // Test string parser
    root = parseJSON("\"hello world\"");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeString, root->type());
    EXPECT_TRUE(root->asString(&strVal));
    EXPECT_EQ("hello world", strVal);

    // Empty string
    root = parseJSON("\"\"");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeString, root->type());
    EXPECT_TRUE(root->asString(&strVal));
    EXPECT_EQ("", strVal);

    // Test basic string escapes
    root = parseJSON("\" \\\"\\\\\\/\\b\\f\\n\\r\\t\\v\"");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeString, root->type());
    EXPECT_TRUE(root->asString(&strVal));
    EXPECT_EQ(" \"\\/\b\f\n\r\t\v", strVal);

    // Test hex and unicode escapes including the null character.
    root = parseJSON("\"\\x41\\x00\\u1234\"");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeString, root->type());
    EXPECT_TRUE(root->asString(&strVal));
    UChar tmp1[] = {0x41, 0, 0x1234};
    EXPECT_EQ(String(tmp1, WTF_ARRAY_LENGTH(tmp1)), strVal);

    // Test invalid strings
    root = parseJSON("\"no closing quote");
    EXPECT_FALSE(root.get());
    root = parseJSON("\"\\z invalid escape char\"");
    EXPECT_FALSE(root.get());
    root = parseJSON("\"\\xAQ invalid hex code\"");
    EXPECT_FALSE(root.get());
    root = parseJSON("not enough hex chars\\x1\"");
    EXPECT_FALSE(root.get());
    root = parseJSON("\"not enough escape chars\\u123\"");
    EXPECT_FALSE(root.get());
    root = parseJSON("\"extra backslash at end of input\\\"");
    EXPECT_FALSE(root.get());

    // Basic array
    root = parseJSON("[true, false, null]");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeArray, root->type());
    list = ListValue::cast(root);
    ASSERT_TRUE(list);
    EXPECT_EQ(3U, list->length());

    // Empty array
    root = parseJSON("[]");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeArray, root->type());
    list = ListValue::cast(root);
    ASSERT_TRUE(list);
    EXPECT_EQ(0U, list->length());

    // Nested arrays
    root = parseJSON("[[true], [], [false, [], [null]], null]");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeArray, root->type());
    list = ListValue::cast(root);
    ASSERT_TRUE(list);
    EXPECT_EQ(4U, list->length());

    // Invalid, missing close brace.
    root = parseJSON("[[true], [], [false, [], [null]], null");
    EXPECT_FALSE(root.get());

    // Invalid, too many commas
    root = parseJSON("[true,, null]");
    EXPECT_FALSE(root.get());

    // Invalid, no commas
    root = parseJSON("[true null]");
    EXPECT_FALSE(root.get());

    // Invalid, trailing comma
    root = parseJSON("[true,]");
    EXPECT_FALSE(root.get());

    root = parseJSON("[true]");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeArray, root->type());
    list = ListValue::cast(root);
    ASSERT_TRUE(list);
    EXPECT_EQ(1U, list->length());
    tmpValue = list->get(0);
    ASSERT_TRUE(tmpValue.get());
    EXPECT_EQ(Value::TypeBoolean, tmpValue->type());
    bool boolValue = false;
    EXPECT_TRUE(tmpValue->asBoolean(&boolValue));
    EXPECT_TRUE(boolValue);

    // Don't allow empty elements.
    root = parseJSON("[,]");
    EXPECT_FALSE(root.get());
    root = parseJSON("[true,,]");
    EXPECT_FALSE(root.get());
    root = parseJSON("[,true,]");
    EXPECT_FALSE(root.get());
    root = parseJSON("[true,,false]");
    EXPECT_FALSE(root.get());

    // Test objects
    root = parseJSON("{}");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeObject, root->type());

    root = parseJSON("{\"number\":9.87654321, \"null\":null , \"\\x53\" : \"str\" }");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeObject, root->type());
    RefPtr<protocol::DictionaryValue> objectVal = DictionaryValue::cast(root);
    ASSERT_TRUE(objectVal);
    doubleVal = 0.0;
    EXPECT_TRUE(objectVal->getNumber("number", &doubleVal));
    EXPECT_DOUBLE_EQ(9.87654321, doubleVal);
    RefPtr<protocol::Value> nullVal = objectVal->get("null");
    ASSERT_TRUE(nullVal.get());
    EXPECT_EQ(Value::TypeNull, nullVal->type());
    EXPECT_TRUE(objectVal->getString("S", &strVal));
    EXPECT_EQ("str", strVal);

    // Test newline equivalence.
    root2 = parseJSON(
        "{\n"
        "  \"number\":9.87654321,\n"
        "  \"null\":null,\n"
        "  \"\\x53\":\"str\"\n"
        "}\n");
    ASSERT_TRUE(root2.get());
    EXPECT_EQ(root->toJSONString(), root2->toJSONString());

    root2 = parseJSON(
        "{\r\n"
        "  \"number\":9.87654321,\r\n"
        "  \"null\":null,\r\n"
        "  \"\\x53\":\"str\"\r\n"
        "}\r\n");
    ASSERT_TRUE(root2.get());
    EXPECT_EQ(root->toJSONString(), root2->toJSONString());

    // Test nesting
    root = parseJSON("{\"inner\":{\"array\":[true]},\"false\":false,\"d\":{}}");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeObject, root->type());
    objectVal = DictionaryValue::cast(root);
    ASSERT_TRUE(objectVal);
    RefPtr<protocol::DictionaryValue> innerObject = objectVal->getObject("inner");
    ASSERT_TRUE(innerObject.get());
    RefPtr<protocol::ListValue> innerArray = innerObject->getArray("array");
    ASSERT_TRUE(innerArray.get());
    EXPECT_EQ(1U, innerArray->length());
    boolValue = true;
    EXPECT_TRUE(objectVal->getBoolean("false", &boolValue));
    EXPECT_FALSE(boolValue);
    innerObject = objectVal->getObject("d");
    EXPECT_TRUE(innerObject.get());

    // Test keys with periods
    root = parseJSON("{\"a.b\":3,\"c\":2,\"d.e.f\":{\"g.h.i.j\":1}}");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeObject, root->type());
    objectVal = DictionaryValue::cast(root);
    ASSERT_TRUE(objectVal);
    int integerValue = 0;
    EXPECT_TRUE(objectVal->getNumber("a.b", &integerValue));
    EXPECT_EQ(3, integerValue);
    EXPECT_TRUE(objectVal->getNumber("c", &integerValue));
    EXPECT_EQ(2, integerValue);
    innerObject = objectVal->getObject("d.e.f");
    ASSERT_TRUE(innerObject.get());
    EXPECT_EQ(1, innerObject->size());
    EXPECT_TRUE(innerObject->getNumber("g.h.i.j", &integerValue));
    EXPECT_EQ(1, integerValue);

    root = parseJSON("{\"a\":{\"b\":2},\"a.b\":1}");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeObject, root->type());
    objectVal = DictionaryValue::cast(root);
    ASSERT_TRUE(objectVal);
    innerObject = objectVal->getObject("a");
    ASSERT_TRUE(innerObject.get());
    EXPECT_TRUE(innerObject->getNumber("b", &integerValue));
    EXPECT_EQ(2, integerValue);
    EXPECT_TRUE(objectVal->getNumber("a.b", &integerValue));
    EXPECT_EQ(1, integerValue);

    // Invalid, no closing brace
    root = parseJSON("{\"a\": true");
    EXPECT_FALSE(root.get());

    // Invalid, keys must be quoted
    root = parseJSON("{foo:true}");
    EXPECT_FALSE(root.get());

    // Invalid, trailing comma
    root = parseJSON("{\"a\":true,}");
    EXPECT_FALSE(root.get());

    // Invalid, too many commas
    root = parseJSON("{\"a\":true,,\"b\":false}");
    EXPECT_FALSE(root.get());

    // Invalid, no separator
    root = parseJSON("{\"a\" \"b\"}");
    EXPECT_FALSE(root.get());

    // Invalid, lone comma.
    root = parseJSON("{,}");
    EXPECT_FALSE(root.get());
    root = parseJSON("{\"a\":true,,}");
    EXPECT_FALSE(root.get());
    root = parseJSON("{,\"a\":true}");
    EXPECT_FALSE(root.get());
    root = parseJSON("{\"a\":true,,\"b\":false}");
    EXPECT_FALSE(root.get());

    // Test stack overflow
    StringBuilder evil;
    evil.reserveCapacity(2000000);
    for (int i = 0; i < 1000000; ++i)
        evil.append('[');
    for (int i = 0; i < 1000000; ++i)
        evil.append(']');
    root = parseJSON(evil.toString());
    EXPECT_FALSE(root.get());

    // A few thousand adjacent lists is fine.
    StringBuilder notEvil;
    notEvil.reserveCapacity(15010);
    notEvil.append('[');
    for (int i = 0; i < 5000; ++i)
        notEvil.append("[],");
    notEvil.append("[]]");
    root = parseJSON(notEvil.toString());
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeArray, root->type());
    list = ListValue::cast(root);
    ASSERT_TRUE(list);
    EXPECT_EQ(5001U, list->length());

    // Test utf8 encoded input
    root = parseJSON("\"\\xe7\\xbd\\x91\\xe9\\xa1\\xb5\"");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeString, root->type());
    EXPECT_TRUE(root->asString(&strVal));
    UChar tmp4[] = {0x7f51, 0x9875};
    EXPECT_EQ(String(tmp4, WTF_ARRAY_LENGTH(tmp4)), strVal);

    root = parseJSON("{\"path\": \"/tmp/\\xc3\\xa0\\xc3\\xa8\\xc3\\xb2.png\"}");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeObject, root->type());
    objectVal = DictionaryValue::cast(root);
    ASSERT_TRUE(objectVal);
    EXPECT_TRUE(objectVal->getString("path", &strVal));
    UChar tmp5[] = {0x2f, 0x74, 0x6d, 0x70, 0x2f, 0xe0, 0xe8, 0xf2, 0x2e, 0x70, 0x6e, 0x67};
    EXPECT_EQ(String(tmp5, WTF_ARRAY_LENGTH(tmp5)), strVal);

    // Test invalid utf8 encoded input
    root = parseJSON("\"345\\xb0\\xa1\\xb0\\xa2\"");
    ASSERT_FALSE(root.get());
    root = parseJSON("\"123\\xc0\\x81\"");
    ASSERT_FALSE(root.get());
    root = parseJSON("\"abc\\xc0\\xae\"");
    ASSERT_FALSE(root.get());

    // Test utf16 encoded strings.
    root = parseJSON("\"\\u20ac3,14\"");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeString, root->type());
    EXPECT_TRUE(root->asString(&strVal));
    UChar tmp2[] = {0x20ac, 0x33, 0x2c, 0x31, 0x34};
    EXPECT_EQ(String(tmp2, WTF_ARRAY_LENGTH(tmp2)), strVal);

    root = parseJSON("\"\\ud83d\\udca9\\ud83d\\udc6c\"");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeString, root->type());
    EXPECT_TRUE(root->asString(&strVal));
    UChar tmp3[] = {0xd83d, 0xdca9, 0xd83d, 0xdc6c};
    EXPECT_EQ(String(tmp3, WTF_ARRAY_LENGTH(tmp3)), strVal);

    // Test invalid utf16 strings.
    const char* const cases[] = {
        "\"\\u123\"", // Invalid scalar.
        "\"\\ud83d\"", // Invalid scalar.
        "\"\\u$%@!\"", // Invalid scalar.
        "\"\\uzz89\"", // Invalid scalar.
        "\"\\ud83d\\udca\"", // Invalid lower surrogate.
        "\"\\ud83d\\ud83d\"", // Invalid lower surrogate.
        "\"\\ud83foo\"", // No lower surrogate.
        "\"\\ud83\\foo\"" // No lower surrogate.
    };
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(cases); ++i) {
        root = parseJSON(cases[i]);
        EXPECT_FALSE(root.get()) << cases[i];
    }

    // Test literal root objects.
    root = parseJSON("null");
    EXPECT_EQ(Value::TypeNull, root->type());

    root = parseJSON("true");
    ASSERT_TRUE(root.get());
    EXPECT_TRUE(root->asBoolean(&boolValue));
    EXPECT_TRUE(boolValue);

    root = parseJSON("10");
    ASSERT_TRUE(root.get());
    EXPECT_TRUE(root->asNumber(&integerValue));
    EXPECT_EQ(10, integerValue);

    root = parseJSON("\"root\"");
    ASSERT_TRUE(root.get());
    EXPECT_TRUE(root->asString(&strVal));
    EXPECT_EQ("root", strVal);
}

TEST(ParserTest, InvalidSanity)
{
    const char* const invalidJson[] = {
        "/* test *",
        "{\"foo\"",
        "{\"foo\":",
        "  [",
        "\"\\u123g\"",
        "{\n\"eh:\n}",
        "////",
        "*/**/",
        "/**/",
        "/*/",
        "//**/"
    };

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(invalidJson); ++i) {
        RefPtr<protocol::Value> result = parseJSON(invalidJson[i]);
        EXPECT_FALSE(result.get());
    }
}

} // namespace protocol
} // namespace blink
