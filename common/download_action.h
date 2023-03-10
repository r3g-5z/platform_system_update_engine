//
// Copyright (C) 2011 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_COMMON_DOWNLOAD_ACTION_H_
#define UPDATE_ENGINE_COMMON_DOWNLOAD_ACTION_H_

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <utility>

#include "update_engine/common/boot_control_interface.h"
#include "update_engine/common/http_fetcher.h"
#include "update_engine/common/multi_range_http_fetcher.h"
#include "update_engine/payload_consumer/delta_performer.h"
#include "update_engine/payload_consumer/install_plan.h"

// The Download Action downloads a specified url to disk. The url should point
// to an update in a delta payload format. The payload will be piped into a
// DeltaPerformer that will apply the delta to the disk.

namespace chromeos_update_engine {

class DownloadActionDelegate {
 public:
  virtual ~DownloadActionDelegate() = default;

  // Called periodically after bytes are received. This method will be invoked
  // only if the DownloadAction is running. |bytes_progressed| is the number of
  // bytes downloaded since the last call of this method, |bytes_received|
  // the number of bytes downloaded thus far and |total| is the number of bytes
  // expected.
  virtual void BytesReceived(uint64_t bytes_progressed,
                             uint64_t bytes_received,
                             uint64_t total) = 0;

  // Returns whether the download should be canceled, in which case the
  // |cancel_reason| error should be set to the reason why the download was
  // canceled.
  virtual bool ShouldCancel(ErrorCode* cancel_reason) = 0;

  // Called once the complete payload has been downloaded. Note that any errors
  // while applying or downloading the partial payload will result in this
  // method not being called.
  virtual void DownloadComplete() = 0;
};

class PrefsInterface;

class DownloadAction : public InstallPlanAction, public HttpFetcherDelegate {
 public:
  // Debugging/logging
  static std::string StaticType() { return "DownloadAction"; }

  // Takes ownership of the passed in HttpFetcher. Useful for testing.
  // A good calling pattern is:
  // DownloadAction(prefs, boot_contol, hardware,
  //                new WhateverHttpFetcher, false);
  DownloadAction(
      PrefsInterface* prefs,
      BootControlInterface* boot_control,
      HardwareInterface* hardware,
      HttpFetcher* http_fetcher,
      bool interactive,
      std::string update_certs_path = constants::kUpdateCertificatesPath);
  ~DownloadAction() override;

  // InstallPlanAction overrides.
  void PerformAction() override;
  void SuspendAction() override;
  void ResumeAction() override;
  void TerminateProcessing() override;
  std::string Type() const override { return StaticType(); }

  // Testing
  void SetTestFileWriter(std::unique_ptr<DeltaPerformer> writer) {
    delta_performer_ = std::move(writer);
  }

  int GetHTTPResponseCode() { return http_fetcher_->http_response_code(); }

  // HttpFetcherDelegate methods (see http_fetcher.h)
  bool ReceivedBytes(HttpFetcher* fetcher,
                     const void* bytes,
                     size_t length) override;
  void SeekToOffset(off_t offset) override;
  void TransferComplete(HttpFetcher* fetcher, bool successful) override;
  void TransferTerminated(HttpFetcher* fetcher) override;

  DownloadActionDelegate* delegate() const { return delegate_; }
  void set_delegate(DownloadActionDelegate* delegate) { delegate_ = delegate; }

  void set_base_offset(int64_t base_offset) { base_offset_ = base_offset; }

  HttpFetcher* http_fetcher() { return http_fetcher_.get(); }

 private:
  // Attempt to load cached manifest data from prefs
  // return true on success, false otherwise.
  bool LoadCachedManifest(int64_t manifest_size);

  // Start downloading the current payload using delta_performer.
  void StartDownloading();

  // Pointer to the current payload in install_plan_.payloads.
  InstallPlan::Payload* payload_{nullptr};

  // Required pointers.
  PrefsInterface* prefs_;
  BootControlInterface* boot_control_;
  HardwareInterface* hardware_;

  // Pointer to the MultiRangeHttpFetcher that does the http work.
  std::unique_ptr<MultiRangeHttpFetcher> http_fetcher_;

  // If |true|, the update is user initiated (vs. periodic update checks). Hence
  // the |delta_performer_| can decide not to use O_DSYNC flag for faster
  // update.
  bool interactive_;

  std::unique_ptr<DeltaPerformer> delta_performer_;

  // Used by TransferTerminated to figure if this action terminated itself or
  // was terminated by the action processor.
  ErrorCode code_;

  // For reporting status to outsiders
  DownloadActionDelegate* delegate_;
  uint64_t bytes_received_{0};  // per file/range
  uint64_t bytes_received_previous_payloads_{0};
  uint64_t bytes_total_{0};
  bool download_active_{false};

  // Loaded from prefs before downloading any payload.
  size_t resume_payload_index_{0};

  // Offset of the payload in the download URL, used by UpdateAttempterAndroid.
  int64_t base_offset_{0};

  // The path to the zip file with X509 certificates.
  const std::string update_certificates_path_;

  DISALLOW_COPY_AND_ASSIGN(DownloadAction);
};

// We want to be sure that we're compiled with large file support on linux,
// just in case we find ourselves downloading large images.
static_assert(8 == sizeof(off_t), "off_t not 64 bit");

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_COMMON_DOWNLOAD_ACTION_H_
