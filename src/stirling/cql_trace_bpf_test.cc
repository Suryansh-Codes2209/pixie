#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/common/base/test_utils.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/cql/types.h"
#include "src/stirling/data_table.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/testing/common.h"
#include "src/stirling/testing/socket_trace_bpf_test_fixture.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::FindRecordIdxMatchesPid;
using ::pl::stirling::testing::SocketTraceBPFTest;
using ::pl::types::ColumnWrapper;
using ::pl::types::ColumnWrapperRecordBatch;

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class CassandraContainer : public ContainerRunner {
 public:
  CassandraContainer()
      : ContainerRunner(kCassandraImage, kCassandraInstanceNamePrefix, kCassandraReadyMessage) {}

 private:
  static constexpr std::string_view kCassandraImage = "datastax/dse-server:6.7.7";
  static constexpr std::string_view kCassandraInstanceNamePrefix = "dse_server";
  static constexpr std::string_view kCassandraReadyMessage = "DSE startup complete.";
};

class CQLTraceTest : public SocketTraceBPFTest {
 protected:
  CQLTraceTest() {
    // Run the cassandra server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    // Note that this step will make an access to docker hub to  download the Cassandra image.
    PL_CHECK_OK(container_.Run(150, {"--env=DS_LICENSE=accept"}));
  }
  ~CQLTraceTest() { container_.Stop(); }

  CassandraContainer container_;
};

//-----------------------------------------------------------------------------
// Utility Functions and Matchers
//-----------------------------------------------------------------------------

std::vector<cass::Record> ToRecordVector(const types::ColumnWrapperRecordBatch& rb,
                                         const std::vector<size_t>& indices) {
  std::vector<cass::Record> result;

  for (const auto& idx : indices) {
    cass::Record r;
    r.req.op = static_cast<cass::ReqOp>(rb[kCQLReqOp]->Get<types::Int64Value>(idx).val);
    r.req.msg = rb[kCQLReqBody]->Get<types::StringValue>(idx);
    r.resp.op = static_cast<cass::RespOp>(rb[kCQLRespOp]->Get<types::Int64Value>(idx).val);
    r.resp.msg = rb[kCQLRespBody]->Get<types::StringValue>(idx);
    result.push_back(r);
  }
  return result;
}

auto EqCassReq(const cass::Request& x) {
  return AllOf(Field(&cass::Request::op, Eq(x.op)), Field(&cass::Request::msg, StrEq(x.msg)));
}

auto EqCassResp(const cass::Response& x) {
  return AllOf(Field(&cass::Response::op, Eq(x.op)), Field(&cass::Response::msg, StrEq(x.msg)));
}

auto EqCassRecord(const cass::Record& x) {
  return AllOf(Field(&cass::Record::req, EqCassReq(x.req)),
               Field(&cass::Record::resp, EqCassResp(x.resp)));
}

//-----------------------------------------------------------------------------
// Expected Test Data
//-----------------------------------------------------------------------------

// Note that timestamps are specified just to keep GCC happy.

// clang-format off
cass::Record kRecord1 = {
  .req = {
    .op = cass::ReqOp::kStartup,
    .msg = R"({"CQL_VERSION":"3.0.0"})",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kReady,
    .msg = "",
    .timestamp_ns = 0,
  }
};
cass::Record kRecord2 = {
  .req = {
    .op = cass::ReqOp::kRegister,
    .msg = R"(["TOPOLOGY_CHANGE","STATUS_CHANGE","SCHEMA_CHANGE"])",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kReady,
    .msg = "",
    .timestamp_ns = 0,
  }
};
cass::Record kRecord3 = {
  .req = {
    .op = cass::ReqOp::kQuery,
    .msg = R"(SELECT * FROM system.peers)",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kResult,
    .msg = R"(Response type = ROWS
Number of columns = 20
["peer","data_center","dse_version","graph","host_id","jmx_port","native_transport_address","native_transport_port",)"
R"("native_transport_port_ssl","preferred_ip","rack","release_version","rpc_address","schema_version","server_id",)"
R"("storage_port","storage_port_ssl","tokens","workload","workloads"]
Number of rows = 0)",
    .timestamp_ns = 0,
  }
};

