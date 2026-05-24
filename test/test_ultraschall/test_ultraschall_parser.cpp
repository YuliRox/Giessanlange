#include <unity.h>

#include "A02yyuwFrameParser.h"

void test_parser_rejects_noise_without_header()
{
    A02yyuwFrameParser parser;
    TEST_ASSERT_FALSE(parser.consume(0x00));
    TEST_ASSERT_FALSE(parser.consume(0x12));
    TEST_ASSERT_FALSE(parser.hasValidDistance());
}

void test_parser_accepts_valid_frame()
{
    A02yyuwFrameParser parser;

    const uint8_t high = 0x00;
    const uint8_t low = 0x64; // 100 cm
    const uint8_t checksum = static_cast<uint8_t>(0xFF + high + low);

    TEST_ASSERT_FALSE(parser.consume(0xFF));
    TEST_ASSERT_FALSE(parser.consume(high));
    TEST_ASSERT_FALSE(parser.consume(low));
    TEST_ASSERT_TRUE(parser.consume(checksum));

    TEST_ASSERT_TRUE(parser.hasValidDistance());
    TEST_ASSERT_EQUAL_UINT16(100U, parser.getDistanceCm());
}

void test_parser_rejects_bad_checksum()
{
    A02yyuwFrameParser parser;

    TEST_ASSERT_FALSE(parser.consume(0xFF));
    TEST_ASSERT_FALSE(parser.consume(0x00));
    TEST_ASSERT_FALSE(parser.consume(0x64));
    TEST_ASSERT_FALSE(parser.consume(0x00));

    TEST_ASSERT_FALSE(parser.hasValidDistance());
}

void test_parser_resynchronizes_after_invalid_data()
{
    A02yyuwFrameParser parser;

    TEST_ASSERT_FALSE(parser.consume(0xFF));
    TEST_ASSERT_FALSE(parser.consume(0x01));
    TEST_ASSERT_FALSE(parser.consume(0x02));
    TEST_ASSERT_FALSE(parser.consume(0x00));
    TEST_ASSERT_FALSE(parser.hasValidDistance());

    const uint8_t high = 0x01;
    const uint8_t low = 0x2C; // 300 cm
    const uint8_t checksum = static_cast<uint8_t>(0xFF + high + low);

    TEST_ASSERT_FALSE(parser.consume(0xFF));
    TEST_ASSERT_FALSE(parser.consume(high));
    TEST_ASSERT_FALSE(parser.consume(low));
    TEST_ASSERT_TRUE(parser.consume(checksum));

    TEST_ASSERT_TRUE(parser.hasValidDistance());
    TEST_ASSERT_EQUAL_UINT16(300U, parser.getDistanceCm());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_parser_rejects_noise_without_header);
    RUN_TEST(test_parser_accepts_valid_frame);
    RUN_TEST(test_parser_rejects_bad_checksum);
    RUN_TEST(test_parser_resynchronizes_after_invalid_data);
    return UNITY_END();
}
