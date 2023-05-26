"""
Transitive dependencies.
"""

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")

def cel_spec_deps_extra():
    """CEL Spec dependencies."""
    go_repository(
        name = "org_golang_google_genproto",
        build_file_proto_mode = "disable_global",
        commit = "69f6226f97e558dbaa68715071622af0d86b3a17",
        importpath = "google.golang.org/genproto",
    )

    go_repository(
        name = "org_golang_google_grpc",
        build_file_proto_mode = "disable_global",
        importpath = "google.golang.org/grpc",
        tag = "v1.49.0",
        build_directives = ["gazelle:resolve go google.golang.org/genproto/googleapis/rpc/status @org_golang_google_genproto//googleapis/rpc/status:status"],
    )

    go_repository(
        name = "org_golang_x_net",
        importpath = "golang.org/x/net",
        sum = "h1:oWX7TPOiFAMXLq8o0ikBYfCJVlRHBcsciT5bXOrH628=",
        version = "v0.0.0-20190311183353-d8887717615a",
    )

    go_repository(
        name = "org_golang_x_text",
        importpath = "golang.org/x/text",
        sum = "h1:tW2bmiBqwgJj/UpqtC8EpXEZVYOwU0yG4iWbprSVAcs=",
        version = "v0.3.2",
    )

    go_rules_dependencies()
    go_register_toolchains(version = "1.19.1")
    gazelle_dependencies()
    protobuf_deps()

def cel_cpp_deps_extra():
    """All transitive dependencies."""
    switched_rules_by_language(
        name = "com_google_googleapis_imports",
        cc = True,
        go = True,  # cel-spec requirement
    )
    cel_spec_deps_extra()