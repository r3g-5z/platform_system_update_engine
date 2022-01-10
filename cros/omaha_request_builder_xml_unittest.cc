//
// Copyright (C) 2019 The Android Open Source Project
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

#include "update_engine/cros/omaha_request_builder_xml.h"

#include <string>
#include <utility>
#include <vector>

#include <base/guid.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <gtest/gtest.h>

#include "update_engine/common/mock_cros_healthd.h"
#include "update_engine/cros/fake_system_state.h"

using std::pair;
using std::string;
using std::vector;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

namespace chromeos_update_engine {

namespace {
// Helper to find key and extract value from the given string |xml|, instead
// of using a full parser. The attribute key will be followed by "=\"" as xml
// attribute values must be within double quotes (not single quotes).
static string FindAttributeKeyValueInXml(const string& xml,
                                         const string& key,
                                         const size_t val_size) {
  string key_with_quotes = key + "=\"";
  const size_t val_start_pos = xml.find(key);
  if (val_start_pos == string::npos)
    return "";
  return xml.substr(val_start_pos + key_with_quotes.size(), val_size);
}
// Helper to find the count of substring in a string.
static size_t CountSubstringInString(const string& str, const string& substr) {
  size_t count = 0, pos = 0;
  while ((pos = str.find(substr, pos ? pos + 1 : 0)) != string::npos)
    ++count;
  return count;
}
}  // namespace

class OmahaRequestBuilderXmlTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FakeSystemState::CreateInstance();
    params_.set_hw_details(false);
    FakeSystemState::Get()->set_request_params(&params_);

    ON_CALL(*FakeSystemState::Get()->mock_update_attempter(),
            IsRepeatedUpdatesEnabled())
        .WillByDefault(Return(true));
  }
  void TearDown() override {}

  static constexpr size_t kGuidSize = 36;

  OmahaRequestParams params_;
};

TEST_F(OmahaRequestBuilderXmlTest, XmlEncodeTest) {
  string output;
  vector<pair<string, string>> xml_encode_pairs = {
      {"ab", "ab"},
      {"a<b", "a&lt;b"},
      {"<&>\"\'\\", "&lt;&amp;&gt;&quot;&apos;\\"},
      {"&lt;&amp;&gt;", "&amp;lt;&amp;amp;&amp;gt;"}};
  for (const auto& xml_encode_pair : xml_encode_pairs) {
    const auto& before_encoding = xml_encode_pair.first;
    const auto& after_encoding = xml_encode_pair.second;
    EXPECT_TRUE(XmlEncode(before_encoding, &output));
    EXPECT_EQ(after_encoding, output);
  }
  // Check that unterminated UTF-8 strings are handled properly.
  EXPECT_FALSE(XmlEncode("\xc2", &output));
  // Fail with invalid ASCII-7 chars.
  EXPECT_FALSE(XmlEncode("This is an 'n' with a tilde: \xc3\xb1", &output));
}

TEST_F(OmahaRequestBuilderXmlTest, XmlEncodeWithDefaultTest) {
  EXPECT_EQ("", XmlEncodeWithDefault(""));
  EXPECT_EQ("&lt;&amp;&gt;", XmlEncodeWithDefault("<&>", "something else"));
  EXPECT_EQ("<not escaped>", XmlEncodeWithDefault("\xc2", "<not escaped>"));
}

TEST_F(OmahaRequestBuilderXmlTest, PlatformGetAppTest) {
  params_.set_device_requisition("device requisition");
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       ""};
  OmahaAppData dlc_app_data = {.id = "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX",
                               .version = "",
                               .skip_update = false,
                               .is_dlc = false};

  // Verify that the attributes that shouldn't be missing for Platform AppID are
  // in fact present in the <app ...></app>.
  const string app = omaha_request.GetApp(dlc_app_data);
  EXPECT_NE(string::npos, app.find("requisition="));
}

TEST_F(OmahaRequestBuilderXmlTest, GetLastFpTest) {
  params_.set_device_requisition("device requisition");
  params_.set_last_fp("1.75");
  FakeSystemState::Get()->update_attempter()->ChangeRepeatedUpdates(true);
  OmahaRequestBuilderXml omaha_request{nullptr, false, false, 0, 0, 0, ""};
  OmahaAppData dlc_app_data = {.id = "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX",
                               .version = "",
                               .skip_update = false,
                               .is_dlc = false};

  // Verify that the attributes that shouldn't be missing for Platform AppID are
  // in fact present in the <app ...></app>.
  const string app = omaha_request.GetApp(dlc_app_data);
  EXPECT_NE(string::npos, app.find("last_fp=\"1.75\""));
}

