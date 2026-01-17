#include "catch.hpp"
#include "test_helpers.hpp"
#include "duckdb/parser/parser.hpp"
#include "duckdb/parser/statement/select_statement.hpp"
#include "duckdb/parser/query_node/select_node.hpp"
#include "duckdb/parser/query_node/insert_query_node.hpp"
#include "duckdb/parser/statement/insert_statement.hpp"
#include "duckdb/parser/parsed_expression_iterator.hpp"

using namespace duckdb;
using namespace std;

TEST_CASE("Test InsertQueryNode::Equals compares on_conflict_info", "[api]") {
	DuckDB db(nullptr);
	Connection con(db);

	// Create a table with a primary key for ON CONFLICT to work
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE test(id INT PRIMARY KEY, val INT)"));

	// Parse INSERT with ON CONFLICT DO UPDATE - will be transformed into InsertQueryNode
	auto sql_with_conflict = "WITH cte AS (INSERT INTO test VALUES (1, 10) ON CONFLICT (id) DO UPDATE SET val = 20 RETURNING *) SELECT * FROM cte";
	auto stmts_with = con.ExtractStatements(sql_with_conflict);
	REQUIRE(stmts_with.size() == 1);

	auto &select_stmt_with = stmts_with[0]->Cast<SelectStatement>();
	// The top-level node is a SELECT node, CTEs are stored in the cte_map
	auto &cte_map_with = select_stmt_with.node->cte_map;
	REQUIRE(cte_map_with.map.size() == 1);
	auto &cte_info_with = cte_map_with.map.begin()->second;
	REQUIRE(cte_info_with->query->node->type == QueryNodeType::INSERT_QUERY_NODE);
	auto &insert_node_with = cte_info_with->query->node->Cast<InsertQueryNode>();
	REQUIRE(insert_node_with.on_conflict_info != nullptr);

	// Parse INSERT without ON CONFLICT
	auto sql_without_conflict = "WITH cte AS (INSERT INTO test VALUES (2, 20) RETURNING *) SELECT * FROM cte";
	auto stmts_without = con.ExtractStatements(sql_without_conflict);
	REQUIRE(stmts_without.size() == 1);

	auto &select_stmt_without = stmts_without[0]->Cast<SelectStatement>();
	auto &cte_map_without = select_stmt_without.node->cte_map;
	REQUIRE(cte_map_without.map.size() == 1);
	auto &cte_info_without = cte_map_without.map.begin()->second;
	REQUIRE(cte_info_without->query->node->type == QueryNodeType::INSERT_QUERY_NODE);
	auto &insert_node_without = cte_info_without->query->node->Cast<InsertQueryNode>();
	REQUIRE(insert_node_without.on_conflict_info == nullptr);

	// These two InsertQueryNode instances differ in on_conflict_info
	// Equals() should return false
	REQUIRE(!insert_node_with.Equals(&insert_node_without));
}

TEST_CASE("Test InsertQueryNode::Equals compares different on_conflict_info actions", "[api]") {
	DuckDB db(nullptr);
	Connection con(db);

	REQUIRE_NO_FAIL(con.Query("CREATE TABLE test(id INT PRIMARY KEY, val INT)"));

	// Parse INSERT with ON CONFLICT DO UPDATE
	auto sql_do_update = "WITH cte AS (INSERT INTO test VALUES (1, 10) ON CONFLICT (id) DO UPDATE SET val = 20 RETURNING *) SELECT * FROM cte";
	auto stmts_update = con.ExtractStatements(sql_do_update);
	auto &select_stmt_update = stmts_update[0]->Cast<SelectStatement>();
	auto &cte_info_update = select_stmt_update.node->cte_map.map.begin()->second;
	auto &insert_node_update = cte_info_update->query->node->Cast<InsertQueryNode>();

	// Parse INSERT with ON CONFLICT DO NOTHING
	auto sql_do_nothing = "WITH cte AS (INSERT INTO test VALUES (1, 10) ON CONFLICT (id) DO NOTHING RETURNING *) SELECT * FROM cte";
	auto stmts_nothing = con.ExtractStatements(sql_do_nothing);
	auto &select_stmt_nothing = stmts_nothing[0]->Cast<SelectStatement>();
	auto &cte_info_nothing = select_stmt_nothing.node->cte_map.map.begin()->second;
	auto &insert_node_nothing = cte_info_nothing->query->node->Cast<InsertQueryNode>();

	// Both have on_conflict_info, but with different actions
	REQUIRE(insert_node_update.on_conflict_info != nullptr);
	REQUIRE(insert_node_nothing.on_conflict_info != nullptr);

	// They should not be equal
	REQUIRE(!insert_node_update.Equals(&insert_node_nothing));
}

