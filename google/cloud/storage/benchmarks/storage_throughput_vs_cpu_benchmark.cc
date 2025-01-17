// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/storage/client.h"
#include <future>
#include <iomanip>
#include <sstream>

namespace {
namespace gcs = google::cloud::storage;
namespace gcs_bm = google::cloud::storage_benchmarks;

char const kDescription[] = R"""(
A throughput vs. CPU benchmark for the Google Cloud Storage C++ client library.

This program measures the throughput and CPU utilization when uploading
and downloading relatively large (~250 MiB) objects Google Cloud Storage C++
client library. The program repeats the "experiment" of uploading, then
downloading, and then removing a file many times, using a randomly selected
size in each iteration. An external script performs statistical analysis on
the results to estimate likely values for the throughput and the CPU cost per
byte on both upload and download operations.

The program first creates a GCS bucket that will contain all the objects used
by that run of the program. The name of this bucket is selected at random, so
multiple copies of the program can run simultaneously. The bucket is deleted at
the end of the run of this program. The bucket uses the `REGIONAL` storage
class, in a region set via the command line. Choosing regions close to where the
program is running can be used to estimate the latency without any wide-area
network effects. Choosing regions far from where the program is running can be
used to evaluate the performance of the library when the WAN is taken into
account.

After creating this bucket the program creates a number of threads, configurable
via the command line, to obtain more samples in parallel. Configure this value
with a small enough number of threads such that you do not saturate the CPU.
Each thread creates a separate copy of the `storage::Client` object, and repeats
this loop until a prescribed *time* has elapsed:

- Select a random size, between two values configured in the command line of the
  object to upload.
- Select a random chunk size, between two values configured in the command line,
  the data is uploaded in chunks of this size.
- Upload an object of the selected size, choosing the name of the object at
  random.
- Once the object is fully uploaded, the program captures the object size, the
  chunk size, the elapsed time (in microseconds), the CPU time (in microseconds)
  used during the upload, and the status code for the upload.
- Then the program downloads the same object, and captures the object size, the
  chunk size, the elapsed time (in microseconds), the CPU time (in microseconds)
  used during the download, and the status code for the download.
- The program then deletes this object and starts another iteration.

The loop stops when any of the following conditions are met:

- The test has obtained more than a prescribed "maximum number of samples"
- The test has obtained at least a prescribed "minimum number of samples" *and*
  the test has been running for more than a prescribed "duration".

Once the threads finish running their loops the program prints the captured
performance data. The bucket is deleted after the program terminates.

A helper script in this directory can generate pretty graphs from the output of
this program.
)""";

struct Options {
  std::string project_id;
  std::string region;
  std::chrono::seconds duration =
      std::chrono::seconds(std::chrono::minutes(15));
  int thread_count = 1;
  std::int64_t minimum_object_size = 32 * gcs_bm::kMiB;
  std::int64_t maximum_object_size = 256 * gcs_bm::kMiB;
  std::int64_t minimum_chunk_size = 128 * gcs_bm::kKiB;
  std::int64_t maximum_chunk_size = 4096 * gcs_bm::kKiB;
  long minimum_sample_count = 0;
  long maximum_sample_count = std::numeric_limits<long>::max();
  bool disable_crc32c = false;
  bool disable_md5 = false;
};

enum OpType { OP_UPLOAD, OP_DOWNLOAD };
struct IterationResult {
  OpType op;
  std::uint64_t object_size;
  std::uint64_t chunk_size;
  std::uint64_t buffer_size;
  std::chrono::microseconds elapsed_time;
  std::chrono::microseconds cpu_time;
  google::cloud::StatusCode status;
};
using TestResults = std::vector<IterationResult>;

TestResults RunThread(Options const& options, std::string const& bucket_name);
void PrintResults(TestResults const& results);

Options ParseArgs(int argc, char* argv[]);

}  // namespace

