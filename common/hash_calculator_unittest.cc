//
// Copyright (C) 2009 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "update_engine/common/hash_calculator.h"

#include <math.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <brillo/data_encoding.h>
#include <brillo/secure_blob.h>
#include <gtest/gtest.h>

#include "update_engine/common/test_utils.h"
#include "update_engine/common/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

// Generated by running this on a linux shell:
// $ echo -n hi | openssl dgst -sha256 -binary |
//   hexdump -v -e '"    " 12/1 "0x%02x, " "\n"'
static const uint8_t kExpectedRawHash[] = {
    0x8f, 0x43, 0x43, 0x46, 0x64, 0x8f, 0x6b, 0x96, 0xdf, 0x89, 0xdd,
    0xa9, 0x01, 0xc5, 0x17, 0x6b, 0x10, 0xa6, 0xd8, 0x39, 0x61, 0xdd,
    0x3c, 0x1a, 0xc8, 0x8b, 0x59, 0xb2, 0xdc, 0x32, 0x7a, 0xa4};

class HashCalculatorTest : public ::testing::Test {};

TEST_F(HashCalculatorTest, SimpleTest) {
  HashCalculator calc;
  calc.Update("hi", 2);
  calc.Finalize();
  brillo::Blob raw_hash(std::begin(kExpectedRawHash),
                        std::end(kExpectedRawHash));
  EXPECT_EQ(raw_hash, calc.raw_hash());
}

TEST_F(HashCalculatorTest, MultiUpdateTest) {
  HashCalculator calc;
  calc.Update("h", 1);
  calc.Update("i", 1);
  calc.Finalize();
  brillo::Blob raw_hash(std::begin(kExpectedRawHash),
                        std::end(kExpectedRawHash));
  EXPECT_EQ(raw_hash, calc.raw_hash());
}

TEST_F(HashCalculatorTest, ContextTest) {
  HashCalculator calc;
  calc.Update("h", 1);
  string calc_context = calc.GetContext();
  calc.Finalize();
  HashCalculator calc_next;
  calc_next.SetContext(calc_context);
  calc_next.Update("i", 1);
  calc_next.Finalize();
  brillo::Blob raw_hash(std::begin(kExpectedRawHash),
                        std::end(kExpectedRawHash));
  EXPECT_EQ(raw_hash, calc_next.raw_hash());
}

TEST_F(HashCalculatorTest, BigTest) {
  HashCalculator calc;

  int digit_count = 1;
  int next_overflow = 10;
  for (int i = 0; i < 1000000; i++) {
    char buf[8];
    if (i == next_overflow) {
      next_overflow *= 10;
      digit_count++;
    }
    ASSERT_EQ(digit_count, snprintf(buf, sizeof(buf), "%d", i)) << " i = " << i;
    calc.Update(buf, strlen(buf));
  }
  calc.Finalize();

  // Hash constant generated by running this on a linux shell:
  // $ C=0
  // $ while [ $C -lt 1000000 ]; do
  //     echo -n $C
  //     let C=C+1
  //   done | openssl dgst -sha256 -binary | openssl base64
  EXPECT_EQ("NZf8k6SPBkYMvhaX8YgzuMgbkLP1XZ+neM8K5wcSsf8=",
            brillo::data_encoding::Base64Encode(calc.raw_hash()));
}

TEST_F(HashCalculatorTest, UpdateFileSimpleTest) {
  ScopedTempFile data_file("data.XXXXXX");
  ASSERT_TRUE(test_utils::WriteFileString(data_file.path(), "hi"));

  for (const int length : {-1, 2, 10}) {
    HashCalculator calc;
    EXPECT_EQ(2, calc.UpdateFile(data_file.path(), length));
    EXPECT_TRUE(calc.Finalize());
    brillo::Blob raw_hash(std::begin(kExpectedRawHash),
                          std::end(kExpectedRawHash));
    EXPECT_EQ(raw_hash, calc.raw_hash());
  }

  HashCalculator calc;
  EXPECT_EQ(0, calc.UpdateFile(data_file.path(), 0));
  EXPECT_EQ(1, calc.UpdateFile(data_file.path(), 1));
  EXPECT_TRUE(calc.Finalize());
  // echo -n h | openssl dgst -sha256 -binary | openssl base64
  EXPECT_EQ("qqlAJmTxpB9A67xSyZk+tmrrNmYClY/fqig7ceZNsSM=",
            brillo::data_encoding::Base64Encode(calc.raw_hash()));
}

TEST_F(HashCalculatorTest, RawHashOfFileSimpleTest) {
  ScopedTempFile data_file("data.XXXXXX");
  ASSERT_TRUE(test_utils::WriteFileString(data_file.path(), "hi"));

  for (const int length : {-1, 2, 10}) {
    brillo::Blob exp_raw_hash(std::begin(kExpectedRawHash),
                              std::end(kExpectedRawHash));
    brillo::Blob raw_hash;
    EXPECT_EQ(
        2, HashCalculator::RawHashOfFile(data_file.path(), length, &raw_hash));
    EXPECT_EQ(exp_raw_hash, raw_hash);
  }
}

TEST_F(HashCalculatorTest, UpdateFileNonexistentTest) {
  HashCalculator calc;
  EXPECT_EQ(-1, calc.UpdateFile("/some/non-existent/file", -1));
}

TEST_F(HashCalculatorTest, AbortTest) {
  // Just make sure we don't crash and valgrind doesn't detect memory leaks
  { HashCalculator calc; }
  {
    HashCalculator calc;
    calc.Update("h", 1);
  }
}

}  // namespace chromeos_update_engine
