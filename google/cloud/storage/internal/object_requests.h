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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_REQUESTS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_REQUESTS_H_

#include "google/cloud/storage/download_options.h"
#include "google/cloud/storage/hashing_options.h"
#include "google/cloud/storage/internal/generic_object_request.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/upload_options.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/storage/well_known_parameters.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
struct ObjectMetadataParser {
  static StatusOr<ObjectMetadata> FromJson(internal::nl::json const& json);
  static StatusOr<ObjectMetadata> FromString(std::string const& payload);
};

//@{
/**
 * @name Create the correct JSON payload depending on the operation.
 *
 * Depending on the specific operation being performed the JSON object sent to
 * the server needs to exclude different fields. We handle this by having
 * different functions for each operation, though their implementations are
 * shared.
 */
internal::nl::json ObjectMetadataJsonForCompose(ObjectMetadata const& meta);
internal::nl::json ObjectMetadataJsonForCopy(ObjectMetadata const& meta);
internal::nl::json ObjectMetadataJsonForInsert(ObjectMetadata const& meta);
internal::nl::json ObjectMetadataJsonForRewrite(ObjectMetadata const& meta);
internal::nl::json ObjectMetadataJsonForUpdate(ObjectMetadata const& meta);
//@}

/**
 * Represents a request to the `Objects: list` API.
 */
class ListObjectsRequest
    : public GenericRequest<ListObjectsRequest, MaxResults, Prefix, Projection,
                            UserProject, Versions> {
 public:
  ListObjectsRequest() = default;
  explicit ListObjectsRequest(std::string bucket_name)
      : bucket_name_(std::move(bucket_name)) {}

  std::string const& bucket_name() const { return bucket_name_; }
  std::string const& page_token() const { return page_token_; }
  ListObjectsRequest& set_page_token(std::string page_token) {
    page_token_ = std::move(page_token);
    return *this;
  }

 private:
  std::string bucket_name_;
  std::string page_token_;
};

std::ostream& operator<<(std::ostream& os, ListObjectsRequest const& r);

struct ListObjectsResponse {
  static StatusOr<ListObjectsResponse> FromHttpResponse(
      std::string const& payload);

  std::string next_page_token;
  std::vector<ObjectMetadata> items;
};

std::ostream& operator<<(std::ostream& os, ListObjectsResponse const& r);

/**
 * Represents a request to the `Objects: get` API.
 */
class GetObjectMetadataRequest
    : public GenericObjectRequest<
          GetObjectMetadataRequest, Generation, IfGenerationMatch,
          IfGenerationNotMatch, IfMetagenerationMatch, IfMetagenerationNotMatch,
          Projection, UserProject> {
 public:
  using GenericObjectRequest::GenericObjectRequest;
};

std::ostream& operator<<(std::ostream& os, GetObjectMetadataRequest const& r);

/**
 * Represents a request to the `Objects: insert` API with a string for the
 * media.
 *
 * This request type is used to upload objects with media that completely
 * fits in memory. Such requests are simpler than uploading objects streaming
 * objects.
 */
class InsertObjectMediaRequest
    : public GenericObjectRequest<
          InsertObjectMediaRequest, ContentEncoding, ContentType,
          Crc32cChecksumValue, DisableCrc32cChecksum, DisableMD5Hash,
          EncryptionKey, IfGenerationMatch, IfGenerationNotMatch,
          IfMetagenerationMatch, IfMetagenerationNotMatch, KmsKeyName,
          MD5HashValue, PredefinedAcl, Projection, UserProject,
          WithObjectMetadata> {
 public:
  InsertObjectMediaRequest() : GenericObjectRequest(), contents_() {}

  explicit InsertObjectMediaRequest(std::string bucket_name,
                                    std::string object_name,
                                    std::string contents)
      : GenericObjectRequest(std::move(bucket_name), std::move(object_name)),
        contents_(std::move(contents)) {}

  std::string const& contents() const { return contents_; }
  InsertObjectMediaRequest& set_contents(std::string&& v) {
    contents_ = std::move(v);
    return *this;
  }

 private:
  std::string contents_;
};

std::ostream& operator<<(std::ostream& os, InsertObjectMediaRequest const& r);

/**
 * Represents a request to the `Objects: insert` API where the media will be
 * uploaded as a stream.
 *
 * This request type is used to upload objects where the media is not known in
 * advance, and it is uploaded using chunked encoding as it is generated by the
 * application.
 */
