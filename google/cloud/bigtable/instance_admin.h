// Copyright 2018 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_ADMIN_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_ADMIN_H_

#include "google/cloud/bigtable/app_profile_config.h"
#include "google/cloud/bigtable/cluster_config.h"
#include "google/cloud/bigtable/cluster_list_responses.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/instance_admin_client.h"
#include "google/cloud/bigtable/instance_config.h"
#include "google/cloud/bigtable/instance_list_responses.h"
#include "google/cloud/bigtable/instance_update_config.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/future.h"
#include "google/cloud/iam_policy.h"
#include "google/cloud/status_or.h"
#include <future>
#include <memory>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Implements the APIs to administer Cloud Bigtable instances.
 *
 * @par Thread-safety
 * Instances of this class created via copy-construction or copy-assignment
 * share the underlying pool of connections. Access to these copies via multiple
 * threads is guaranteed to work. Two threads operating on the same instance of
 * this class is not guaranteed to work.
 *
 * @par Cost
 * Creating a new object of type `InstanceAdmin` is comparable to creating a few
 * objects of type `std::string` or a few objects of type
 * `std::shared_ptr<int>`. The class represents a shallow handle to a remote
 * object.
 *
 * @par Error Handling
 * This class uses `StatusOr<T>` to report errors. When an operation fails to
 * perform its work the returned `StatusOr<T>` contains the error details. If
 * the `ok()` member function in the `StatusOr<T>` returns `true` then it
 * contains the expected result. Operations that do not return a value simply
 * return a `google::cloud::Status` indicating success or the details of the
 * error Please consult the
 * [`StatusOr<T>` documentation](#google::cloud::v0::StatusOr) for more details.
 *
 * @code
 * namespace cbt = google::cloud::bigtable;
 * namespace btadmin = google::bigtable::admin::v2;
 * cbt::TableAdmin admin = ...;
 * google::cloud::StatusOr<btadmin::Table> metadata = admin.GetTable(...);
 *
 * if (!metadata) {
 *   std::cerr << "Error fetching table metadata\n";
 *   return;
 * }
 *
 * // Use "metadata" as a smart pointer here, e.g.:
 * std::cout << "The full table name is " << table->name() << " the table has "
 *           << table->column_families_size() << " column families\n";
 * @endcode
 *
 * In addition, the @ref index "main page" contains examples using `StatusOr<T>`
 * to handle errors.
 *
 * @par Retry, Backoff, and Idempotency Policies
 * The library automatically retries requests that fail with transient errors,
 * and uses [truncated exponential backoff][backoff-link] to backoff between
 * retries. The default policies are to continue retrying for up to 10 minutes.
 * On each transient failure the backoff period is doubled, starting with an
 * initial backoff of 100 milliseconds. The backoff period growth is truncated
 * at 60 seconds. The default idempotency policy is to only retry idempotent
 * operations. Note that most operations that change state are **not**
 * idempotent.
 *
 * The application can override these policies when constructing objects of this
 * class. The documentation for the constructors show examples of this in
 * action.
 *
 * [backoff-link]: https://cloud.google.com/storage/docs/exponential-backoff
 *
 * @see https://cloud.google.com/bigtable/ for an overview of Cloud Bigtable.
 *
 * @see https://cloud.google.com/bigtable/docs/overview for an overview of the
 *     Cloud Bigtable data model.
 *
 * @see https://cloud.google.com/bigtable/docs/instances-clusters-nodes for an
 *     introduction of the main APIs into Cloud Bigtable.
 *
 * @see https://cloud.google.com/bigtable/docs/reference/service-apis-overview
 *     for an overview of the underlying Cloud Bigtable API.
 *
 * @see #google::cloud::v0::StatusOr for a description of the error reporting
 *     class used by this library.
 *
 * @see `LimitedTimeRetryPolicy` and `LimitedErrorCountRetryPolicy` for
 *     alternative retry policies.
 *
 * @see `ExponentialBackoffPolicy` to configure different parameters for the
 *     exponential backoff policy.
 *
 * @see `SafeIdempotentMutationPolicy` and `AlwaysRetryMutationPolicy` for
 *     alternative idempotency policies.
 */
class InstanceAdmin {
 public:
  /**
   * @param client the interface to create grpc stubs, report errors, etc.
   */
  explicit InstanceAdmin(std::shared_ptr<InstanceAdminClient> client)
      : client_(std::move(client)),
        project_name_("projects/" + project_id()),
        rpc_retry_policy_(
            DefaultRPCRetryPolicy(internal::kBigtableInstanceAdminLimits)),
        rpc_backoff_policy_(
            DefaultRPCBackoffPolicy(internal::kBigtableInstanceAdminLimits)),
        polling_policy_(
            DefaultPollingPolicy(internal::kBigtableInstanceAdminLimits)),
        metadata_update_policy_(project_name(), MetadataParamTypes::PARENT) {}

  /**
   * Create a new InstanceAdmin using explicit policies to handle RPC errors.
   *
   * @param client the interface to create grpc stubs, report errors, etc.
   * @param policies the set of policy overrides for this object.
   * @tparam Policies the types of the policies to override, the types must
   *     derive from one of the following types:
   *     - `RPCBackoffPolicy` how to backoff from a failed RPC. Currently only
   *       `ExponentialBackoffPolicy` is implemented. You can also create your
   *       own policies that backoff using a different algorithm.
   *     - `RPCRetryPolicy` for how long to retry failed RPCs. Use
   *       `LimitedErrorCountRetryPolicy` to limit the number of failures
   *       allowed. Use `LimitedTimeRetryPolicy` to bound the time for any
   *       request. You can also create your own policies that combine time and
   *       error counts.
   *     - `PollingPolicy` for how long will the class wait for
   *       `google.longrunning.Operation` to complete. This class combines both
   *       the backoff policy for checking long running operations and the
   *       retry policy.
   *
   * @see GenericPollingPolicy, ExponentialBackoffPolicy,
   *     LimitedErrorCountRetryPolicy, LimitedTimeRetryPolicy.
   */
  template <typename... Policies>
  explicit InstanceAdmin(std::shared_ptr<InstanceAdminClient> client,
                         Policies&&... policies)
      : InstanceAdmin(std::move(client)) {
    ChangePolicies(std::forward<Policies>(policies)...);
  }

  /// The full name (`projects/<project_id>`) of the project.
  std::string const& project_name() const { return project_name_; }
  /// The project id, i.e., `project_name()` without the `projects/` prefix.
  std::string const& project_id() const { return client_->project(); }

  /// Return the fully qualified name of the given instance_id.
  std::string InstanceName(std::string const& instance_id) const {
    return project_name() + "/instances/" + instance_id;
  }

  /// Return the fully qualified name of the given cluster_id in give
  /// instance_id.
  std::string ClusterName(std::string const& instance_id,
                          std::string const& cluster_id) const {
    return InstanceName(instance_id) + "/clusters/" + cluster_id;
  }

  /**
   * Create a new instance of Cloud Bigtable.
   *
   * @warning Note that this is operation can take seconds or minutes to
   * complete. The application may prefer to perform other work while waiting
   * for this operation.
   *
   * @param instance_config a description of the new instance to be created.
   *   instance_id and a display_name parameters must be set in instance_config,
   *   - instance_id : must be between 6 and 33 characters.
   *   - display_name : must be between 4 and 30 characters.
   * @return a future that becomes satisfied when (a) the operation has
   *   completed successfully, in which case it returns a proto with the
   *   Instance details, (b) the operation has failed, in which case the future
   *   contains an `google::cloud::Status` with the details of the failure, or
   *   (c) the state of the operation is unknown after the time allocated by the
   *   retry policies has expired, in which case the future contains the last
   *   error status.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc create instance
   */
  future<StatusOr<google::bigtable::admin::v2::Instance>> CreateInstance(
      InstanceConfig instance_config);

  /**
   * Create an instance (asynchronously).
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param instance_config the desired configuration of the instance.
   *
   * @return a future satisfied when either (a) the cluster is created or (b)
   *     a permanent error occurs or (c) polling or retry policy has been
   *     exhausted.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async create instance
   */
  future<StatusOr<google::bigtable::admin::v2::Instance>> AsyncCreateInstance(
      CompletionQueue& cq, bigtable::InstanceConfig instance_config);

  /**
   * Create a new Cluster of Cloud Bigtable.
   *
   * @param cluster_config a description of the new cluster to be created.
   * @param instance_id the id of the instance in the project
   * @param cluster_id the id of the cluster in the project that needs to be
   *   created. It must be between 6 and 30 characters.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc create cluster
   */
  future<StatusOr<google::bigtable::admin::v2::Cluster>> CreateCluster(
      ClusterConfig cluster_config, std::string const& instance_id,
      std::string const& cluster_id);

  /**
   * Create a new cluster (asynchronously).
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param cluster_config a description of the new cluster to be created.
   * @param instance_id the id of the instance in the project
   * @param cluster_id the id of the cluster in the project that needs to be
   *   created. It must be between 6 and 30 characters.
   *
   * @return a future satisfied when either (a) the cluster is created or (b)
   *     a permanent error occurs or (c) polling or retry policy has been
   *     exhausted.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async create cluster
   */
  future<StatusOr<google::bigtable::admin::v2::Cluster>> AsyncCreateCluster(
      CompletionQueue& cq, ClusterConfig cluster_config,
      std::string const& instance_id, std::string const& cluster_id);

  /**
   * Update an existing instance of Cloud Bigtable.
   *
   * @warning Note that this is operation can take seconds or minutes to
   * complete. The application may prefer to perform other work while waiting
   * for this operation.
   *
   * @param instance_update_config config with modified instance.
   * @return a future that becomes satisfied when (a) the operation has
   *   completed successfully, in which case it returns a proto with the
   *   Instance details, (b) the operation has failed, in which case the future
   *   contains an exception (typically `bigtable::GrpcError`) with the details
   *   of the failure, or (c) the state of the operation is unknown after the
   *   time allocated by the retry policies has expired, in which case the
   *   future contains an exception of type `bigtable::PollTimeout`.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc update instance
   */
  future<StatusOr<google::bigtable::admin::v2::Instance>> UpdateInstance(
      InstanceUpdateConfig instance_update_config);

  /**
   * Update an existing instance (asynchronously).
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param instance_update_config config with modified instance.
   *
   * @return a future satisfied when either (a) the instance is updated or (b)
   *     an unretriable error occurs or (c) polling or retry policy has been
   *     exhausted.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async update instance
   */
  future<StatusOr<google::bigtable::admin::v2::Instance>> AsyncUpdateInstance(
      CompletionQueue& cq, InstanceUpdateConfig instance_update_config);

  /**
   * Obtain the list of instances in the project.
   *
   * @note In some circumstances Cloud Bigtable may be unable to obtain the full
   *   list of instances, typically because some transient failure has made
   *   specific zones unavailable. In this cases the service returns a separate
   *   list of `failed_locations` that represent the unavailable zones.
   *   Applications may want to retry the operation after the transient
   *   conditions have cleared.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc list instances
   */
  StatusOr<InstanceList> ListInstances();

  /**
   * Query (asynchronously) the list of instances in the project.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @note In some circumstances Cloud Bigtable may be unable to obtain the full
   *   list of instances, typically because some transient failure has made
   *   specific zones unavailable. In this cases the service returns a separate
   *   list of `failed_locations` that represent the unavailable zones.
   *   Applications may want to retry the operation after the transient
   *   conditions have cleared.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @return the list of instances. It is possible that some zones are not
   * currently available for querying. In that case this function returns the
   * list of failed locations in the `projects/<project>/locations/<zone_id>`
   * format.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async list instances
   */
  future<StatusOr<InstanceList>> AsyncListInstances(CompletionQueue& cq);

  /**
   * Return the details of @p instance_id.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc get instance
   */
  StatusOr<google::bigtable::admin::v2::Instance> GetInstance(
      std::string const& instance_id);

  /**
   * Sends an asynchronous request to get information about an existing
   * instance.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_id the id of the instance in the project that to be
   *     retrieved.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   *
   * @return a future that will be satisfied when the request succeeds or the
   *   retry policy expires. In the first case, the future will contain the
   *   response from the service. In the second the future is satisfied with
   *   an exception.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async get instance
   */
  future<StatusOr<google::bigtable::admin::v2::Instance>> AsyncGetInstance(
      CompletionQueue& cq, std::string const& instance_id);

  /**
   * Deletes the instances in the project.
   *
   * @param instance_id the id of the instance in the project that needs to be
   * deleted
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc delete instance
   */
  Status DeleteInstance(std::string const& instance_id);

  /**
   * Makes an asynchronous request to delete an instance.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_id the id of the instance in the project to be deleted.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async-delete-instance
   */
  future<Status> AsyncDeleteInstance(std::string const& instance_id,
                                     CompletionQueue& cq);

  /**
   * Obtain the list of clusters in an instance.
   *
   * @note In some circumstances Cloud Bigtable may be unable to obtain the full
   *   list of clusters, typically because some transient failure has made
   *   specific zones unavailable. In this cases the service returns a separate
   *   list of `failed_locations` that represent the unavailable zones.
   *   Applications may want to retry the operation after the transient
   *   conditions have cleared.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc list clusters
   */
  StatusOr<ClusterList> ListClusters();

  /**
   * Obtain the list of clusters in an instance.
   *
   * @note In some circumstances Cloud Bigtable may be unable to obtain the full
   *   list of clusters, typically because some transient failure has made
   *   specific zones unavailable. In this cases the service returns a separate
   *   list of `failed_locations` that represent the unavailable zones.
   *   Applications may want to retry the operation after the transient
   *   conditions have cleared.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc list clusters
   */
  StatusOr<ClusterList> ListClusters(std::string const& instance_id);

  /**
   * Query (asynchronously) the list of clusters in a project.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @note In some circumstances Cloud Bigtable may be unable to obtain the full
   *   list of clusters, typically because some transient failure has made
   *   specific zones unavailable. In this cases the service returns a separate
   *   list of `failed_locations` that represent the unavailable zones.
   *   Applications may want to retry the operation after the transient
   *   conditions have cleared.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @return the list of clusters. It is possible that some zones are not
   *     currently available for querying. In that case this function returns
   *     the list of failed locations in the
   *     `projects/<project>/locations/<zone_id>` format.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc list clusters
   */
  future<StatusOr<ClusterList>> AsyncListClusters(CompletionQueue& cq);

  /**
   * Query (asynchronously) the list of clusters in an instance.
   *
   * @note In some circumstances Cloud Bigtable may be unable to obtain the full
   *   list of clusters, typically because some transient failure has made
   *   specific zones unavailable. In this cases the service returns a separate
   *   list of `failed_locations` that represent the unavailable zones.
   *   Applications may want to retry the operation after the transient
   *   conditions have cleared.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param instance_id the instance in a project.
   * @return the list of clusters. It is possible that some zones are not
   *     currently available for querying. In that case this function returns
   *     the list of failed locations in the
   *     `projects/<project>/locations/<zone_id>` format.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc list clusters
   */
  future<StatusOr<ClusterList>> AsyncListClusters(
      CompletionQueue& cq, std::string const& instance_id);

  /**
   * Update an existing cluster of Cloud Bigtable.
   *
   * @warning Note that this is operation can take seconds or minutes to
   * complete. The application may prefer to perform other work while waiting
   * for this operation.
   *
   * @param cluster_config cluster with updated values.
   * @return a future that becomes satisfied when (a) the operation has
   *   completed successfully, in which case it returns a proto with the
   *   Instance details, (b) the operation has failed, in which case the future
   *   contains an exception (typically `bigtable::GrpcError`) with the details
   *   of the failure, or (c) the state of the operation is unknown after the
   *   time allocated by the retry policies has expired, in which case the
   *   future contains an exception of type `bigtable::PollTimeout`.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc update cluster
   */
  future<StatusOr<google::bigtable::admin::v2::Cluster>> UpdateCluster(
      ClusterConfig cluster_config);

  /**
   * Update an existing cluster (asynchronously).
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param cluster_config cluster with updated values.
   *
   * @return a future satisfied when either (a) the cluster is updated or (b)
   *     an unretriable error occurs or (c) polling or retry policy has been
   *     exhausted.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async update cluster
   */
  future<StatusOr<google::bigtable::admin::v2::Cluster>> AsyncUpdateCluster(
      CompletionQueue& cq, ClusterConfig cluster_config);

  /**
   * Deletes the specified cluster of an instance in the project.
   *
   * @param instance_id the id of the instance in the project
   * @param cluster_id the id of the cluster in the project that needs to be
   *   deleted
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc delete cluster
   */
  Status DeleteCluster(std::string const& instance_id,
                       std::string const& cluster_id);

  /**
   * Sends an asynchronous request to delete a cluster.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param instance_id the id of the instance in the project.
   * @param cluster_id the id of the cluster in the project that needs to be
   *     deleted.
   *
   * @return a future that will be satisfied when the request succeeds or the
   *     retry policy expires.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async delete cluster
   */
  future<Status> AsyncDeleteCluster(CompletionQueue& cq,
                                    std::string const& instance_id,
                                    std::string const& cluster_id);

  /**
   * Gets the specified cluster of an instance in the project.
   *
   * @param instance_id the id of the instance in the project
   * @param cluster_id the id of the cluster in the project that needs to be
   *   deleted
   * @return a Cluster for given instance_id and cluster_id.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc get cluster
   */
  StatusOr<google::bigtable::admin::v2::Cluster> GetCluster(
      std::string const& instance_id, std::string const& cluster_id);

  /**
   * Sends an asynchronous request to get information about existing cluster of
   * an instance.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_id the id of the instance in the project.
   * @param cluster_id the id of the cluster in the project that needs to be
   * retrieved.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   *
   * @return a future that will be satisfied when the request succeeds or the
   *   retry policy expires. In the first case, the future will contain the
   *   response from the service. In the second the future is satisfied with
   *   an exception.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async get cluster
   */
  future<StatusOr<google::bigtable::admin::v2::Cluster>> AsyncGetCluster(
      CompletionQueue& cq, std::string const& instance_id,
      std::string const& cluster_id);

  /**
   * Create a new application profile.
   *
   * @param instance_id the instance for the new application profile.
   * @param config the configuration for the new application profile.
   * @return The proto describing the new application profile.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Multi-cluster Routing Example
   * @snippet bigtable_instance_admin_snippets.cc create app profile
   *
   * @par Single Cluster Routing Example
   * @snippet bigtable_instance_admin_snippets.cc create app profile cluster
   */
  StatusOr<google::bigtable::admin::v2::AppProfile> CreateAppProfile(
      std::string const& instance_id, AppProfileConfig config);

  /**
   * Asynchronously create a new application profile.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param instance_id the instance for the new application profile.
   * @param config the configuration for the new application profile.
   * @return The proto describing the new application profile.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async create app profile
   */
  future<StatusOr<google::bigtable::admin::v2::AppProfile>>
  AsyncCreateAppProfile(CompletionQueue& cq, std::string const& instance_id,
                        AppProfileConfig config);

  /**
   * Fetch the detailed information about an existing application profile.
   *
   * @param instance_id the instance to look the profile in.
   * @param profile_id the id of the profile within that instance.
   * @return The proto describing the application profile.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc get app profile
   */
  StatusOr<google::bigtable::admin::v2::AppProfile> GetAppProfile(
      std::string const& instance_id, std::string const& profile_id);

  /**
   * Asynchronously fetch the information about an existing application profile.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param instance_id the instance to look the profile in.
   * @param profile_id the id of the profile within that instance.
   * @return a future satisfied when either (a) the profile is fetched or (b)
   *     an unretriable error occurs or (c) retry policy has been exhausted.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async get app profile
   */
  future<StatusOr<google::bigtable::admin::v2::AppProfile>> AsyncGetAppProfile(
      CompletionQueue& cq, std::string const& instance_id,
      std::string const& profile_id);

  /**
   * Updates an existing application profile.
   *
   * @param instance_id the instance for the new application profile.
   * @param profile_id the id (not the full name) of the profile to update.
   * @param config the configuration for the new application profile.
   * @return The proto describing the new application profile.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Change Description Example
   * @snippet bigtable_instance_admin_snippets.cc update app profile description
   *
   * @par Change Routing to Any Cluster Example
   * @snippet bigtable_instance_admin_snippets.cc update app profile routing any
   *
   * @par Change Routing to a Specific Cluster Example
   * @snippet bigtable_instance_admin_snippets.cc update app profile routing
   */
  future<StatusOr<google::bigtable::admin::v2::AppProfile>> UpdateAppProfile(
      std::string const& instance_id, std::string const& profile_id,
      AppProfileUpdateConfig config);

  /**
   * Updates an application profile (asynchronously).
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param instance_id the instance for the new application profile.
   * @param profile_id the id (not the full name) of the profile to update.
   * @param config the configuration for the new application profile.
   *
   * @return a future satisfied when either (a) the profile is updated or (b)
   *     an unretriable error occurs or (c) polling or retry policy has been
   *     exhausted.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async update app profile
   */
  future<StatusOr<google::bigtable::admin::v2::AppProfile>>
  AsyncUpdateAppProfile(CompletionQueue& cq, std::string const& instance_id,
                        std::string const& profile_id,
                        AppProfileUpdateConfig config);

  /**
   * List the application profiles in an instance.
   *
   * @param instance_id the instance to list the profiles for.
   * @return a std::vector with the protos describing any profiles.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc list app profiles
   */
  StatusOr<std::vector<google::bigtable::admin::v2::AppProfile>>
  ListAppProfiles(std::string const& instance_id);

  /**
   * Query (asynchronously) the list of app profiles in an instance.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param instance_id the instance in a project.
   * @return the list of app profiles.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc list clusters
   */
  future<StatusOr<std::vector<google::bigtable::admin::v2::AppProfile>>>
  AsyncListAppProfiles(CompletionQueue& cq, std::string const& instance_id);

  /**
   * Delete an existing application profile.
   *
   * @param instance_id the instance to look the profile in.
   * @param profile_id the id of the profile within that instance.
   * @param ignore_warnings if true, ignore safety checks when deleting the
   *     application profile. This value is to to `true` by default. Passing
   *     `false` causes this function to fail even when no operations are
   *     pending.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc delete app profile
   */
  Status DeleteAppProfile(std::string const& instance_id,
                          std::string const& profile_id,
                          bool ignore_warnings = true);

  /**
   * Asynchronously delete an existing application profile.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param instance_id the instance to look the profile in.
   * @param profile_id the id of the profile within that instance.
   * @param ignore_warnings if true, ignore safety checks when deleting the
   *     application profile. This value is to to `true` by default. Passing
   *     `false` causes this function to fail even when no operations are
   *     pending.
   *
   * @return a future satisfied when either (a) the app profile is deleted or
   *     (b) an unretriable error occurs or (c) retry policy has been exhausted.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async delete app profile
   */
  future<Status> AsyncDeleteAppProfile(CompletionQueue& cq,
                                       std::string const& instance_id,
                                       std::string const& profile_id,
                                       bool ignore_warnings = true);

  /**
   * Gets the policy for @p instance_id.
   *
   * @param instance_id the instance to query.
   * @return Policy the full IAM policy for the instance.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc get iam policy
   */
  StatusOr<google::cloud::IamPolicy> GetIamPolicy(
      std::string const& instance_id);

  /**
   * Asynchronously gets the policy for @p instance_id.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param instance_id the instance to query.
   * @return a future satisfied when either (a) the policy is fetched or (b)
   *     an unretriable error occurs or (c) retry policy has been exhausted.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async get iam policy
   */
  future<StatusOr<google::cloud::IamPolicy>> AsyncGetIamPolicy(
      CompletionQueue& cq, std::string const& instance_id);

  /**
   * Sets the IAM policy for an instance.
   *
   * Applications can provide the @p etag to implement optimistic concurrency
   * control. If @p etag is not empty, the server will reject calls where the
   * provided ETag does not match the ETag value stored in the server.
   *
   * @param instance_id which instance to set the IAM policy for.
   * @param iam_bindings IamBindings object containing role and members.
   * @param etag the expected ETag value for the current policy.
   * @return Policy the current IAM bindings for the instance.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc set iam policy
   */
  StatusOr<google::cloud::IamPolicy> SetIamPolicy(
      std::string const& instance_id,
      google::cloud::IamBindings const& iam_bindings,
      std::string const& etag = std::string{});

  /**
   * Asynchronously sets the IAM policy for an instance.
   *
   * Applications can provide the @p etag to implement optimistic concurrency
   * control. If @p etag is not empty, the server will reject calls where the
   * provided ETag does not match the ETag value stored in the server.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param instance_id which instance to set the IAM policy for.
   * @param iam_bindings IamBindings object containing role and members.
   * @param etag the expected ETag value for the current policy.
   * @return a future satisfied when either (a) the policy is created or (b)
   *     an unretriable error occurs or (c) retry policy has been
   *     exhausted.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async set iam policy
   */
  future<StatusOr<google::cloud::IamPolicy>> AsyncSetIamPolicy(
      CompletionQueue& cq, std::string const& instance_id,
      google::cloud::IamBindings const& iam_bindings,
      std::string const& etag = std::string{});

  /**
   * Returns a permission set that the caller has on the specified instance.
   *
   * @param instance_id the ID of the instance to query.
   * @param permissions set of permissions to check for the resource.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc test iam permissions
   *
   * @see https://cloud.google.com/bigtable/docs/access-control for a list of
   *     valid permissions on Google Cloud Bigtable.
   */
  StatusOr<std::vector<std::string>> TestIamPermissions(
      std::string const& instance_id,
      std::vector<std::string> const& permissions);

  /**
   * Asynchronously obtains a permission set that the caller has on the
   * specified instance.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param instance_id the ID of the instance to query.
   * @param permissions set of permissions to check for the resource.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async test iam permissions
   *
   * @see https://cloud.google.com/bigtable/docs/access-control for a list of
   *     valid permissions on Google Cloud Bigtable.
   */
  future<StatusOr<std::vector<std::string>>> AsyncTestIamPermissions(
      CompletionQueue& cq, std::string const& instance_id,
      std::vector<std::string> const& permissions);

 private:
  static inline google::cloud::IamPolicy ProtoToWrapper(
      google::iam::v1::Policy proto) {
    google::cloud::IamPolicy result;
    result.version = proto.version();
    result.etag = std::move(*proto.mutable_etag());
    for (auto& binding : *proto.mutable_bindings()) {
      for (auto& member : *binding.mutable_members()) {
        result.bindings.AddMember(binding.role(), std::move(member));
      }
    }

    return result;
  }

  std::unique_ptr<PollingPolicy> clone_polling_policy() {
    return polling_policy_->clone();
  }

  std::unique_ptr<RPCRetryPolicy> clone_rpc_retry_policy() {
    return rpc_retry_policy_->clone();
  }

  std::unique_ptr<RPCBackoffPolicy> clone_rpc_backoff_policy() {
    return rpc_backoff_policy_->clone();
  }

  MetadataUpdatePolicy clone_metadata_update_policy() {
    return metadata_update_policy_;
  }

  std::shared_ptr<InstanceAdminClient> client_;
  std::string project_name_;
  std::shared_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::shared_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  std::shared_ptr<PollingPolicy> polling_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_ADMIN_H_