TEST_F(OmahaRequestBuilderXmlTest, DlcGetAppTest) {
  params_.set_device_requisition("device requisition");
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       ""};
  OmahaAppData dlc_app_data = {
      .id = "_dlc_id", .version = "", .skip_update = false, .is_dlc = true};

  // Verify that the attributes that should be missing for DLC AppIDs are in
  // fact not present in the <app ...></app>.
  const string app = omaha_request.GetApp(dlc_app_data);
  EXPECT_EQ(string::npos, app.find("requisition="));
}

TEST_F(OmahaRequestBuilderXmlTest, GetNotRunningMiniOS) {
  OmahaRequestBuilderXml omaha_request{nullptr, false, false, 0, 0, 0, ""};
  const string request_xml = omaha_request.GetRequest();
  const string isminios =
      FindAttributeKeyValueInXml(request_xml, "isminios", 1);
  EXPECT_TRUE(isminios.empty());
}

TEST_F(OmahaRequestBuilderXmlTest, GetRunningMiniOS) {
  FakeSystemState::Get()->fake_hardware()->SetIsRunningFromMiniOs(true);
  OmahaRequestBuilderXml omaha_request{nullptr, false, false, 0, 0, 0, ""};
  const string request_xml = omaha_request.GetRequest();
  const string isminios =
      FindAttributeKeyValueInXml(request_xml, "isminios", 1);
  EXPECT_EQ("1", isminios);
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlRequestIdTest) {
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  const string key = "requestid";
  const string request_id =
      FindAttributeKeyValueInXml(request_xml, key, kGuidSize);
  // A valid |request_id| is either a GUID version 4 or empty string.
  if (!request_id.empty())
    EXPECT_TRUE(base::IsValidGUID(request_id));
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlSessionIdTest) {
  const string gen_session_id = base::GenerateGUID();
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       gen_session_id};
  const string request_xml = omaha_request.GetRequest();
  const string key = "sessionid";
  const string session_id =
      FindAttributeKeyValueInXml(request_xml, key, kGuidSize);
  // A valid |session_id| is either a GUID version 4 or empty string.
  if (!session_id.empty()) {
    EXPECT_TRUE(base::IsValidGUID(session_id));
  }
  EXPECT_EQ(gen_session_id, session_id);
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlPlatformUpdateTest) {
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(1, CountSubstringInString(request_xml, "<updatecheck"))
      << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlPlatformUpdateWithDlcsTest) {
  params_.set_dlc_apps_params(
      {{params_.GetDlcAppId("dlc_no_0"), {.name = "dlc_no_0"}},
       {params_.GetDlcAppId("dlc_no_1"), {.name = "dlc_no_1"}}});
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(3, CountSubstringInString(request_xml, "<updatecheck"))
      << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlDlcInstallationTest) {
  const std::map<std::string, OmahaRequestParams::AppParams> dlcs = {
      {params_.GetDlcAppId("dlc_no_0"), {.name = "dlc_no_0"}},
      {params_.GetDlcAppId("dlc_no_1"), {.name = "dlc_no_1"}}};
  params_.set_dlc_apps_params(dlcs);
  params_.set_is_install(true);
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(2, CountSubstringInString(request_xml, "<updatecheck"))
      << request_xml;

  auto FindAppId = [request_xml](size_t pos) -> size_t {
    return request_xml.find("<app appid", pos);
  };
  // Skip over the Platform AppID, which is always first.
  size_t pos = FindAppId(0);
  for (auto&& _ : dlcs) {
    (void)_;
    EXPECT_NE(string::npos, (pos = FindAppId(pos + 1))) << request_xml;
    const string dlc_app_id_version = FindAttributeKeyValueInXml(
        request_xml.substr(pos), "version", string(kNoVersion).size());
    EXPECT_EQ(kNoVersion, dlc_app_id_version);

    const string false_str = "false";
    const string dlc_app_id_delta_okay = FindAttributeKeyValueInXml(
        request_xml.substr(pos), "delta_okay", false_str.length());
    EXPECT_EQ(false_str, dlc_app_id_delta_okay);
  }
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlMiniOsTestForInstallations) {
  FakeSystemState::Get()->fake_boot_control()->SetSupportsMiniOSPartitions(
      true);
  params_.set_is_install(true);
  params_.set_minios_app_params({});
  OmahaRequestBuilderXml omaha_request{nullptr, false, false, 0, 0, 0, ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(0, CountSubstringInString(request_xml, "<updatecheck"))
      << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlMiniOsTest) {
  FakeSystemState::Get()->fake_boot_control()->SetSupportsMiniOSPartitions(
      true);
  params_.set_minios_app_params({});
  OmahaRequestBuilderXml omaha_request{nullptr, false, false, 0, 0, 0, ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(2, CountSubstringInString(request_xml, "<updatecheck"))
      << request_xml;

  auto FindAppId = [request_xml](size_t pos) -> size_t {
    return request_xml.find("<app appid=\"_minios\"", pos);
  };

  EXPECT_EQ(FindAppId(1), FindAppId(0));
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlDlcNoPing) {
  params_.set_dlc_apps_params(
      {{params_.GetDlcAppId("dlc_no_0"), {.name = "dlc_no_0"}}});
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(0, CountSubstringInString(request_xml, "<ping")) << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlDlcPingRollCallNoActive) {
  params_.set_dlc_apps_params(
      {{params_.GetDlcAppId("dlc_no_0"),
        {.active_counting_type = OmahaRequestParams::kDateBased,
         .name = "dlc_no_0",
         .ping_date_last_active = 25,
         .ping_date_last_rollcall = 36,
         .send_ping = true}}});
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(1, CountSubstringInString(request_xml, "<ping rd=\"36\""))
      << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlDlcPingRollCallAndActive) {
  params_.set_dlc_apps_params(
      {{params_.GetDlcAppId("dlc_no_0"),
        {.active_counting_type = OmahaRequestParams::kDateBased,
         .name = "dlc_no_0",
         .ping_active = 1,
         .ping_date_last_active = 25,
         .ping_date_last_rollcall = 36,
         .send_ping = true}}});
  OmahaRequestBuilderXml omaha_request{nullptr,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(1,
            CountSubstringInString(request_xml,
                                   "<ping active=\"1\" ad=\"25\" rd=\"36\""))
      << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlDlcFp) {
  FakeSystemState::Get()->update_attempter()->ChangeRepeatedUpdates(true);
  params_.set_dlc_apps_params({{params_.GetDlcAppId("dlc_no_0"),
                                {.name = "dlc_no_0", .last_fp = "1.1"}}});
  OmahaRequestBuilderXml omaha_request{nullptr, false, false, 0, 0, 0, ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(1, CountSubstringInString(request_xml, "last_fp=\"1.1\""))
      << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlMiniOSFp) {
  FakeSystemState::Get()->update_attempter()->ChangeRepeatedUpdates(true);
  FakeSystemState::Get()->fake_boot_control()->SetSupportsMiniOSPartitions(
      true);
  params_.set_minios_app_params({.last_fp = "1.2"});

  OmahaRequestBuilderXml omaha_request{nullptr, false, false, 0, 0, 0, ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(1, CountSubstringInString(request_xml, "last_fp=\"1.2\""))
      << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlUpdateCompleteEvent) {
  OmahaEvent event(OmahaEvent::kTypeUpdateComplete);
  OmahaRequestBuilderXml omaha_request{&event,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  LOG(INFO) << request_xml;
  EXPECT_EQ(
      1,
      CountSubstringInString(
          request_xml, "<event eventtype=\"3\" eventresult=\"1\"></event>"))
      << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest,
       GetRequestXmlUpdateCompleteEventSomeDlcsExcluded) {
  params_.set_dlc_apps_params({
      {params_.GetDlcAppId("dlc_1"), {.updated = true}},
      {params_.GetDlcAppId("dlc_2"), {.updated = false}},
  });
  OmahaEvent event(OmahaEvent::kTypeUpdateComplete);
  OmahaRequestBuilderXml omaha_request{&event,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(
      2,
      CountSubstringInString(
          request_xml, "<event eventtype=\"3\" eventresult=\"1\"></event>"))
      << request_xml;
  EXPECT_EQ(
      1,
      CountSubstringInString(
          request_xml,
          "<event eventtype=\"3\" eventresult=\"0\" errorcode=\"62\"></event>"))
      << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest,
       GetRequestXmlUpdateCompleteEventAllDlcsExcluded) {
  params_.set_dlc_apps_params({
      {params_.GetDlcAppId("dlc_1"), {.updated = false}},
      {params_.GetDlcAppId("dlc_2"), {.updated = false}},
  });
  OmahaEvent event(OmahaEvent::kTypeUpdateComplete);
  OmahaRequestBuilderXml omaha_request{&event,
                                       false,
                                       false,
                                       0,
                                       0,
                                       0,
                                       ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(
      1,
      CountSubstringInString(
          request_xml, "<event eventtype=\"3\" eventresult=\"1\"></event>"))
      << request_xml;
  EXPECT_EQ(
      2,
      CountSubstringInString(
          request_xml,
          "<event eventtype=\"3\" eventresult=\"0\" errorcode=\"62\"></event>"))
      << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest,
       GetRequestXmlUpdateCompleteEventMiniOsExcluded) {
  FakeSystemState::Get()->fake_boot_control()->SetSupportsMiniOSPartitions(
      true);
  params_.set_minios_app_params({.updated = false});

  OmahaEvent event(OmahaEvent::kTypeUpdateComplete);
  OmahaRequestBuilderXml omaha_request{&event, false, false, 0, 0, 0, ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(
      1,
      CountSubstringInString(
          request_xml, "<event eventtype=\"3\" eventresult=\"1\"></event>"))
      << request_xml;
  // MiniOS package is not updated due to exclusions. Send corresponding event.
  EXPECT_EQ(
      1,
      CountSubstringInString(
          request_xml,
          "<event eventtype=\"3\" eventresult=\"0\" errorcode=\"62\"></event>"))
      << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlDlcCohortMissingCheck) {
  constexpr char kDlcId[] = "test-dlc-id";
  params_.set_dlc_apps_params(
      {{params_.GetDlcAppId(kDlcId), {.name = kDlcId}}});
  OmahaEvent event(OmahaEvent::kTypeUpdateDownloadStarted);
  OmahaRequestBuilderXml omaha_request{&event, false, false, 0, 0, 0, ""};
  const string request_xml = omaha_request.GetRequest();

  // Check that no cohorts are in the request.
  EXPECT_EQ(0, CountSubstringInString(request_xml, "cohort=")) << request_xml;
  EXPECT_EQ(0, CountSubstringInString(request_xml, "cohortname="))
      << request_xml;
  EXPECT_EQ(0, CountSubstringInString(request_xml, "cohorthint="))
      << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlDlcCohortCheck) {
  const string kDlcId = "test-dlc-id";
  params_.set_dlc_apps_params(
      {{params_.GetDlcAppId(kDlcId), {.name = kDlcId}}});
  auto* fake_prefs = FakeSystemState::Get()->fake_prefs();
  OmahaEvent event(OmahaEvent::kTypeUpdateDownloadStarted);
  OmahaRequestBuilderXml omaha_request{&event, false, false, 0, 0, 0, ""};
  // DLC App ID Expectations.
  const string dlc_cohort_key = PrefsInterface::CreateSubKey(
      {kDlcPrefsSubDir, kDlcId, kPrefsOmahaCohort});
  const string kDlcCohortVal = "test-cohort";
  EXPECT_TRUE(fake_prefs->SetString(dlc_cohort_key, kDlcCohortVal));
  const string dlc_cohort_name_key = PrefsInterface::CreateSubKey(
      {kDlcPrefsSubDir, kDlcId, kPrefsOmahaCohortName});
  const string kDlcCohortNameVal = "test-cohortname";
  EXPECT_TRUE(fake_prefs->SetString(dlc_cohort_name_key, kDlcCohortNameVal));
  const string dlc_cohort_hint_key = PrefsInterface::CreateSubKey(
      {kDlcPrefsSubDir, kDlcId, kPrefsOmahaCohortHint});
  const string kDlcCohortHintVal = "test-cohortval";
  EXPECT_TRUE(fake_prefs->SetString(dlc_cohort_hint_key, kDlcCohortHintVal));
  const string request_xml = omaha_request.GetRequest();

  EXPECT_EQ(1,
            CountSubstringInString(
                request_xml,
                base::StringPrintf(
                    "cohort=\"%s\" cohortname=\"%s\" cohorthint=\"%s\"",
                    kDlcCohortVal.c_str(),
                    kDlcCohortNameVal.c_str(),
                    kDlcCohortHintVal.c_str())))
      << request_xml;
}

TEST_F(OmahaRequestBuilderXmlTest, GetRequestXmlHwCheck) {
  params_.set_hw_details(true);
  MockCrosHealthd mock_cros_healthd;
  FakeSystemState::Get()->set_cros_healthd(&mock_cros_healthd);

  const string sys_vendor = "fake-sys-vendor",
               product_name = "fake-product-name",
               product_version = "fake-product-version",
               bios_version = "fake-bios-version",
               model_name = "fake-model-name";
  auto boot_mode = TelemetryInfo::SystemV2Info::OsInfo::BootMode::kCrosEfi;
  uint32_t total_memory_kib = 123;
  uint64_t size = 456;

  TelemetryInfo telemetry_info{
      .system_v2_info =
          // NOLINTNEXTLINE(whitespace/braces)
      {
          .dmi_info =
              // NOLINTNEXTLINE(whitespace/braces)
          {
              .sys_vendor = sys_vendor,
              .product_name = product_name,
              .product_version = product_version,
              .bios_version = bios_version,
          },
          .os_info =
              // NOLINTNEXTLINE(whitespace/braces)
          {
              .boot_mode = boot_mode,
          },
      },
      .memory_info =
          // NOLINTNEXTLINE(whitespace/braces)
      {
          .total_memory_kib = total_memory_kib,
      },
      .block_device_info =
          // NOLINTNEXTLINE(whitespace/braces)
      {
          {
              .size = size,
          },
      },
      .cpu_info =
          // NOLINTNEXTLINE(whitespace/braces)
      {
          .physical_cpus =
              // NOLINTNEXTLINE(whitespace/braces)
          {
              {
                  .model_name = model_name,
              },
          },
      },
      .bus_devices =
          // NOLINTNEXTLINE(whitespace/braces)
      {
          {
              .device_class =
                  TelemetryInfo::BusDevice::BusDeviceClass::kWirelessController,
              .bus_type_info =
                  TelemetryInfo::BusDevice::PciBusInfo{
                      .vendor_id = 1,
                      .device_id = 2,
                      .driver = "fake-driver-1",
                  },
          },
          {
              .device_class =
                  TelemetryInfo::BusDevice::BusDeviceClass::kWirelessController,
              .bus_type_info =
                  TelemetryInfo::BusDevice::UsbBusInfo{
                      .vendor_id = 3,
                      .product_id = 4,
                  },
          },
          {
              .device_class =
                  TelemetryInfo::BusDevice::BusDeviceClass::kDisplayController,
              .bus_type_info =
                  TelemetryInfo::BusDevice::PciBusInfo{
                      .vendor_id = 5,
                      .device_id = 6,
                      .driver = "fake-driver-2",
                  },
          },
          {
              .device_class =
                  TelemetryInfo::BusDevice::BusDeviceClass::kDisplayController,
              .bus_type_info =
                  TelemetryInfo::BusDevice::UsbBusInfo{
                      .vendor_id = 7,
                      .product_id = 8,
                  },
          },
          // Should be ignored.
          {
              .device_class =
                  TelemetryInfo::BusDevice::BusDeviceClass::kEthernetController,
              .bus_type_info =
                  TelemetryInfo::BusDevice::PciBusInfo{
                      .vendor_id = 9,
                      .device_id = 10,
                      .driver = "fake-driver-3",
                  },
          },
          {
              .device_class =
                  TelemetryInfo::BusDevice::BusDeviceClass::kEthernetController,
              .bus_type_info =
                  TelemetryInfo::BusDevice::UsbBusInfo{
                      .vendor_id = 11,
                      .product_id = 12,
                  },
          },
      },
  };

  EXPECT_CALL(mock_cros_healthd, GetTelemetryInfo())
      .WillOnce(Return(&telemetry_info));

  OmahaRequestBuilderXml omaha_request{nullptr, false, false, 0, 0, 0, ""};
  const string request_xml = omaha_request.GetRequest();
  EXPECT_EQ(1,
            CountSubstringInString(
                request_xml,
                base::StringPrintf("    <hw"
                                   " vendor_name=\"%s\""
                                   " product_name=\"%s\""
                                   " product_version=\"%s\""
                                   " bios_version=\"%s\""
                                   " uefi=\"%" PRId32 "\""
                                   " system_memory_bytes=\"%" PRIu32 "\""
                                   " root_disk_drive=\"%" PRIu64 "\""
                                   " cpu_name=\"%s\""
                                   " wireless_drivers=\"%s\""
                                   " wireless_ids=\"%s\""
                                   " gpu_ids=\"%s\""
                                   " />\n",
                                   sys_vendor.c_str(),
                                   product_name.c_str(),
                                   product_version.c_str(),
                                   bios_version.c_str(),
                                   static_cast<int32_t>(boot_mode),
                                   total_memory_kib,
                                   size,
                                   model_name.c_str(),
                                   "fake-driver-1",
                                   "0100:0200 0300:0400",
                                   "0500:0600 0700:0800")))
      << request_xml;
}

}  // namespace chromeos_update_engine