class InsertObjectStreamingRequest
    : public GenericObjectRequest<
          InsertObjectStreamingRequest, ContentEncoding, ContentType,
          Crc32cChecksumValue, DisableCrc32cChecksum, DisableMD5Hash,
          EncryptionKey, IfGenerationMatch, IfGenerationNotMatch,
          IfMetagenerationMatch, IfMetagenerationNotMatch, KmsKeyName,
          MD5HashValue, PredefinedAcl, Projection, UseResumableUploadSession,
          UserProject, WithObjectMetadata> {
 public:
  using GenericObjectRequest::GenericObjectRequest;
};

std::ostream& operator<<(std::ostream& os,
                         InsertObjectStreamingRequest const& r);

/**
 * Represents a request to the `Objects: copy` API.
 */
class CopyObjectRequest
    : public GenericRequest<
          CopyObjectRequest, DestinationPredefinedAcl, EncryptionKey,
          IfGenerationMatch, IfGenerationNotMatch, IfMetagenerationMatch,
          IfMetagenerationNotMatch, IfSourceGenerationMatch,
          IfSourceGenerationNotMatch, IfSourceMetagenerationMatch,
          IfSourceMetagenerationNotMatch, Projection, SourceGeneration,
          UserProject, WithObjectMetadata> {
 public:
  CopyObjectRequest() = default;
  CopyObjectRequest(std::string source_bucket, std::string source_object,
                    std::string destination_bucket,
                    std::string destination_object)
      : source_bucket_(std::move(source_bucket)),
        source_object_(std::move(source_object)),
        destination_bucket_(std::move(destination_bucket)),
        destination_object_(std::move(destination_object)) {}

  std::string const& source_bucket() const { return source_bucket_; }
  std::string const& source_object() const { return source_object_; }
  std::string const& destination_bucket() const { return destination_bucket_; }
  std::string const& destination_object() const { return destination_object_; }

 private:
  std::string source_bucket_;
  std::string source_object_;
  std::string destination_bucket_;
  std::string destination_object_;
};

std::ostream& operator<<(std::ostream& os, CopyObjectRequest const& r);

/**
 * Represents a request to the `Objects: get` API with `alt=media`.
 */
class ReadObjectRangeRequest
    : public GenericObjectRequest<
          ReadObjectRangeRequest, DisableCrc32cChecksum, DisableMD5Hash,
          EncryptionKey, Generation, IfGenerationMatch, IfGenerationNotMatch,
          IfMetagenerationMatch, IfMetagenerationNotMatch, ReadFromOffset,
          ReadRange, UserProject> {
 public:
  using GenericObjectRequest::GenericObjectRequest;

  bool RequiresNoCache() const;
  bool RequiresRangeHeader() const;
  std::string RangeHeader() const;
  std::int64_t StartingByte() const;
};

std::ostream& operator<<(std::ostream& os, ReadObjectRangeRequest const& r);

struct ReadObjectRangeResponse {
  std::string contents;
  std::int64_t first_byte;
  std::int64_t last_byte;
  std::int64_t object_size;

  static ReadObjectRangeResponse FromHttpResponse(HttpResponse&& response);
};

std::ostream& operator<<(std::ostream& os, ReadObjectRangeResponse const& r);

/**
 * Represents a request to the `Objects: delete` API.
 */
class DeleteObjectRequest
    : public GenericObjectRequest<DeleteObjectRequest, Generation,
                                  IfGenerationMatch, IfGenerationNotMatch,
                                  IfMetagenerationMatch,
                                  IfMetagenerationNotMatch, UserProject> {
 public:
  using GenericObjectRequest::GenericObjectRequest;
};

std::ostream& operator<<(std::ostream& os, DeleteObjectRequest const& r);

/**
 * Represents a request to the `Objects: update` API.
 */
class UpdateObjectRequest
    : public GenericObjectRequest<
          UpdateObjectRequest, Generation, IfGenerationMatch,
          IfGenerationNotMatch, IfMetagenerationMatch, IfMetagenerationNotMatch,
          PredefinedAcl, Projection, UserProject> {
 public:
  UpdateObjectRequest() = default;
  explicit UpdateObjectRequest(std::string bucket_name, std::string object_name,
                               ObjectMetadata metadata)
      : GenericObjectRequest(std::move(bucket_name), std::move(object_name)),
        metadata_(std::move(metadata)) {}

  /// Returns the request as the JSON API payload.
  std::string json_payload() const {
    return ObjectMetadataJsonForUpdate(metadata_).dump();
  }

  ObjectMetadata const& metadata() const { return metadata_; }

 private:
  ObjectMetadata metadata_;
};

