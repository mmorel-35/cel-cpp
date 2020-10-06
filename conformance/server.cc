#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/conformance_service.grpc.pb.h"
#include "google/api/expr/v1alpha1/eval.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/api/expr/v1alpha1/value.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/rpc/code.pb.h"
#include "grpcpp/grpcpp.h"
#include "absl/strings/str_split.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "internal/proto_util.h"
#include "parser/parser.h"
#include "absl/status/statusor.h"


using ::grpc::Status;
using ::grpc::StatusCode;
using ::google::protobuf::Arena;

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {
CelValue ImportValue(Arena* arena, const v1alpha1::Value& value) {
  switch (value.kind_case()) {
    case v1alpha1::Value::KindCase::kNullValue:
      return CelValue::CreateNull();
    case v1alpha1::Value::KindCase::kBoolValue:
      return CelValue::CreateBool(value.bool_value());
    case v1alpha1::Value::KindCase::kInt64Value:
      return CelValue::CreateInt64(value.int64_value());
    case v1alpha1::Value::KindCase::kUint64Value:
      return CelValue::CreateUint64(value.uint64_value());
    case v1alpha1::Value::KindCase::kDoubleValue:
      return CelValue::CreateDouble(value.double_value());
    case v1alpha1::Value::KindCase::kStringValue:
      return CelValue::CreateString(&value.string_value());
    case v1alpha1::Value::KindCase::kBytesValue:
      return CelValue::CreateBytes(&value.bytes_value());
    case v1alpha1::Value::KindCase::kListValue: {
      std::vector<CelValue> list;
      int size = value.list_value().values_size();

      list.reserve(size);
      for (int i = 0; i < size; i++) {
        list.push_back(ImportValue(arena, value.list_value().values(i)));
      }

      return CelValue::CreateList(
          Arena::Create<ContainerBackedListImpl>(arena, list));
    }
    case v1alpha1::Value::KindCase::kMapValue: {
      std::vector<std::pair<CelValue, CelValue>> pairs;

      pairs.reserve(value.map_value().entries_size());
      for (const auto& entry : value.map_value().entries()) {
        auto key = ImportValue(arena, entry.key());
        if (!key.IsBool() && !key.IsInt64() && !key.IsUint64() &&
            !key.IsString()) {
          return CreateErrorValue(arena, "invalid key type in a map");
        }
        pairs.push_back({key, ImportValue(arena, entry.value())});
      }

      auto cel_map =
          CreateContainerBackedMap(absl::Span<std::pair<CelValue, CelValue>>(
              pairs.data(), pairs.size()));
      if (cel_map == nullptr) {
        return CreateErrorValue(arena, "invalid pairs in map constructor");
      }

      auto result = CelValue::CreateMap(cel_map.get());

      // Pass object ownership to Arena.
      arena->Own(cel_map.release());

      return result;
    }
    default:
      // unsupported values
      return CreateErrorValue(arena, "unsupported import value type");
  }
}
void ExportValue(const CelValue& result, v1alpha1::Value* out) {
  switch (result.type()) {
    case CelValue::Type::kBool:
      out->set_bool_value(result.BoolOrDie());
      break;
    case CelValue::Type::kInt64:
      out->set_int64_value(result.Int64OrDie());
      break;
    case CelValue::Type::kUint64:
      out->set_uint64_value(result.Uint64OrDie());
      break;
    case CelValue::Type::kDouble:
      out->set_double_value(result.DoubleOrDie());
      break;
    case CelValue::Type::kString:
      *out->mutable_string_value() = std::string(result.StringOrDie().value());
      break;
    case CelValue::Type::kBytes:
      *out->mutable_bytes_value() = std::string(result.BytesOrDie().value());
      break;
    case CelValue::Type::kMessage: {
      if (result.IsNull()) {
        out->set_null_value(google::protobuf::NullValue::NULL_VALUE);
      } else {
        auto msg = result.MessageOrDie();
        out->mutable_object_value()->PackFrom(*msg);
      }
      break;
    }
    case CelValue::Type::kDuration: {
      google::protobuf::Duration duration;
      expr::internal::EncodeDuration(result.DurationOrDie(), &duration);
      out->mutable_object_value()->PackFrom(duration);
      break;
    }
    case CelValue::Type::kTimestamp: {
      google::protobuf::Timestamp timestamp;
      expr::internal::EncodeTime(result.TimestampOrDie(), &timestamp);
      out->mutable_object_value()->PackFrom(timestamp);
      break;
    }
    case CelValue::Type::kList: {
      auto list = result.ListOrDie();
      auto values = out->mutable_list_value();
      for (int i = 0; i < list->size(); i++) {
        ExportValue((*list)[i], values->add_values());
      }
      break;
    }
    case CelValue::Type::kMap: {
      auto map = result.MapOrDie();
      auto list = map->ListKeys();
      auto values = out->mutable_map_value();
      for (int i = 0; i < list->size(); i++) {
        auto entry = values->add_entries();
        ExportValue((*list)[i], entry->mutable_key());
        ExportValue((*map)[(*list)[i]].value(), entry->mutable_value());
      }
      break;
    }
    case CelValue::Type::kUnknownSet:
    case CelValue::Type::kError:
    case CelValue::Type::kAny:
      // do nothing for special values
      break;
  }
}

}  // namespace

