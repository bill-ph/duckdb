//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parser/query_node/insert_query_node.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/parser/parsed_expression.hpp"
#include "duckdb/parser/query_node.hpp"
#include "duckdb/parser/statement/select_statement.hpp"
#include "duckdb/parser/tableref.hpp"

namespace duckdb {

class OnConflictInfo;
enum class InsertColumnOrder : uint8_t;

//! InsertQueryNode represents an INSERT statement with RETURNING used in a CTE
class InsertQueryNode : public QueryNode {
public:
	static constexpr const QueryNodeType TYPE = QueryNodeType::INSERT_QUERY_NODE;

public:
	InsertQueryNode();

	//! The select statement to insert from
	unique_ptr<SelectStatement> select_statement;
	//! Column names to insert into
	vector<string> columns;

	//! Table name to insert to
	string table;
	//! Schema name to insert to
	string schema;
	//! The catalog name to insert to
	string catalog;

	//! The RETURNING clause expressions
	vector<unique_ptr<ParsedExpression>> returning_list;

	//! ON CONFLICT info
	unique_ptr<OnConflictInfo> on_conflict_info;
	//! Table reference (for qualified names)
	unique_ptr<TableRef> table_ref;

	//! Whether or not this is a DEFAULT VALUES insert
	bool default_values = false;

	//! INSERT BY POSITION or INSERT BY NAME
	InsertColumnOrder column_order;

public:
	//! Convert the query node to a string
	string ToString() const override;

	bool Equals(const QueryNode *other) const override;

	//! Create a copy of this InsertQueryNode
	unique_ptr<QueryNode> Copy() const override;

	void Serialize(Serializer &serializer) const override;
	static unique_ptr<QueryNode> Deserialize(Deserializer &deserializer);
};

} // namespace duckdb
