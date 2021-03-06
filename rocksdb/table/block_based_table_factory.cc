//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "table/block_based_table_factory.h"

#include <memory>
#include <string>
#include <stdint.h>

#include "port/port.h"
#include "rocksdb/flush_block_policy.h"
#include "rocksdb/cache.h"
#include "table/block_based_table_builder.h"
#include "table/block_based_table_reader.h"
#include "table/format.h"

namespace rocksdb
{
BlockBasedTableFactory::BlockBasedTableFactory(
	const BlockBasedTableOptions &_table_options)
	: table_options_(_table_options)
{
	if (table_options_.flush_block_policy_factory == nullptr) {
		table_options_.flush_block_policy_factory.reset(
			new FlushBlockBySizePolicyFactory());
	}
	if (table_options_.no_block_cache) {
		table_options_.block_cache.reset();
	} else if (table_options_.block_cache == nullptr) {
		table_options_.block_cache = NewLRUCache(8 << 20);
	}
	if (table_options_.block_size_deviation < 0 ||
	    table_options_.block_size_deviation > 100) {
		table_options_.block_size_deviation = 0;
	}
	if (table_options_.block_restart_interval < 1) {
		table_options_.block_restart_interval = 1;
	}
	if (table_options_.index_block_restart_interval < 1) {
		table_options_.index_block_restart_interval = 1;
	}
}

Status BlockBasedTableFactory::NewTableReader(
	const TableReaderOptions &table_reader_options,
	unique_ptr<RandomAccessFileReader> &&file, uint64_t file_size,
	unique_ptr<TableReader> *table_reader,
	bool prefetch_index_and_filter_in_cache) const
{
	return BlockBasedTable::Open(
		table_reader_options.ioptions, table_reader_options.env_options,
		table_options_, table_reader_options.internal_comparator,
		std::move(file), file_size, table_reader,
		prefetch_index_and_filter_in_cache,
		table_reader_options.skip_filters, table_reader_options.level);
}

TableBuilder *BlockBasedTableFactory::NewTableBuilder(
	const TableBuilderOptions &table_builder_options,
	uint32_t column_family_id, WritableFileWriter *file) const
{
	auto table_builder = new BlockBasedTableBuilder(
		table_builder_options.ioptions, table_options_,
		table_builder_options.internal_comparator,
		table_builder_options.int_tbl_prop_collector_factories,
		column_family_id, file, table_builder_options.compression_type,
		table_builder_options.compression_opts,
		table_builder_options.compression_dict,
		table_builder_options.skip_filters,
		table_builder_options.column_family_name);

	return table_builder;
}

Status BlockBasedTableFactory::SanitizeOptions(
	const DBOptions &db_opts, const ColumnFamilyOptions &cf_opts) const
{
	if (table_options_.index_type == BlockBasedTableOptions::kHashSearch &&
	    cf_opts.prefix_extractor == nullptr) {
		return Status::InvalidArgument(
			"Hash index is specified for block-based "
			"table, but prefix_extractor is not given");
	}
	if (table_options_.cache_index_and_filter_blocks &&
	    table_options_.no_block_cache) {
		return Status::InvalidArgument(
			"Enable cache_index_and_filter_blocks, "
			", but block cache is disabled");
	}
	if (table_options_.pin_l0_filter_and_index_blocks_in_cache &&
	    table_options_.no_block_cache) {
		return Status::InvalidArgument(
			"Enable pin_l0_filter_and_index_blocks_in_cache, "
			", but block cache is disabled");
	}
	if (!BlockBasedTableSupportedVersion(table_options_.format_version)) {
		return Status::InvalidArgument(
			"Unsupported BlockBasedTable format_version. Please check "
			"include/rocksdb/table.h for more info");
	}
	return Status::OK();
}

std::string BlockBasedTableFactory::GetPrintableTableOptions() const
{
	std::string ret;
	ret.reserve(20000);
	const int kBufferSize = 200;
	char buffer[kBufferSize];

	snprintf(buffer, kBufferSize, "  flush_block_policy_factory: %s (%p)\n",
		 table_options_.flush_block_policy_factory->Name(),
		 static_cast<void *>(
			 table_options_.flush_block_policy_factory.get()));
	ret.append(buffer);
	snprintf(buffer, kBufferSize, "  cache_index_and_filter_blocks: %d\n",
		 table_options_.cache_index_and_filter_blocks);
	ret.append(buffer);
	snprintf(
		buffer, kBufferSize,
		"  cache_index_and_filter_blocks_with_high_priority: %d\n",
		table_options_.cache_index_and_filter_blocks_with_high_priority);
	ret.append(buffer);
	snprintf(buffer, kBufferSize,
		 "  pin_l0_filter_and_index_blocks_in_cache: %d\n",
		 table_options_.pin_l0_filter_and_index_blocks_in_cache);
	ret.append(buffer);
	snprintf(buffer, kBufferSize, "  index_type: %d\n",
		 table_options_.index_type);
	ret.append(buffer);
	snprintf(buffer, kBufferSize, "  hash_index_allow_collision: %d\n",
		 table_options_.hash_index_allow_collision);
	ret.append(buffer);
	snprintf(buffer, kBufferSize, "  checksum: %d\n",
		 table_options_.checksum);
	ret.append(buffer);
	snprintf(buffer, kBufferSize, "  no_block_cache: %d\n",
		 table_options_.no_block_cache);
	ret.append(buffer);
	snprintf(buffer, kBufferSize, "  block_cache: %p\n",
		 static_cast<void *>(table_options_.block_cache.get()));
	ret.append(buffer);
	if (table_options_.block_cache) {
		const char *block_cache_name =
			table_options_.block_cache->Name();
		if (block_cache_name != nullptr) {
			snprintf(buffer, kBufferSize,
				 "  block_cache_name: %s\n", block_cache_name);
			ret.append(buffer);
		}
		ret.append("  block_cache_options:\n");
		ret.append(table_options_.block_cache->GetPrintableOptions());
	}
	snprintf(buffer, kBufferSize, "  block_cache_compressed: %p\n",
		 static_cast<void *>(
			 table_options_.block_cache_compressed.get()));
	ret.append(buffer);
	if (table_options_.block_cache_compressed) {
		const char *block_cache_compressed_name =
			table_options_.block_cache_compressed->Name();
		if (block_cache_compressed_name != nullptr) {
			snprintf(buffer, kBufferSize,
				 "  block_cache_name: %s\n",
				 block_cache_compressed_name);
			ret.append(buffer);
		}
		ret.append("  block_cache_compressed_options:\n");
		ret.append(table_options_.block_cache_compressed
				   ->GetPrintableOptions());
	}
	snprintf(buffer, kBufferSize, "  persistent_cache: %p\n",
		 static_cast<void *>(table_options_.persistent_cache.get()));
	ret.append(buffer);
	if (table_options_.persistent_cache) {
		snprintf(buffer, kBufferSize, "  persistent_cache_options:\n");
		ret.append(buffer);
		ret.append(
			table_options_.persistent_cache->GetPrintableOptions());
	}
	snprintf(buffer, kBufferSize, "  block_size: %" ROCKSDB_PRIszt "\n",
		 table_options_.block_size);
	ret.append(buffer);
	snprintf(buffer, kBufferSize, "  block_size_deviation: %d\n",
		 table_options_.block_size_deviation);
	ret.append(buffer);
	snprintf(buffer, kBufferSize, "  block_restart_interval: %d\n",
		 table_options_.block_restart_interval);
	ret.append(buffer);
	snprintf(buffer, kBufferSize, "  index_block_restart_interval: %d\n",
		 table_options_.index_block_restart_interval);
	ret.append(buffer);
	snprintf(buffer, kBufferSize, "  filter_policy: %s\n",
		 table_options_.filter_policy == nullptr ?
			       "nullptr" :
			       table_options_.filter_policy->Name());
	ret.append(buffer);
	snprintf(buffer, kBufferSize, "  whole_key_filtering: %d\n",
		 table_options_.whole_key_filtering);
	ret.append(buffer);
	snprintf(buffer, kBufferSize, "  format_version: %d\n",
		 table_options_.format_version);
	ret.append(buffer);
	return ret;
}

const BlockBasedTableOptions &BlockBasedTableFactory::table_options() const
{
	return table_options_;
}

TableFactory *
NewBlockBasedTableFactory(const BlockBasedTableOptions &_table_options)
{
	return new BlockBasedTableFactory(_table_options);
}

const std::string BlockBasedTablePropertyNames::kIndexType =
	"rocksdb.block.based.table.index.type";
const std::string BlockBasedTablePropertyNames::kWholeKeyFiltering =
	"rocksdb.block.based.table.whole.key.filtering";
const std::string BlockBasedTablePropertyNames::kPrefixFiltering =
	"rocksdb.block.based.table.prefix.filtering";
const std::string kHashIndexPrefixesBlock = "rocksdb.hashindex.prefixes";
const std::string kHashIndexPrefixesMetadataBlock =
	"rocksdb.hashindex.metadata";
const std::string kPropTrue = "1";
const std::string kPropFalse = "0";

} // namespace rocksdb