std::ostream& operator<<(std::ostream& os, UpdateObjectRequest const& r);

/**
 * Represents a request to the `Objects: compose` API.
 */
class ComposeObjectRequest
    : public GenericObjectRequest<ComposeObjectRequest, EncryptionKey,
                                  DestinationPredefinedAcl, KmsKeyName,
                                  IfGenerationMatch, IfMetagenerationMatch,
                                  UserProject, WithObjectMetadata> {
 public:
  ComposeObjectRequest() = default;
  explicit ComposeObjectRequest(std::string bucket_name,
                                std::vector<ComposeSourceObject> source_objects,
                                std::string destination_object_name);

  /// Returns the request as the JSON API payload.
  std::string JsonPayload() const;

 private:
  std::vector<ComposeSourceObject> source_objects_;
};

std::ostream& operator<<(std::ostream& os, ComposeObjectRequest const& r);

/**
 * Represents a request to the `Buckets: patch` API.
 */
class PatchObjectRequest
    : public GenericObjectRequest<
          PatchObjectRequest, IfMetagenerationMatch, IfMetagenerationNotMatch,
          PredefinedAcl, PredefinedDefaultObjectAcl, Projection, UserProject> {
 public:
  PatchObjectRequest() = default;
  explicit PatchObjectRequest(std::string bucket_name, std::string object_name,
                              ObjectMetadata const& original,
                              ObjectMetadata const& updated);
  explicit PatchObjectRequest(std::string bucket_name, std::string object_name,
                              ObjectMetadataPatchBuilder const& patch);

  std::string const& payload() const { return payload_; }

 private:
  std::string payload_;
};

std::ostream& operator<<(std::ostream& os, PatchObjectRequest const& r);

/**
 * Represents a request to the `Objects: rewrite` API.
 */
class RewriteObjectRequest
    : public GenericRequest<
          RewriteObjectRequest, DestinationKmsKeyName, DestinationPredefinedAcl,
          EncryptionKey, IfGenerationMatch, IfGenerationNotMatch,
          IfMetagenerationMatch, IfMetagenerationNotMatch,
          IfSourceGenerationMatch, IfSourceGenerationNotMatch,
          IfSourceMetagenerationMatch, IfSourceMetagenerationNotMatch,
          MaxBytesRewrittenPerCall, Projection, SourceEncryptionKey,
          SourceGeneration, UserProject, WithObjectMetadata> {
 public:
  RewriteObjectRequest() = default;
  RewriteObjectRequest(std::string source_bucket, std::string source_object,
                       std::string destination_bucket,
                       std::string destination_object,
                       std::string rewrite_token)
      : source_bucket_(std::move(source_bucket)),
        source_object_(std::move(source_object)),
        destination_bucket_(std::move(destination_bucket)),
        destination_object_(std::move(destination_object)),
        rewrite_token_(std::move(rewrite_token)) {}

  std::string const& source_bucket() const { return source_bucket_; }
  std::string const& source_object() const { return source_object_; }
  std::string const& destination_bucket() const { return destination_bucket_; }
  std::string const& destination_object() const { return destination_object_; }
  std::string const& rewrite_token() const { return rewrite_token_; }
  void set_rewrite_token(std::string v) { rewrite_token_ = std::move(v); }

 private:
  std::string source_bucket_;
  std::string source_object_;
  std::string destination_bucket_;
  std::string destination_object_;
  std::string rewrite_token_;
};

std::ostream& operator<<(std::ostream& os, RewriteObjectRequest const& r);

/// Holds an `Objects: rewrite` response.
struct RewriteObjectResponse {
  static StatusOr<RewriteObjectResponse> FromHttpResponse(
      std::string const& payload);

  std::uint64_t total_bytes_rewritten;
  std::uint64_t object_size;
  bool done;
  std::string rewrite_token;
  ObjectMetadata resource;
};

std::ostream& operator<<(std::ostream& os, RewriteObjectResponse const& r);

