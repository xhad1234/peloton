//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// sortbench_loader.cpp
//
// Identification: src/main/sortbench/sortbench_loader.cpp
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <iostream>
#include <ctime>

#include "benchmark/sortbench/sortbench_loader.h"
#include "benchmark/sortbench/sortbench_config.h"
#include "catalog/catalog.h"
#include "catalog/schema.h"
#include "concurrency/transaction.h"
#include "concurrency/transaction_manager_factory.h"
#include "executor/abstract_executor.h"
#include "executor/insert_executor.h"
#include "executor/executor_context.h"
#include "executor/executor_tests_util.h"
#include "expression/constant_value_expression.h"
#include "expression/expression_util.h"
#include "index/index_factory.h"
#include "planner/insert_plan.h"
#include "storage/tile.h"
#include "storage/tile_group.h"
#include "storage/data_table.h"
#include "storage/table_factory.h"
#include "storage/database.h"
#include "executor/executor_tests_util.h"

#include "parser/statement_insert.h"

#define LINEITEM_TABLE_SIZE 6000000
#define PART_TABLE_SIZE 200000

namespace peloton {
namespace benchmark {
namespace sortbench {

storage::Database *sortbench_database = nullptr;

storage::DataTable *left_table = nullptr;
storage::DataTable *right_table = nullptr;

std::string left_table_name = "LEFT_TABLE";
std::string right_table_name = "RIGHT_TABLE";

void CreateSortBenchDatabase() {
  /////////////////////////////////////////////////////////
  // Create database & tables
  /////////////////////////////////////////////////////////
  // Clean up
  delete sortbench_database;
  sortbench_database = nullptr;
  left_table = nullptr;
  right_table = nullptr;

  auto catalog = catalog::Catalog::GetInstance();

  auto &txn_manager = concurrency::TransactionManagerFactory::GetInstance();
  auto txn = txn_manager.BeginTransaction();

  auto l_id_col = catalog::Column(
      common::Type::INTEGER, common::Type::GetTypeSize(common::Type::INTEGER),
      "l_id", true);
  auto l_sortkey_col = catalog::Column(
      common::Type::INTEGER, common::Type::GetTypeSize(common::Type::INTEGER),
      "l_sortkey", true);
  auto l_shipdate_col = catalog::Column(
      common::Type::INTEGER, common::Type::GetTypeSize(common::Type::INTEGER),
      "l_shipdate", true);

  std::unique_ptr<catalog::Schema> left_table_schema(
      new catalog::Schema({l_id_col, l_sortkey_col, l_shipdate_col}));

  auto r_id_col = catalog::Column(
      common::Type::INTEGER, common::Type::GetTypeSize(common::Type::INTEGER),
      "r_id", true);
  auto r_sortkey_col = catalog::Column(
      common::Type::INTEGER, common::Type::GetTypeSize(common::Type::INTEGER),
      "r_sortkey", true);
  auto r_shipdate_col = catalog::Column(
      common::Type::INTEGER, common::Type::GetTypeSize(common::Type::INTEGER),
      "r_shipdate", true);

  std::unique_ptr<catalog::Schema> right_table_schema(
      new catalog::Schema({r_id_col, r_sortkey_col, r_shipdate_col}));


  catalog->CreateDatabase(SORTBENCH_DB_NAME, txn);
  txn_manager.CommitTransaction(txn);

  // create left table
  txn = txn_manager.BeginTransaction();

  catalog->CreateTable(SORTBENCH_DB_NAME, left_table_name,
                       std::move(left_table_schema), txn,
                       1);
  txn_manager.CommitTransaction(txn);
  // create right table
  txn = txn_manager.BeginTransaction();
  catalog->CreateTable(SORTBENCH_DB_NAME, right_table_name,
                       std::move(right_table_schema), txn,
                       1);
  txn_manager.CommitTransaction(txn);
}

void LoadHelper(parser::InsertStatement* insert_stmt, int insert_size){
  auto &txn_manager = concurrency::TransactionManagerFactory::GetInstance();
  planner::InsertPlan node(insert_stmt);

  // start the tasks
  auto txn = txn_manager.BeginTransaction();
  auto context = new executor::ExecutorContext(txn);

  executor::AbstractExecutor* executor = new executor::InsertExecutor(context);
  executor->Execute();

  txn_manager.CommitTransaction(txns[partition]);

  for (auto val_list : *insert_stmt->insert_values){
    for (auto val : *val_list){
      delete val;
    }
    delete val_list;
  }

  insert_stmt->insert_values->clear();
}

void LoadSortBenchDatabase() {
  left_table = catalog::Catalog::GetInstance()->GetTableWithName(
      SORTBENCH_DB_NAME, left_table_name);

  right_table = catalog::Catalog::GetInstance()->GetTableWithName(
      SORTBENCH_DB_NAME, right_table_name);

  size_t num_partition = PL_NUM_PARTITIONS();

  char *left_table_name_arr = new char[left_table_name.size()+1];
  std::copy(left_table_name.begin(), left_table_name.end(), left_table_name_arr);
  char *left_db_name_arr = new char[strlen(SORTBENCH_DB_NAME)+1];
  strcpy(left_db_name_arr, SORTBENCH_DB_NAME);

  char *right_table_name_arr = new char[right_table_name.size()+1];
  std::copy(right_table_name.begin(), right_table_name.end(), right_table_name_arr);
  char *right_db_name_arr = new char[strlen(SORTBENCH_DB_NAME)+1];
  strcpy(right_db_name_arr, SORTBENCH_DB_NAME);

  auto *left_table = new parser::TableInfo();
  left_table->table_name = left_table_name_arr;
  left_table->database_name = left_db_name_arr;

  auto *right_table = new parser::TableInfo();
  right_table->table_name = right_table_name_arr;
  right_table->database_name = right_db_name_arr;

  char *l_col_1 = new char[5];
  strcpy(l_col_1, "l_id");
  char *l_col_2 = new char[10];
  strcpy(l_col_2, "l_sortkey");
  char *l_col_3 = new char[11];
  strcpy(l_col_3, "l_shipdate");

  // insert to left table; build an insert statement
  std::unique_ptr<parser::InsertStatement> insert_stmt(
      new parser::InsertStatement(INSERT_TYPE_VALUES));

  insert_stmt->table_info_ = left_table;
  insert_stmt->columns = new std::vector<char *>;
  insert_stmt->columns->push_back(const_cast<char *>(l_col_1));
  insert_stmt->columns->push_back(const_cast<char *>(l_col_2));
  insert_stmt->columns->push_back(const_cast<char *>(l_col_3));
  insert_stmt->select = new parser::SelectStatement();
  insert_stmt->insert_values =
      new std::vector<std::vector<expression::AbstractExpression *> *>;


  for (int tuple_id = 0; tuple_id < LEFT_TABLE_SIZE * state.scale_factor; tuple_id++) {
    auto values_ptr = new std::vector<expression::AbstractExpression *>;
    insert_stmt->insert_values->push_back(values_ptr);
    int shipdate = rand() % 60;
    int sortkey = rand() % (1 << SIMD_SORT_KEY_BITS);

    values_ptr->push_back(new expression::ConstantValueExpression(
        common::ValueFactory::GetIntegerValue(tuple_id)));
    values_ptr->push_back(new expression::ConstantValueExpression(
        common::ValueFactory::GetIntegerValue(sortkey)));
    values_ptr->push_back(new expression::ConstantValueExpression(
        common::ValueFactory::GetIntegerValue(shipdate)));

    if ((tuple_id + 1) % INSERT_SIZE == 0) {
      LoadHelper(insert_stmt.get(), insert_size);
      LOG_INFO("finished writing tuple in part table: %d", tuple_id+1);
    }
  }

  char *r_col_1 = new char[5];
  strcpy(r_col_1, "r_id");
  char *r_col_2 = new char[10];
  strcpy(r_col_2, "r_sortkey");
  char *r_col_3 = new char[11];
  strcpy(r_col_3, "r_shipdate");

  // insert to left table; build an insert statement
  std::unique_ptr<parser::InsertStatement> insert_stmt(
      new parser::InsertStatement(INSERT_TYPE_VALUES));
  insert_stmt->table_info_ = right_table_name;
  insert_stmt->columns = new std::vector<char *>;
  insert_stmt->columns->push_back(const_cast<char *>(r_col_1));
  insert_stmt->columns->push_back(const_cast<char *>(r_col_2));
  insert_stmt->columns->push_back(const_cast<char *>(r_col_3));
  insert_stmt->select = new parser::SelectStatement();
  insert_stmt->insert_values =
      new std::vector<std::vector<expression::AbstractExpression *> *>;

  for (int tuple_id = 0; tuple_id < RIGHT_TABLE_SIZE * state.scale_factor; tuple_id++) {
    auto values_ptr = new std::vector<expression::AbstractExpression *>;
    insert_stmt->insert_values->push_back(values_ptr);
    int shipdate = rand() % 60;
    int sortkey = rand() % (1 << SIMD_SORT_KEY_BITS);

    values_ptr->push_back(new expression::ConstantValueExpression(
        common::ValueFactory::GetIntegerValue(tuple_id)));
    values_ptr->push_back(new expression::ConstantValueExpression(
        common::ValueFactory::GetIntegerValue(sortkey)));
    values_ptr->push_back(new expression::ConstantValueExpression(
        common::ValueFactory::GetIntegerValue(shipdate)));

    if ((tuple_id + 1) % INSERT_SIZE == 0) {
      LoadHelper(insert_stmt.get(), insert_size);
      LOG_INFO("finished writing tuple in part table: %d", tuple_id+1);
    }
  }
}

}  // namespace sortbench
}  // namespace benchmark
}  // namespace peloton