#include "duckdb/parser/query_node/insert_query_node.hpp"
#include "duckdb/parser/statement/insert_statement.hpp"
#include "duckdb/parser/statement/update_statement.hpp"
#include "duckdb/parser/expression_util.hpp"
#include "duckdb/common/serializer/serializer.hpp"

namespace duckdb {

static bool UpdateSetInfoEquals(const unique_ptr<UpdateSetInfo> &left, const unique_ptr<UpdateSetInfo> &right) {
	if (left && right) {
		if (left->columns != right->columns) {
			return false;
		}
		if (!ExpressionUtil::ListEquals(left->expressions, right->expressions)) {
			return false;
		}
		if (!ParsedExpression::Equals(left->condition, right->condition)) {
			return false;
		}
		return true;
	}
	// One is null, the other is not
	return left.get() == right.get();
}

static bool OnConflictInfoEquals(const unique_ptr<OnConflictInfo> &left, const unique_ptr<OnConflictInfo> &right) {
	if (left && right) {
		if (left->action_type != right->action_type) {
			return false;
		}
		if (left->indexed_columns != right->indexed_columns) {
			return false;
		}
		if (!UpdateSetInfoEquals(left->set_info, right->set_info)) {
			return false;
		}
		if (!ParsedExpression::Equals(left->condition, right->condition)) {
			return false;
		}
		return true;
	}
	// One is null, the other is not
	return left.get() == right.get();
}

InsertQueryNode::InsertQueryNode()
    : QueryNode(QueryNodeType::INSERT_QUERY_NODE), default_values(false),
      column_order(InsertColumnOrder::INSERT_BY_POSITION) {
}

string InsertQueryNode::ToString() const {
	string result;
	result += "INSERT INTO ";
	if (!catalog.empty() && catalog != INVALID_CATALOG) {
		result += KeywordHelper::WriteOptionallyQuoted(catalog) + ".";
	}
	if (!schema.empty() && schema != DEFAULT_SCHEMA) {
		result += KeywordHelper::WriteOptionallyQuoted(schema) + ".";
	}
	result += KeywordHelper::WriteOptionallyQuoted(table);
	if (table_ref && !table_ref->alias.empty()) {
		result += StringUtil::Format(" AS %s", KeywordHelper::WriteOptionallyQuoted(table_ref->alias));
	}
	if (column_order == InsertColumnOrder::INSERT_BY_NAME) {
		result += " BY NAME";
	}
	if (!columns.empty()) {
		result += " (";
		for (idx_t i = 0; i < columns.size(); i++) {
			if (i > 0) {
				result += ", ";
			}
			result += KeywordHelper::WriteOptionallyQuoted(columns[i]);
		}
		result += ")";
	}
	result += " ";
	if (select_statement) {
		result += select_statement->ToString();
	} else if (default_values) {
		result += "DEFAULT VALUES";
	}
	if (!returning_list.empty()) {
		result += " RETURNING ";
		for (idx_t i = 0; i < returning_list.size(); i++) {
			if (i > 0) {
				result += ", ";
			}
			result += returning_list[i]->ToString();
			if (!returning_list[i]->GetAlias().empty()) {
				result += StringUtil::Format(" AS %s",
				                             KeywordHelper::WriteOptionallyQuoted(returning_list[i]->GetAlias()));
			}
		}
	}
	return result + ResultModifiersToString();
}

bool InsertQueryNode::Equals(const QueryNode *other_p) const {
	if (!QueryNode::Equals(other_p)) {
		return false;
	}
	if (this == other_p) {
		return true;
	}
	auto &other = other_p->Cast<InsertQueryNode>();

	if (catalog != other.catalog || schema != other.schema || table != other.table) {
		return false;
	}
	if (columns != other.columns) {
		return false;
	}
	if (default_values != other.default_values) {
		return false;
	}
	if (column_order != other.column_order) {
		return false;
	}
	if (!ExpressionUtil::ListEquals(returning_list, other.returning_list)) {
		return false;
	}
	if (!TableRef::Equals(table_ref, other.table_ref)) {
		return false;
	}
	// Compare select_statement
	if (select_statement && other.select_statement) {
		if (!select_statement->Equals(*other.select_statement)) {
			return false;
		}
	} else if (select_statement || other.select_statement) {
		return false;
	}
	// Compare on_conflict_info
	if (!OnConflictInfoEquals(on_conflict_info, other.on_conflict_info)) {
		return false;
	}
	return true;
}

unique_ptr<QueryNode> InsertQueryNode::Copy() const {
	auto result = make_uniq<InsertQueryNode>();
	result->catalog = catalog;
	result->schema = schema;
	result->table = table;
	result->columns = columns;
	result->default_values = default_values;
	result->column_order = column_order;
	if (select_statement) {
		result->select_statement = unique_ptr_cast<SQLStatement, SelectStatement>(select_statement->Copy());
	}
	for (auto &expr : returning_list) {
		result->returning_list.push_back(expr->Copy());
	}
	if (on_conflict_info) {
		result->on_conflict_info = on_conflict_info->Copy();
	}
	if (table_ref) {
		result->table_ref = table_ref->Copy();
	}
	this->CopyProperties(*result);
	return std::move(result);
}

void InsertQueryNode::Serialize(Serializer &serializer) const {
	// For now, disallow serialization of DML CTEs (views cannot contain them)
	throw NotImplementedException("INSERT in CTE cannot be serialized - views cannot contain data-modifying CTEs");
}

unique_ptr<QueryNode> InsertQueryNode::Deserialize(Deserializer &deserializer) {
	throw NotImplementedException("INSERT in CTE cannot be deserialized");
}

} // namespace duckdb