int main(int argc, char* argv[]) try {
  Options options = ParseArgs(argc, argv);

  google::cloud::StatusOr<gcs::ClientOptions> client_options =
      gcs::ClientOptions::CreateDefaultClientOptions();
  if (!client_options) {
    std::cerr << "Could not create ClientOptions, status="
              << client_options.status() << "\n";
    return 1;
  }
  if (!options.project_id.empty()) {
    client_options->set_project_id(options.project_id);
  }
  gcs::Client client(*std::move(client_options));

  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  auto bucket_name =
      gcs_bm::MakeRandomBucketName(generator, "bm-throughput-vs-cpu-");
  auto meta =
      client
          .CreateBucket(bucket_name,
                        gcs::BucketMetadata()
                            .set_storage_class(gcs::storage_class::Regional())
                            .set_location(options.region),
                        gcs::PredefinedAcl("private"),
                        gcs::PredefinedDefaultObjectAcl("projectPrivate"),
                        gcs::Projection("full"))
          .value();
  std::cout << "# Running test on bucket: " << meta.name() << "\n";
  std::string notes = google::cloud::storage::version_string() + ";" +
                      google::cloud::internal::compiler() + ";" +
                      google::cloud::internal::compiler_flags();
  std::transform(notes.begin(), notes.end(), notes.begin(),
                 [](char c) { return c == '\n' ? ';' : c; });

  std::cout << "# Start time: "
            << google::cloud::internal::FormatRfc3339(
                   std::chrono::system_clock::now())
            << "\n# Region: " << options.region
            << "\n# Duration: " << options.duration.count() << "s"
            << "\n# Thread Count: " << options.thread_count
            << "\n# Min Object Size: " << options.minimum_object_size
            << "\n# Max Object Size: " << options.maximum_object_size
            << "\n# Min Chunk Size: " << options.minimum_chunk_size
            << "\n# Max Chunk Size: " << options.maximum_chunk_size
            << "\n# Min Object Size (MiB): "
            << options.minimum_object_size / gcs_bm::kMiB
            << "\n# Max Object Size (MiB): "
            << options.maximum_object_size / gcs_bm::kMiB
            << "\n# Min Chunk Size (KiB): "
            << options.minimum_chunk_size / gcs_bm::kKiB
            << "\n# Max Chunk Size (KiB): "
            << options.maximum_chunk_size / gcs_bm::kKiB << std::boolalpha
            << "\n# Disable CRC32C: " << options.disable_crc32c
            << "\n# Disable MD5: " << options.disable_md5
            << "\n# Build info: " << notes << "\n";
  // Make this immediately visible in the console, helps with debugging.
  std::cout << std::flush;

  std::vector<std::future<TestResults>> tasks;
  for (int i = 0; i != options.thread_count; ++i) {
    tasks.emplace_back(
        std::async(std::launch::async, RunThread, options, bucket_name));
  }
  for (auto& f : tasks) {
    PrintResults(f.get());
  }

  // Some of the downloads or deletes may have failed, delete any leftover
  // objects.
  std::cout << "# Deleting any leftover objects and the bucket\n";
  for (auto object : client.ListObjects(bucket_name, gcs::Versions(true))) {
    if (!object) {
      std::cout << "# Error listing objects: " << object.status() << "\n";
      break;
    }
    auto status = client.DeleteObject(object->bucket(), object->name(),
                                      gcs::Generation(object->generation()));
    if (!status.ok()) {
      std::cout << "# Error deleting object, name=" << object->name()
                << ", generation=" << object->generation()
                << ", status=" << status << "\n";
    }
  }
  auto status = client.DeleteBucket(bucket_name);
  if (!status.ok()) {
    std::cout << "# Error deleting bucket, status=" << status << "\n";
  }
  std::cout << "# DONE\n" << std::flush;

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}

