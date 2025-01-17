// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/curl_resumable_upload_session.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

StatusOr<ResumableUploadResponse> CurlResumableUploadSession::UploadChunk(
    std::string const& buffer) {
  UploadChunkRequest request(session_id_, next_expected_, buffer);
  auto result = client_->UploadChunk(request);
  Update(result);
  return result;
}

StatusOr<ResumableUploadResponse> CurlResumableUploadSession::UploadFinalChunk(
    std::string const& buffer, std::uint64_t upload_size) {
  UploadChunkRequest request(session_id_, next_expected_, buffer, upload_size);
  auto result = client_->UploadChunk(request);
  Update(result);
  return result;
}

StatusOr<ResumableUploadResponse> CurlResumableUploadSession::ResetSession() {
  QueryResumableUploadRequest request(session_id_);
  auto result = client_->QueryResumableUpload(request);
  Update(result);
  return result;
}

std::uint64_t CurlResumableUploadSession::next_expected_byte() const {
  return next_expected_;
}

void CurlResumableUploadSession::Update(
    StatusOr<ResumableUploadResponse> const& result) {
  if (!result.ok()) {
    return;
  }
  if (result->last_committed_byte == 0) {
    next_expected_ = 0;
  } else {
    next_expected_ = result->last_committed_byte + 1;
  }
  if (!result->upload_session_url.empty()) {
    session_id_ = result->upload_session_url;
  }
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
