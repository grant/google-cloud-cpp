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

#include "google/cloud/storage/oauth2/authorized_user_credentials.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
StatusOr<AuthorizedUserCredentialsInfo> ParseAuthorizedUserCredentials(
    std::string const& content, std::string const& source,
    std::string const& default_token_uri) {
  auto credentials =
      storage::internal::nl::json::parse(content, nullptr, false);
  if (credentials.is_discarded()) {
    return Status(
        StatusCode::kInvalidArgument,
        "Invalid AuthorizedUserCredentials, parsing failed on data from " +
            source);
  }

  char const client_id_key[] = "client_id";
  char const client_secret_key[] = "client_secret";
  char const refresh_token_key[] = "refresh_token";
  char const token_uri_key[] = "token_uri";  // Not required; often not present.
  for (auto const& key :
       {client_id_key, client_secret_key, refresh_token_key}) {
    if (credentials.count(key) == 0) {
      return Status(StatusCode::kInvalidArgument,
                    "Invalid AuthorizedUserCredentials, the " +
                        std::string(key) +
                        " field is missing on data loaded from " + source);
    }
    if (credentials.value(key, "").empty()) {
      return Status(StatusCode::kInvalidArgument,
                    "Invalid AuthorizedUserCredentials, the " +
                        std::string(key) +
                        " field is empty on data loaded from " + source);
    }
  }
  return AuthorizedUserCredentialsInfo{
      credentials.value(client_id_key, ""),
      credentials.value(client_secret_key, ""),
      credentials.value(refresh_token_key, ""),
      // Some credential formats (e.g. gcloud's ADC file) don't contain a
      // "token_uri" attribute in the JSON object.  In this case, we try using
      // the default value.
      credentials.value(token_uri_key, default_token_uri)};
}
}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