cass::Record kRecord4 = {
  .req = {
    .op = cass::ReqOp::kQuery,
    .msg = R"(SELECT * FROM system.local WHERE key='local')",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kResult,
    .msg = R"(Response type = ROWS
Number of columns = 28
["key","bootstrapped","broadcast_address","cluster_name","cql_version","data_center","dse_version",)"
R"("gossip_generation","graph","host_id","jmx_port","listen_address","native_protocol_version",)"
R"("native_transport_address","native_transport_port","native_transport_port_ssl","partitioner","rack",)"
R"("release_version","rpc_address","schema_version","server_id","storage_port","storage_port_ssl","tokens",)"
R"("truncated_at","workload","workloads"]
Number of rows = 1)",
    .timestamp_ns = 0,
  }
};
cass::Record kRecord5 = {
  .req = {
    .op = cass::ReqOp::kQuery,
    .msg = R"(SELECT * FROM system_schema.keyspaces)",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kResult,
    .msg = R"(Response type = ROWS
Number of columns = 3
["keyspace_name","durable_writes","replication"]
Number of rows = 13)",
    .timestamp_ns = 0,
  }
};
cass::Record kRecord6 = {
  .req = {
    .op = cass::ReqOp::kQuery,
    .msg = R"(SELECT * FROM system_schema.types)",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kResult,
    .msg = R"(Response type = ROWS
Number of columns = 4
["keyspace_name","type_name","field_names","field_types"]
Number of rows = 6)",
    .timestamp_ns = 0,
  }
};
cass::Record kRecord7 = {
  .req = {
    .op = cass::ReqOp::kQuery,
    .msg = R"(SELECT * FROM system_schema.functions)",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kResult,
    .msg = R"(Response type = ROWS
Number of columns = 11
["keyspace_name","function_name","argument_types","argument_names","body","called_on_null_input","deterministic",)"
R"("language","monotonic","monotonic_on","return_type"]
Number of rows = 0)",
    .timestamp_ns = 0,
  }
};
cass::Record kRecord8 = {
  .req = {
    .op = cass::ReqOp::kQuery,
    .msg = R"(SELECT * FROM system_schema.aggregates)",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kResult,
    .msg = R"(Response type = ROWS
Number of columns = 9
["keyspace_name","aggregate_name","argument_types","deterministic","final_func","initcond","return_type","state_func",)"
R"("state_type"]
Number of rows = 0)",
    .timestamp_ns = 0,
  }
};
cass::Record kRecord9 = {
  .req = {
    .op = cass::ReqOp::kQuery,
    .msg = R"(SELECT * FROM system_schema.tables)",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kResult,
    .msg = R"(Response type = ROWS
Number of columns = 21
["keyspace_name","table_name","bloom_filter_fp_chance","caching","cdc","comment","compaction","compression",)"
R"("crc_check_chance","dclocal_read_repair_chance","default_time_to_live","extensions","flags","gc_grace_seconds",)"
R"("id","max_index_interval","memtable_flush_period_in_ms","min_index_interval","nodesync",)"
R"("read_repair_chance","speculative_retry"]
Number of rows = 49)",
    .timestamp_ns = 0,
  }
};
cass::Record kRecord10 = {
  .req = {
    .op = cass::ReqOp::kQuery,
    .msg = R"(SELECT * FROM system_schema.triggers)",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kResult,
    .msg = R"(Response type = ROWS
Number of columns = 4
["keyspace_name","table_name","trigger_name","options"]
Number of rows = 0)",
    .timestamp_ns = 0,
  }
};
cass::Record kRecord11 = {
  .req = {
    .op = cass::ReqOp::kQuery,
    .msg = R"(SELECT * FROM system_schema.indexes)",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kResult,
    .msg = R"(Response type = ROWS
Number of columns = 5
["keyspace_name","table_name","index_name","kind","options"]
Number of rows = 0)",
    .timestamp_ns = 0,
  }
};
cass::Record kRecord12 = {
  .req = {
    .op = cass::ReqOp::kQuery,
    .msg = R"(SELECT * FROM system_schema.views)",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kResult,
    .msg = R"(Response type = ROWS
Number of columns = 25
["keyspace_name","view_name","base_table_id","base_table_name","bloom_filter_fp_chance","caching","cdc","comment",)"
R"("compaction","compression","crc_check_chance","dclocal_read_repair_chance","default_time_to_live","extensions",)"
R"("gc_grace_seconds","id","include_all_columns","max_index_interval","memtable_flush_period_in_ms",)"
R"("min_index_interval","nodesync","read_repair_chance","speculative_retry","version","where_clause"]
Number of rows = 0)",
    .timestamp_ns = 0,
  }
};
cass::Record kRecord13 = {
  .req = {
    .op = cass::ReqOp::kQuery,
    .msg = R"(SELECT * FROM system_schema.columns)",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kResult,
    .msg = R"(Response type = ROWS
Number of columns = 9
["keyspace_name","table_name","column_name","clustering_order","column_name_bytes","kind","position",)"
R"("required_for_liveness","type"]
Number of rows = 337)",
    .timestamp_ns = 0,
  }
};
cass::Record kRecord14 = {
  .req = {
    .op = cass::ReqOp::kQuery,
    .msg = R"(SELECT * from system_virtual_schema.keyspaces)",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kResult,
    .msg = R"(Response type = ROWS
Number of columns = 1
["keyspace_name"]
Number of rows = 2)",
    .timestamp_ns = 0,
  }
};
cass::Record kRecord15 = {
  .req = {
    .op = cass::ReqOp::kQuery,
    .msg = R"(SELECT * from system_virtual_schema.tables)",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kResult,
    .msg = R"(Response type = ROWS
Number of columns = 3
["keyspace_name","table_name","comment"]
Number of rows = 4)",
    .timestamp_ns = 0,
  }
};
cass::Record kRecord16 = {
  .req = {
    .op = cass::ReqOp::kQuery,
    .msg = R"(SELECT * from system_virtual_schema.columns)",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kResult,
    .msg = R"(Response type = ROWS
Number of columns = 8
["keyspace_name","table_name","column_name","clustering_order","column_name_bytes","kind","position","type"]
Number of rows = 19)",
    .timestamp_ns = 0,
  }
};
cass::Record kRecord17 = {
  .req = {
    .op = cass::ReqOp::kStartup,
    .msg = R"({"CQL_VERSION":"3.0.0"})",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kReady,
    .msg = "",
    .timestamp_ns = 0,
  }
};
cass::Record kRecord18 = {
  .req = {
    .op = cass::ReqOp::kQuery,
    .msg = R"(select * from system.local where key = 'local')",
    .timestamp_ns = 0,
  },
  .resp = {
    .op = cass::RespOp::kResult,
    .msg = R"(Response type = ROWS
Number of columns = 28
["key","bootstrapped","broadcast_address","cluster_name","cql_version","data_center","dse_version",)"
R"("gossip_generation","graph","host_id","jmx_port","listen_address","native_protocol_version",)"
R"("native_transport_address","native_transport_port","native_transport_port_ssl","partitioner","rack",)"
R"("release_version","rpc_address","schema_version","server_id","storage_port","storage_port_ssl","tokens",)"
R"("truncated_at","workload","workloads"]
Number of rows = 1)",
    .timestamp_ns = 0,
  }
};
// clang-format on

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

