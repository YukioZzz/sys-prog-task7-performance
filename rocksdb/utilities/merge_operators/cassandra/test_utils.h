// Copyright (c) 2017-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
// This source code is also licensed under the GPLv2 license found in the
// COPYING file in the root directory of this source tree.

#pragma once
#include <memory>
#include "util/testharness.h"
#include "utilities/merge_operators/cassandra/format.h"
#include "utilities/merge_operators/cassandra/serialize.h"

namespace rocksdb
{
namespace cassandra
{
extern const char kData[];
extern const char kExpiringData[];
extern const int32_t kLocalDeletionTime;
extern const int32_t kTtl;
extern const int8_t kColumn;
extern const int8_t kTombstone;
extern const int8_t kExpiringColumn;

std::unique_ptr<ColumnBase> CreateTestColumn(int8_t mask, int8_t index,
					     int64_t timestamp);

RowValue CreateTestRowValue(
	std::vector<std::tuple<int8_t, int8_t, int64_t> > column_specs);

RowValue CreateRowTombstone(int64_t timestamp);

void VerifyRowValueColumns(std::vector<std::unique_ptr<ColumnBase> > &columns,
			   std::size_t index_of_vector, int8_t expected_mask,
			   int8_t expected_index, int64_t expected_timestamp);

} // namespace cassandra
} // namespace rocksdb
