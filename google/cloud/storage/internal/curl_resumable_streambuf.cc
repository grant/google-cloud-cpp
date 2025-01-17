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

#include "google/cloud/storage/internal/curl_resumable_streambuf.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

CurlResumableStreambuf::CurlResumableStreambuf(
    std::unique_ptr<ResumableUploadSession> upload_session,
    std::size_t max_buffer_size, std::unique_ptr<HashValidator> hash_validator)
    : upload_session_(std::move(upload_session)),
      max_buffer_size_(UploadChunkRequest::RoundUpToQuantum(max_buffer_size)),
      hash_validator_(std::move(hash_validator)),
      last_response_{400, {}, {}} {
  current_ios_buffer_.reserve(max_buffer_size_);
  auto pbeg = &current_ios_buffer_[0];
  auto pend = pbeg + current_ios_buffer_.size();
  setp(pbeg, pend);
}

bool CurlResumableStreambuf::IsOpen() const {
  return static_cast<bool>(upload_session_);
}

bool CurlResumableStreambuf::ValidateHash(ObjectMetadata const& meta) {
  hash_validator_->ProcessMetadata(meta);
  hash_validator_result_ = std::move(*hash_validator_).Finish();
  return !hash_validator_result_.is_mismatch;
}

CurlResumableStreambuf::int_type CurlResumableStreambuf::overflow(int_type ch) {
  if (!IsOpen()) {
    return traits_type::eof();
  }
  if (traits_type::eq_int_type(ch, traits_type::eof())) {
    // For ch == EOF this function must do nothing and return any value != EOF.
    return 0;
  }
  // If the buffer is full flush it immediately.
  auto status = Flush();
  if (!status.ok()) {
    return traits_type::eof();
  }
  // Push the character into the current buffer.
  current_ios_buffer_.push_back(traits_type::to_char_type(ch));
  pbump(1);
  return ch;
}

int CurlResumableStreambuf::sync() {
  auto status = Flush();
  if (!status.ok()) {
    return traits_type::eof();
  }
  return 0;
}

std::streamsize CurlResumableStreambuf::xsputn(char const* s,
                                               std::streamsize count) {
  if (!IsOpen()) {
    return traits_type::eof();
  }
  current_ios_buffer_.assign(pbase(), pptr());
  current_ios_buffer_.append(s, static_cast<std::size_t>(count));
  auto pbeg = &current_ios_buffer_[0];
  auto pend = pbeg + current_ios_buffer_.size();
  setp(pbeg, pend);
  pbump(static_cast<int>(current_ios_buffer_.size()));

  auto status = Flush();
  if (!status.ok()) {
    return traits_type::eof();
  }
  return count;
}

StatusOr<HttpResponse> CurlResumableStreambuf::DoClose() {
  GCP_LOG(INFO) << __func__ << "()";
  return FlushFinal();
}

StatusOr<HttpResponse> CurlResumableStreambuf::FlushFinal() {
  // Shorten the buffer to the actual used size.
  auto actual_size = static_cast<std::size_t>(pptr() - pbase());
  std::size_t upload_size = upload_session_->next_expected_byte() + actual_size;
  current_ios_buffer_.resize(actual_size);
  hash_validator_->Update(current_ios_buffer_.data(),
                          current_ios_buffer_.size());

  StatusOr<ResumableUploadResponse> result =
      upload_session_->UploadFinalChunk(current_ios_buffer_, upload_size);
  if (!result) {
    // This was an unrecoverable error, time to signal an error.
    return std::move(result).status();
  }
  // Reset the iostream put area with valid pointers, but empty.
  current_ios_buffer_.resize(1);
  auto pbeg = &current_ios_buffer_[0];
  setp(pbeg, pbeg);

  upload_session_.reset();

  // If `result.ok() == false` we never get to this point, so the last response
  // was actually successful. Represent that by a HTTP 200 status code.
  last_response_ = HttpResponse{200, std::move(result).value().payload, {}};
  return last_response_;
}

StatusOr<HttpResponse> CurlResumableStreambuf::Flush() {
  if (!IsOpen()) {
    return last_response_;
  }
  // Shorten the buffer to the actual used size.
  auto actual_size = static_cast<std::size_t>(pptr() - pbase());
  if (actual_size < max_buffer_size_) {
    return last_response_;
  }

  auto chunk_count = actual_size / UploadChunkRequest::kChunkSizeQuantum;
  auto chunk_size = chunk_count * UploadChunkRequest::kChunkSizeQuantum;
  std::string not_sent(pbase() + chunk_size, pbase() + actual_size);
  current_ios_buffer_.assign(pbase(), pbase() + chunk_size);
  hash_validator_->Update(current_ios_buffer_.data(),
                          current_ios_buffer_.size());

  StatusOr<ResumableUploadResponse> result =
      upload_session_->UploadChunk(current_ios_buffer_);
  if (!result) {
    // This was an unrecoverable error, time to signal an error.
    return std::move(result).status();
  }
  // Reset the put area, preserve any data not setn.
  current_ios_buffer_.clear();
  current_ios_buffer_.reserve(max_buffer_size_);
  current_ios_buffer_.append(not_sent);
  auto pbeg = &current_ios_buffer_[0];
  auto pend = pbeg + current_ios_buffer_.size();
  setp(pbeg, pend);
  pbump(static_cast<int>(not_sent.size()));

  // If `result.ok() == false` we never get to this point, so the last response
  // was actually successful. Represent that by a HTTP 200 status code.
  last_response_ = HttpResponse{200, std::move(result).value().payload, {}};
  return last_response_;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