/**
 * Represents a request to start a resumable upload in `Objects: insert`.
 *
 * This request type is used to start resumable uploads. A resumable upload is
 * started with a `Objects: insert` request with the `uploadType=resumable`
 * query parameter. The payload for the initial request includes the (optional)
 * object metadata. The response includes a URL to send requests that upload
 * the media.
 */
class ResumableUploadRequest
    : public GenericObjectRequest<
          ResumableUploadRequest, ContentEncoding, ContentType,
          Crc32cChecksumValue, DisableCrc32cChecksum, DisableMD5Hash,
          EncryptionKey, IfGenerationMatch, IfGenerationNotMatch,
          IfMetagenerationMatch, IfMetagenerationNotMatch, KmsKeyName,
          MD5HashValue, PredefinedAcl, Projection, UseResumableUploadSession,
          UserProject, WithObjectMetadata> {
 public:
  ResumableUploadRequest() = default;

  ResumableUploadRequest(std::string bucket_name, std::string object_name)
      : GenericObjectRequest(std::move(bucket_name), std::move(object_name)) {}
};

std::ostream& operator<<(std::ostream& os, ResumableUploadRequest const& r);

/**
 * A request to send one chunk in an upload session.
 */
class UploadChunkRequest : public GenericRequest<UploadChunkRequest> {
 public:
  UploadChunkRequest() = default;
  UploadChunkRequest(std::string upload_session_url, std::uint64_t range_begin,
                     std::string payload)
      : GenericRequest(),
        upload_session_url_(std::move(upload_session_url)),
        range_begin_(range_begin),
        payload_(std::move(payload)),
        source_size_(0),
        last_chunk_(false) {}
  UploadChunkRequest(std::string upload_session_url, std::uint64_t range_begin,
                     std::string payload, std::uint64_t source_size)
      : GenericRequest(),
        upload_session_url_(std::move(upload_session_url)),
        range_begin_(range_begin),
        payload_(std::move(payload)),
        source_size_(source_size),
        last_chunk_(true) {}

  std::string const& upload_session_url() const { return upload_session_url_; }
  std::uint64_t range_begin() const { return range_begin_; }
  std::uint64_t range_end() const { return range_begin_ + payload_.size() - 1; }
  std::uint64_t source_size() const { return source_size_; }
  std::string const& payload() const { return payload_; }

  std::string RangeHeader() const;

  // Chunks must be multiples of 256 KiB:
  //  https://cloud.google.com/storage/docs/json_api/v1/how-tos/resumable-upload
  static constexpr std::size_t kChunkSizeQuantum = 256 * 1024UL;

  static std::size_t RoundUpToQuantum(std::size_t max_chunk_size) {
    // If you are tempted to use bit manipulation to do this, modern compilers
    // known how to optimize this (coryan tested this at godbolt.org):
    //   https://godbolt.org/z/xxUsjg
    if (max_chunk_size % kChunkSizeQuantum == 0) {
      return max_chunk_size;
    }
    auto n = max_chunk_size / kChunkSizeQuantum;
    return (n + 1) * kChunkSizeQuantum;
  }

 private:
  std::string upload_session_url_;
  std::uint64_t range_begin_ = 0;
  std::string payload_;
  std::uint64_t source_size_ = 0;
  bool last_chunk_ = false;
};

std::ostream& operator<<(std::ostream& os, UploadChunkRequest const& r);

/**
 * A request to query the status of a resumable upload.
 */
class QueryResumableUploadRequest
    : public GenericRequest<QueryResumableUploadRequest> {
 public:
  QueryResumableUploadRequest() = default;
  explicit QueryResumableUploadRequest(std::string upload_session_url)
      : GenericRequest(), upload_session_url_(std::move(upload_session_url)) {}

  std::string const& upload_session_url() const { return upload_session_url_; }

 private:
  std::string upload_session_url_;
};

std::ostream& operator<<(std::ostream& os,
                         QueryResumableUploadRequest const& r);

struct ResumableUploadResponse {
  static StatusOr<ResumableUploadResponse> FromHttpResponse(
      HttpResponse&& response);

  std::string upload_session_url;
  std::uint64_t last_committed_byte;
  std::string payload;
};

std::ostream& operator<<(std::ostream& os, ResumableUploadResponse const& r);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_REQUESTS_H_