namespace {
char const* ToString(OpType type) {
  switch (type) {
    case OP_DOWNLOAD:
      return "DOWNLOAD";
    case OP_UPLOAD:
      return "UPLOAD";
  }
  return nullptr;  // silence g++ error.
}

std::ostream& operator<<(std::ostream& os, IterationResult const& rhs) {
  return os << ToString(rhs.op) << ',' << rhs.object_size << ','
            << rhs.chunk_size << ',' << rhs.buffer_size << ','
            << rhs.elapsed_time.count() << ',' << rhs.cpu_time.count() << ','
            << rhs.status << ',' << google::cloud::storage::version_string();
}

void PrintResults(TestResults const& results) {
  for (auto& r : results) {
    std::cout << r << '\n';
  }
  std::cout << std::flush;
}

TestResults RunThread(Options const& options, std::string const& bucket_name) {
  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();
  auto contents =
      gcs_bm::MakeRandomData(generator, options.maximum_object_size);
  google::cloud::StatusOr<gcs::ClientOptions> client_options =
      gcs::ClientOptions::CreateDefaultClientOptions();
  if (!client_options) {
    std::cout << "# Could not create ClientOptions, status="
              << client_options.status() << "\n";
    return {};
  }
  std::uint64_t upload_buffer_size = client_options->upload_buffer_size();
  std::uint64_t download_buffer_size = client_options->download_buffer_size();
  gcs::Client client(*std::move(client_options));

  std::uniform_int_distribution<std::uint64_t> size_generator(
      options.minimum_object_size, options.maximum_object_size);
  std::uniform_int_distribution<std::uint64_t> chunk_generator(
      options.minimum_chunk_size, options.maximum_chunk_size);

  auto deadline = std::chrono::steady_clock::now() + options.duration;

  gcs_bm::SimpleTimer timer;
  // This obviously depends on the size of the objects, but a good estimate for
  // the upload + download bandwidth is 250MiB/s.
  constexpr long expected_bandwidth = 250 * gcs_bm::kMiB;
  auto const median_size =
      (options.minimum_object_size + options.minimum_object_size) / 2;
  auto const objects_per_second = median_size / expected_bandwidth;
  TestResults results;
  results.reserve(options.duration.count() * objects_per_second);

  long iteration_count = 0;
  for (auto start = std::chrono::steady_clock::now();
       iteration_count < options.maximum_sample_count &&
       (iteration_count < options.minimum_sample_count || start < deadline);
       start = std::chrono::steady_clock::now(), ++iteration_count) {
    auto object_name = gcs_bm::MakeRandomObjectName(generator);
    auto object_size = size_generator(generator);
    auto chunk_size = chunk_generator(generator);

    timer.Start();
    auto writer =
        client.WriteObject(bucket_name, object_name,
                           gcs::DisableCrc32cChecksum(options.disable_crc32c),
                           gcs::DisableMD5Hash(options.disable_md5));
    for (std::size_t offset = 0; offset < object_size; offset += chunk_size) {
      auto len = chunk_size;
      if (offset + len > object_size) {
        len = object_size - offset;
      }
      writer.write(contents.data() + offset, len);
    }
    writer.Close();
    timer.Stop();

    auto object_metadata = writer.metadata();
    results.emplace_back(IterationResult{OP_UPLOAD, object_size, chunk_size,
                                         download_buffer_size,
                                         timer.elapsed_time(), timer.cpu_time(),
                                         object_metadata.status().code()});

    if (!object_metadata) {
      continue;
    }

    timer.Start();
    auto reader =
        client.ReadObject(object_metadata->bucket(), object_metadata->name(),
                          gcs::Generation(object_metadata->generation()),
                          gcs::DisableCrc32cChecksum(options.disable_crc32c),
                          gcs::DisableMD5Hash(options.disable_md5));
    std::vector<char> buffer(chunk_size);
    while (reader.read(buffer.data(), buffer.size())) {
    }
    timer.Stop();
    results.emplace_back(IterationResult{
        OP_DOWNLOAD, object_size, chunk_size, upload_buffer_size,
        timer.elapsed_time(), timer.cpu_time(), reader.status().code()});

    auto status =
        client.DeleteObject(object_metadata->bucket(), object_metadata->name(),
                            gcs::Generation(object_metadata->generation()));

    if (options.thread_count == 1) {
      // Immediately print the results, this makes it easier to debug problems.
      PrintResults(results);
      results.clear();
    }
  }
  return results;
}

Options ParseArgs(int argc, char* argv[]) {
  Options options;
  bool wants_help = false;
  bool wants_description = false;
  std::vector<gcs_bm::OptionDescriptor> desc{
      {"--help", "print usage information",
       [&wants_help](std::string const&) { wants_help = true; }},
      {"--description", "print benchmark description",
       [&wants_description](std::string const&) { wants_description = true; }},
      {"--project-id", "use the given project id for the benchmark",
       [&options](std::string const& val) { options.project_id = val; }},
      {"--region", "use the given region for the benchmark",
       [&options](std::string const& val) { options.region = val; }},
      {"--thread-count", "set the number of threads in the benchmark",
       [&options](std::string const& val) {
         options.thread_count = std::stoi(val);
       }},
      {"--minimum-object-size", "configure the minimum object size in the test",
       [&options](std::string const& val) {
         options.minimum_object_size = gcs_bm::ParseSize(val);
       }},
      {"--maximum-object-size", "configure the maximum object size in the test",
       [&options](std::string const& val) {
         options.maximum_object_size = gcs_bm::ParseSize(val);
       }},
      {"--minimum-chunk-size", "configure the minimum chunk size in the test",
       [&options](std::string const& val) {
         options.minimum_chunk_size = gcs_bm::ParseSize(val);
       }},
      {"--maximum-chunk-size", "configure the maximum chunk size in the test",
       [&options](std::string const& val) {
         options.maximum_chunk_size = gcs_bm::ParseSize(val);
       }},
      {"--duration", "continue the test for at least this amount of time",
       [&options](std::string const& val) {
         options.duration = gcs_bm::ParseDuration(val);
       }},
      {"--minimum-sample-count",
       "continue the test until at least this number of samples are obtained",
       [&options](std::string const& val) {
         options.minimum_sample_count = std::stol(val);
       }},
      {"--maximum-sample-count",
       "stop the test when this number of samples are obtained",
       [&options](std::string const& val) {
         options.maximum_sample_count = std::stol(val);
       }},
      {"--disable-crc32", "disable CRC32C checksums",
       [&options](std::string const& val) {
         options.disable_crc32c = gcs_bm::ParseBoolean(val, true);
       }},
      {"--disable-md5", "disable MD5 hashes",
       [&options](std::string const& val) {
         options.disable_md5 = gcs_bm::ParseBoolean(val, true);
       }},
  };
  auto usage = gcs_bm::BuildUsage(desc, argv[0]);

  auto unparsed = gcs_bm::OptionsParse(desc, {argv, argv + argc});
  if (wants_help) {
    std::cout << usage << "\n";
  }

  if (wants_description) {
    std::cout << kDescription << "\n";
  }

  if (unparsed.size() > 2) {
    std::ostringstream os;
    os << "Unknown arguments or options\n" << usage << "\n";
    throw std::runtime_error(std::move(os).str());
  }
  if (unparsed.size() == 2) {
    options.region = unparsed[1];
  }
  if (options.region.empty()) {
    std::ostringstream os;
    os << "Missing value for --region option" << usage << "\n";
    throw std::runtime_error(std::move(os).str());
  }

  if (options.minimum_object_size > options.maximum_object_size) {
    std::ostringstream os;
    os << "Invalid range for object size [" << options.minimum_object_size
       << ',' << options.maximum_object_size << "]";
    throw std::runtime_error(std::move(os).str());
  }
  if (options.minimum_chunk_size > options.maximum_chunk_size) {
    std::ostringstream os;
    os << "Invalid range for chunk size [" << options.minimum_chunk_size << ','
       << options.maximum_chunk_size << "]";
    throw std::runtime_error(std::move(os).str());
  }
  if (options.minimum_sample_count > options.maximum_sample_count) {
    std::ostringstream os;
    os << "Invalid range for sample range [" << options.minimum_sample_count
       << ',' << options.maximum_sample_count << "]";
    throw std::runtime_error(std::move(os).str());
  }

  if (!gcs_bm::SimpleTimer::SupportPerThreadUsage() &&
      options.thread_count > 1) {
    std::ostringstream os;
    os << "Your platform does not support per-thread usage metrics"
          " (see getrusage(2)). Running more than one thread is not supported.";
    throw std::runtime_error(std::move(os).str());
  }

  return options;
}

}  // namespace
