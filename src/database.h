#ifndef DATABASE_H
#define DATABASE_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <sql.h>
#include <sqlext.h>

#include <mysql.h>

#include <iostream>
#include <limits>
#include <string>
#include <vector>

struct DbConfig {
    std::wstring odbcDsn = L"project2_mysql";
    std::string host = "127.0.0.1";
    unsigned int port = 3306;
    std::string user = "root";
    std::string password;
    std::string database = "project2";
};

class OdbcConnection {
public:
    OdbcConnection();
    ~OdbcConnection();

    bool connect(const DbConfig& config);
    bool executeAndPrint(const std::wstring& query);
    void printDiagnostics(SQLSMALLINT handleType, SQLHANDLE handle, const std::string& context) const;

private:
    SQLHENV env_;
    SQLHDBC dbc_;
    bool connected_;
};

class MySqlConnection {
public:
    MySqlConnection();
    ~MySqlConnection();

    bool connect(const DbConfig& config);
    bool connect(const DbConfig& config, bool selectDatabase);
    MYSQL* handle();
    void printError(const std::string& context) const;

private:
    MYSQL* conn_;
};

void displayMenu();
int getUserChoice();
std::string readLine(const std::string& prompt);
std::string readDate(const std::string& prompt);

void executeSalesTrendsQuery(OdbcConnection& db);
void executeDefectivePartTrackingQuery(MySqlConnection& db);
void executeTopBrandsByRevenueQuery(OdbcConnection& db);
void executeTopBrandsByUnitSalesQuery(OdbcConnection& db);
void executeSeasonalSalesPatternsQuery(MySqlConnection& db);
void executeDealerInventoryEfficiencyQuery(OdbcConnection& db);
void executeSupplierCoverageQuery(OdbcConnection& db);

#endif