TEST_F(CQLTraceTest, cqlsh_capture) {
  // Sleep an additional second, just to be safe.
  sleep(1);

  // Run cqlsh as a way of generating traffic.
  // As part of it's startup code, it will perform a bunch of CQL transactions,
  // so we just tell it to quit after starting.
  // Run it through bash, and return the PID, so we can use it to filter captured results.
  std::string cmd = absl::StrFormat(
      "docker exec %s bash -c 'cqlsh --protocol-version 4 -e quit & echo $! && wait'",
      container_.container_name());
  ASSERT_OK_AND_ASSIGN(std::string out, pl::Exec(cmd));
  int32_t client_pid;
  ASSERT_TRUE(absl::SimpleAtoi(out, &client_pid));

  // Sleep a little more, just to be safe.
  sleep(2);

  // Grab the data from Stirling.
  DataTable data_table(kCQLTable);
  source_->TransferData(ctx_.get(), SocketTraceConnector::kCQLTableNum, &data_table);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  // Check client-side tracing results.
  {
    const std::vector<size_t> target_record_indices =
        FindRecordIdxMatchesPid(record_batch, kCQLUPIDIdx, client_pid);

    // For Debug:
    for (const auto& idx : target_record_indices) {
      uint32_t pid = record_batch[kCQLUPIDIdx]->Get<types::UInt128Value>(idx).High64();
      int64_t req_op = record_batch[kCQLReqOp]->Get<types::Int64Value>(idx).val;
      std::string req_body = record_batch[kCQLReqBody]->Get<types::StringValue>(idx);
      std::string resp_body = record_batch[kCQLRespBody]->Get<types::StringValue>(idx);
      VLOG(1) << absl::Substitute("$0 $1 $2 $3", pid, req_op, req_body, resp_body);
    }

    std::vector<cass::Record> records = ToRecordVector(record_batch, target_record_indices);

    EXPECT_THAT(records,
                UnorderedElementsAre(
                    EqCassRecord(kRecord1), EqCassRecord(kRecord2), EqCassRecord(kRecord3),
                    EqCassRecord(kRecord4), EqCassRecord(kRecord5), EqCassRecord(kRecord6),
                    EqCassRecord(kRecord7), EqCassRecord(kRecord8), EqCassRecord(kRecord9),
                    EqCassRecord(kRecord10), EqCassRecord(kRecord11), EqCassRecord(kRecord12),
                    EqCassRecord(kRecord13), EqCassRecord(kRecord14), EqCassRecord(kRecord15),
                    EqCassRecord(kRecord16), EqCassRecord(kRecord17), EqCassRecord(kRecord18)));
  }

  // Check server-side tracing results.
  {
    const std::vector<size_t> target_record_indices =
        FindRecordIdxMatchesPid(record_batch, kCQLUPIDIdx, container_.process_pid());

    // For Debug:
    for (const auto& idx : target_record_indices) {
      uint32_t pid = record_batch[kCQLUPIDIdx]->Get<types::UInt128Value>(idx).High64();
      int64_t req_op = record_batch[kCQLReqOp]->Get<types::Int64Value>(idx).val;
      std::string req_body = record_batch[kCQLReqBody]->Get<types::StringValue>(idx);
      std::string resp_body = record_batch[kCQLRespBody]->Get<types::StringValue>(idx);
      VLOG(1) << absl::Substitute("$0 $1 $2 $3", pid, req_op, req_body, resp_body);
    }

    std::vector<cass::Record> records = ToRecordVector(record_batch, target_record_indices);

    EXPECT_THAT(records,
                UnorderedElementsAre(
                    EqCassRecord(kRecord1), EqCassRecord(kRecord2), EqCassRecord(kRecord3),
                    EqCassRecord(kRecord4), EqCassRecord(kRecord5), EqCassRecord(kRecord6),
                    EqCassRecord(kRecord7), EqCassRecord(kRecord8), EqCassRecord(kRecord9),
                    EqCassRecord(kRecord10), EqCassRecord(kRecord11), EqCassRecord(kRecord12),
                    EqCassRecord(kRecord14), EqCassRecord(kRecord15), EqCassRecord(kRecord16),
                    EqCassRecord(kRecord17), EqCassRecord(kRecord18)));
  }

  // TODO(oazizi): Address the warning below.
  LOG(WARNING) << "Server-side tracing does not capture kRecord13 (which has a large result)."
                  "The reason is a BPF limitation on data larger than MAX_MSG_SIZE (30KiB),"
                  "This test generates a write() syscall with a packet that is 34982B large,"
                  "and that packet is not traced. When this limitation is resolved,"
                  "this test should be updated.";
}

}  // namespace stirling
}  // namespace pl
