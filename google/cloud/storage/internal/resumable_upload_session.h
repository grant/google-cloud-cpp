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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RESUMABLE_UPLOAD_SESSION_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RESUMABLE_UPLOAD_SESSION_H_

#include "google/cloud/status_or.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Defines the interface for a resumable upload session.
 */
class ResumableUploadSession {
 public:
  virtual ~ResumableUploadSession() = default;

  /**
   * Uploads a chunk and returns the resulting response.
   *
   * @param buffer the chunk to upload.
   * @return The result of uploading the chunk.
   */
  virtual StatusOr<ResumableUploadResponse> UploadChunk(
      std::string const& buffer) = 0;

  /**
   * Uploads the final chunk in a stream, committing all previous data.
   *
   * @param buffer the chunk to upload.
   * @param upload_size the total size of the upload, use `0` if the size is not
   *   known.
   * @return The final result of the upload, including the object metadata.
   */
  virtual StatusOr<ResumableUploadResponse> UploadFinalChunk(
      std::string const& buffer, std::uint64_t upload_size) = 0;

  /// Resets the session by querying its current state.
  virtual StatusOr<ResumableUploadResponse> ResetSession() = 0;

  /**
   * Returns the next expected byte in the server.
   *
   * Users of this class should check this value in case a previous
   * UploadChunk() has partially failed and the application (or the component
   * using this class) needs to re-send a chunk.
   */
  virtual std::uint64_t next_expected_byte() const = 0;

  /**
   * Returns the current upload session id.
   *
   * Note that the session id might change during an upload.
   */
  virtual std::string const& session_id() const = 0;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RESUMABLE_UPLOAD_SESSION_H_
