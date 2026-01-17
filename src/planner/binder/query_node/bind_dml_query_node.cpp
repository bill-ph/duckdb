#include "duckdb/parser/query_node/insert_query_node.hpp"
#include "duckdb/parser/statement/insert_statement.hpp"
#include "duckdb/planner/binder.hpp"

namespace duckdb {

BoundStatement Binder::BindNode(InsertQueryNode &node) {
	// Convert InsertQueryNode back to InsertStatement for binding
	InsertStatement insert;
	insert.catalog = node.catalog;
	insert.schema = node.schema;
	insert.table = node.table;
	insert.columns = node.columns;
	insert.default_values = node.default_values;
	insert.column_order = node.column_order;
	if (node.select_statement) {
		insert.select_statement = unique_ptr_cast<SQLStatement, SelectStatement>(node.select_statement->Copy());
	}
	for (auto &expr : node.returning_list) {
		insert.returning_list.push_back(expr->Copy());
	}
	if (node.on_conflict_info) {
		insert.on_conflict_info = node.on_conflict_info->Copy();
	}
	if (node.table_ref) {
		insert.table_ref = node.table_ref->Copy();
	}
	// Note: We don't copy node.cte_map because the CTEs are handled at the outer level
	return Bind(insert);
}

} // namespace duckdb
