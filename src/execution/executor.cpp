#include "execution/executor.h"

#include <stdexcept>
#include <utility>

#include "execution/aggregate.h"
#include "execution/hash_join.h"
#include "execution/interpreter/filter.h"
#include "execution/seq_scan.h"
#include "execution/vectorized/filter.h"

namespace minidb {
namespace {

std::vector<Predicate> FlattenOrThrow(const Expression& predicate) {
  std::vector<Predicate> predicates;
  if (!TryFlatten(predicate, predicates)) {
    throw std::logic_error("Executor: 這個條件超出 MVP 的 WHERE 文法，無法攤平");
  }
  return predicates;
}

}

Executor Executor::Build(const LogicalNode& plan, const Catalog& catalog, ExecMode mode,
                         SimdKernel kernel) {
  Executor executor;
  executor.root_ = executor.BuildNode(plan, catalog, mode, kernel);
  executor.num_output_columns_ = plan.schema().num_columns();
  return executor;
}

Operator* Executor::BuildNode(const LogicalNode& node, const Catalog& catalog, ExecMode mode,
                              SimdKernel kernel) {
  const std::uint32_t batch_size = BatchSizeFor(mode);

  switch (node.kind()) {
    case LogicalKind::kScan: {
      const auto& scan = static_cast<const LogicalScan&>(node);
      nodes_.push_back(
          std::make_unique<SeqScan>(catalog.GetTable(scan.table_name()), batch_size));
      return nodes_.back().get();
    }

    case LogicalKind::kFilter: {
      const auto& filter = static_cast<const LogicalFilter&>(node);
      Operator* child = BuildNode(filter.child(), catalog, mode, kernel);
      switch (mode) {
        case ExecMode::kInterpreter:
          nodes_.push_back(std::make_unique<InterpreterFilter>(*child, filter.predicate()));
          return nodes_.back().get();
        case ExecMode::kVectorized:
          nodes_.push_back(std::make_unique<VectorizedFilter>(
              *child, FlattenOrThrow(*filter.predicate()), kernel));
          return nodes_.back().get();
        case ExecMode::kJit:
          throw std::logic_error("Executor: JIT filter 要等 Phase 4");
      }
      throw std::logic_error("Executor: 未知的 ExecMode");
    }

    case LogicalKind::kJoin: {
      const auto& join = static_cast<const LogicalJoin&>(node);
      Operator* probe = BuildNode(join.left(), catalog, mode, kernel);
      Operator* build = BuildNode(join.right(), catalog, mode, kernel);

      HashJoinSpec spec;
      spec.probe_key = join.left_key();
      spec.build_key = join.right_key();
      spec.probe_num_cols = join.left().schema().num_columns();
      spec.build_num_cols = join.right().schema().num_columns();
      spec.batch_size = batch_size;
      spec.batched_probe = mode != ExecMode::kInterpreter;

      nodes_.push_back(std::make_unique<HashJoin>(*probe, *build, spec));
      return nodes_.back().get();
    }

    case LogicalKind::kAggregate: {
      const auto& agg = static_cast<const LogicalAggregate&>(node);
      Operator* child = BuildNode(agg.child(), catalog, mode, kernel);
      const SimdKernel agg_kernel =
          mode == ExecMode::kInterpreter ? SimdKernel::kScalar : kernel;
      nodes_.push_back(
          std::make_unique<Aggregate>(*child, agg.agg(), agg.column(), agg_kernel));
      return nodes_.back().get();
    }
  }

  throw std::logic_error("Executor: 未知的計畫節點");
}

std::vector<std::vector<std::int64_t>> Executor::RunRows() {
  std::vector<std::vector<std::int64_t>> rows;
  while (Batch* batch = root_->Next()) {
    for (std::uint32_t i = 0; i < batch->ActiveRows(); ++i) {
      std::vector<std::int64_t> row;
      row.reserve(batch->num_cols);
      for (std::uint32_t c = 0; c < batch->num_cols; ++c) {
        row.push_back(batch->Value(c, i));
      }
      rows.push_back(std::move(row));
    }
  }
  return rows;
}

std::int64_t Executor::RunScalar() {
  const std::vector<std::vector<std::int64_t>> rows = RunRows();
  if (rows.size() != 1 || rows[0].size() != 1) {
    throw std::logic_error("Executor::RunScalar: 這個計畫沒有回傳剛好一個值");
  }
  return rows[0][0];
}

}
