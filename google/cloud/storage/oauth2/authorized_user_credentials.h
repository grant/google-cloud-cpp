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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_AUTHORIZED_USER_CREDENTIALS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_AUTHORIZED_USER_CREDENTIALS_H_

#include "google/cloud/status.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/oauth2/refreshing_credentials_wrapper.h"
#include "google/cloud/storage/version.h"
#include <iostream>
#include <mutex>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
/// Object to hold information used to instantiate an AuthorizedUserCredentials.
struct AuthorizedUserCredentialsInfo {
  std::string client_id;
  std::string client_secret;
  std::string refresh_token;
  std::string token_uri;
};

/// Parses a user credentials JSON string into an AuthorizedUserCredentialsInfo.
StatusOr<AuthorizedUserCredentialsInfo> ParseAuthorizedUserCredentials(
    std::string const& content, std::string const& source,
    std::string const& default_token_uri = GoogleOAuthRefreshEndpoint());

/**
 * Wrapper class for Google OAuth 2.0 user account credentials.
 *
 * Takes a AuthorizedUserCredentialsInfo and obtains access tokens from the
 * Google Authorization Service as needed. Instances of this class should
 * usually be created via the convenience methods declared in
 * google_credentials.h.
 *
 * An HTTP Authorization header, with an access token as its value,
 * can be obtained by calling the AuthorizationHeader() method; if the current
 * access token is invalid or nearing expiration, this will class will first
 * obtain a new access token before returning the Authorization header string.
 *
 * @see https://developers.google.com/identity/protocols/OAuth2 for an overview
 * of using user credentials with Google's OAuth 2.0 system.
 *
 * @tparam HttpRequestBuilderType a dependency injection point. It makes it
 *     possible to mock internal libcurl wrappers. This should generally not be
 *     overridden except for testing.
 */
template <typename HttpRequestBuilderType =
              storage::internal::CurlRequestBuilder>
class AuthorizedUserCredentials : public Credentials {
 public:
  explicit AuthorizedUserCredentials(
      AuthorizedUserCredentialsInfo const& info) {
    HttpRequestBuilderType request_builder(
        info.token_uri, storage::internal::GetDefaultCurlHandleFactory());
    std::string payload("grant_type=refresh_token");
    payload += "&client_id=";
    payload += request_builder.MakeEscapedString(info.client_id).get();
    payload += "&client_secret=";
    payload += request_builder.MakeEscapedString(info.client_secret).get();
    payload += "&refresh_token=";
    payload += request_builder.MakeEscapedString(info.refresh_token).get();
    payload_ = std::move(payload);
    request_ = request_builder.BuildRequest();
  }

  StatusOr<std::string> AuthorizationHeader() override {
    std::unique_lock<std::mutex> lock(mu_);
    return refreshing_creds_.AuthorizationHeader(
        std::chrono::system_clock::now(), [this] { return Refresh(); });
  }

 private:
  StatusOr<RefreshingCredentialsWrapper::TemporaryToken> Refresh() {
    auto response = request_.MakeRequest(payload_);
    if (!response) {
      return std::move(response).status();
    }
    if (response->status_code >= 300) {
      return AsStatus(*response);
    }
    auto access_token =
        storage::internal::nl::json::parse(response->payload, nullptr, false);
    if (access_token.is_discarded() ||
        access_token.count("access_token") == 0 ||
        access_token.count("expires_in") == 0 ||
        access_token.count("id_token") == 0 ||
        access_token.count("token_type") == 0) {
      response->payload +=
          "Could not find all required fields in response (access_token,"
          " id_token, expires_in, token_type).";
      return AsStatus(*response);
    }
    std::string header = "Authorization: ";
    header += access_token.value("token_type", "");
    header += ' ';
    header += access_token.value("access_token", "");
    std::string new_id = access_token.value("id_token", "");
    auto expires_in =
        std::chrono::seconds(access_token.value("expires_in", int(0)));
    auto new_expiration = std::chrono::system_clock::now() + expires_in;

    return RefreshingCredentialsWrapper::TemporaryToken{std::move(header),
                                                        new_expiration};
  }

  typename HttpRequestBuilderType::RequestType request_;
  std::string payload_;
  mutable std::mutex mu_;
  RefreshingCredentialsWrapper refreshing_creds_;
};

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_AUTHORIZED_USER_CREDENTIALS_H_