class ConformanceServiceImpl final
    : public v1alpha1::ConformanceService::Service {
 public:
  ConformanceServiceImpl(std::unique_ptr<CelExpressionBuilder> builder)
      : builder_(std::move(builder)) {}
  Status Parse(grpc::ServerContext* context,
               const v1alpha1::ParseRequest* request,
               v1alpha1::ParseResponse* response) override {
    if (request->cel_source().empty()) {
      return Status(StatusCode::INVALID_ARGUMENT, "No source code.");
    }
    auto parse_status = parser::Parse(request->cel_source(), "");
    if (!parse_status.ok()) {
      auto issue = response->add_issues();
      *issue->mutable_message() = std::string(parse_status.status().message());
      issue->set_code(google::rpc::Code::INVALID_ARGUMENT);
    } else {
      google::api::expr::v1alpha1::ParsedExpr out;
      (out).MergeFrom(parse_status.value());
      response->mutable_parsed_expr()->CopyFrom(out);
    }
    return Status::OK;
  }
  Status Check(grpc::ServerContext* context,
               const v1alpha1::CheckRequest* request,
               v1alpha1::CheckResponse* response) override {
    return Status(StatusCode::UNIMPLEMENTED, "Check is not supported");
  }
  Status Eval(grpc::ServerContext* context,
              const v1alpha1::EvalRequest* request,
              v1alpha1::EvalResponse* response) override {
    const v1alpha1::Expr* expr = nullptr;
    if (request->has_parsed_expr()) {
      expr = &request->parsed_expr().expr();
    } else if (request->has_checked_expr()) {
      expr = &request->checked_expr().expr();
    }

    Arena arena;
    google::api::expr::v1alpha1::SourceInfo source_info;
    google::api::expr::v1alpha1::Expr out;
    (out).MergeFrom(*expr);
    auto cel_expression_status = builder_->CreateExpression(&out, &source_info);

    if (!cel_expression_status.ok()) {
      return Status(StatusCode::INTERNAL,
                    std::string(cel_expression_status.status().message()));
    }

    auto cel_expression = std::move(cel_expression_status.value());
    Activation activation;

    for (const auto& pair : request->bindings()) {
      auto value = ImportValue(&arena, pair.second.value());
      activation.InsertValue(pair.first, value);
    }

    auto eval_status = cel_expression->Evaluate(activation, &arena);
    if (!eval_status.ok()) {
      return Status(StatusCode::INTERNAL,
                    std::string(eval_status.status().message()));
    }

    CelValue result = eval_status.value();
    if (result.IsError()) {
      *response->mutable_result()
           ->mutable_error()
           ->add_errors()
           ->mutable_message() = std::string(result.ErrorOrDie()->message());
    } else {
      ExportValue(result, response->mutable_result()->mutable_value());
    }
    return Status::OK;
  }

 private:
  std::unique_ptr<CelExpressionBuilder> builder_;
};

int RunServer(std::string server_address) {
  google::protobuf::Arena arena;
  InterpreterOptions options;

  const char* enable_constant_folding =
      getenv("CEL_CPP_ENABLE_CONSTANT_FOLDING");
  if (enable_constant_folding != nullptr) {
    options.constant_folding = true;
    options.constant_arena = &arena;
  }

  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);
  auto register_status = RegisterBuiltinFunctions(builder->GetRegistry());
  if (!register_status.ok()) {
    return 1;
  }

  ConformanceServiceImpl service(std::move(builder));
  grpc::ServerBuilder grpc_builder;
  int port;
  grpc_builder.AddListeningPort(server_address,
                                grpc::InsecureServerCredentials(), &port);
  grpc_builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server(grpc_builder.BuildAndStart());
  std::cout << "Listening on 127.0.0.1:" << port << std::endl;
  fflush(stdout);
  server->Wait();
  return 0;
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

int main(int argc, char** argv) {
  std::string server_address = "127.0.0.1:0";
  return google::api::expr::runtime::RunServer(server_address);
}