TEST_CASE("Test expression iterator traverses on_conflict_info expressions", "[api]") {
	DuckDB db(nullptr);
	Connection con(db);

	REQUIRE_NO_FAIL(con.Query("CREATE TABLE test(id INT PRIMARY KEY, val INT)"));

	// Parse INSERT with ON CONFLICT DO UPDATE SET that contains expressions
	// The expression "val + 100" should be traversed
	auto sql = "WITH cte AS (INSERT INTO test VALUES (1, 10) ON CONFLICT (id) DO UPDATE SET val = val + 100 RETURNING *) SELECT * FROM cte";
	auto stmts = con.ExtractStatements(sql);
	auto &select_stmt = stmts[0]->Cast<SelectStatement>();
	auto &cte_info = select_stmt.node->cte_map.map.begin()->second;
	auto &insert_node = cte_info->query->node->Cast<InsertQueryNode>();

	REQUIRE(insert_node.on_conflict_info != nullptr);
	REQUIRE(insert_node.on_conflict_info->set_info != nullptr);
	REQUIRE(!insert_node.on_conflict_info->set_info->expressions.empty());

	// Count all expressions traversed by the iterator
	idx_t expression_count = 0;
	bool found_addition_expr = false;

	ParsedExpressionIterator::EnumerateQueryNodeChildren(
	    *cte_info->query->node,
	    [&](duckdb::unique_ptr<ParsedExpression> &expr) {
	        expression_count++;
	        // Check if we found the addition expression from ON CONFLICT SET
	        if (expr->ToString().find("+") != string::npos) {
	            found_addition_expr = true;
	        }
	    },
	    [](TableRef &) {});

	// The expression iterator should have found the "val + 100" expression
	// This test will fail until the bug is fixed (on_conflict_info expressions are not traversed)
	REQUIRE(found_addition_expr);
}

TEST_CASE("Test expression iterator traverses on_conflict_info condition", "[api]") {
	DuckDB db(nullptr);
	Connection con(db);

	REQUIRE_NO_FAIL(con.Query("CREATE TABLE test(id INT PRIMARY KEY, val INT)"));

	// Parse INSERT with ON CONFLICT DO UPDATE with a WHERE condition
	auto sql = "WITH cte AS (INSERT INTO test VALUES (1, 10) ON CONFLICT (id) DO UPDATE SET val = 20 WHERE val < 100 RETURNING *) SELECT * FROM cte";
	auto stmts = con.ExtractStatements(sql);
	auto &select_stmt = stmts[0]->Cast<SelectStatement>();
	auto &cte_info = select_stmt.node->cte_map.map.begin()->second;
	auto &insert_node = cte_info->query->node->Cast<InsertQueryNode>();

	REQUIRE(insert_node.on_conflict_info != nullptr);
	REQUIRE(insert_node.on_conflict_info->set_info != nullptr);
	REQUIRE(insert_node.on_conflict_info->set_info->condition != nullptr);

	// Count expressions and check for the WHERE condition
	bool found_comparison_expr = false;

	ParsedExpressionIterator::EnumerateQueryNodeChildren(
	    *cte_info->query->node,
	    [&](duckdb::unique_ptr<ParsedExpression> &expr) {
	        // Check if we found the comparison expression from WHERE clause
	        if (expr->ToString().find("<") != string::npos) {
	            found_comparison_expr = true;
	        }
	    },
	    [](TableRef &) {});

	// The expression iterator should have found the "val < 100" expression
	// This test will fail until the bug is fixed
	REQUIRE(found_comparison_expr);
}

TEST_CASE("Test InsertQueryNode::Copy preserves on_conflict_info", "[api]") {
	DuckDB db(nullptr);
	Connection con(db);

	REQUIRE_NO_FAIL(con.Query("CREATE TABLE test(id INT PRIMARY KEY, val INT)"));

	auto sql = "WITH cte AS (INSERT INTO test VALUES (1, 10) ON CONFLICT (id) DO UPDATE SET val = 20 RETURNING *) SELECT * FROM cte";
	auto stmts = con.ExtractStatements(sql);
	auto &select_stmt = stmts[0]->Cast<SelectStatement>();
	auto &cte_info = select_stmt.node->cte_map.map.begin()->second;
	auto &insert_node = cte_info->query->node->Cast<InsertQueryNode>();

	REQUIRE(insert_node.on_conflict_info != nullptr);

	// Copy the node
	auto copied = insert_node.Copy();
	auto &copied_insert = copied->Cast<InsertQueryNode>();

	// Verify the copy has on_conflict_info
	REQUIRE(copied_insert.on_conflict_info != nullptr);

	// Verify they are equal (after fixing Equals, this should pass)
	REQUIRE(insert_node.Equals(copied.get()));
}
