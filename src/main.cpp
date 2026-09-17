#include "database.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace std;

namespace {
constexpr unsigned long kBufferSize = 1024;

struct BoolValue {
    bool value = false;
};

wstring widen(const string& text) {
    return wstring(text.begin(), text.end());
}

string narrowAscii(const wstring& text) {
    string result;
    result.reserve(text.size());
    for (wchar_t ch : text) {
        result.push_back(static_cast<char>(ch));
    }
    return result;
}

string trim(const string& text) {
    size_t first = 0;
    while (first < text.size() && isspace(static_cast<unsigned char>(text[first]))) {
        ++first;
    }

    size_t last = text.size();
    while (last > first && isspace(static_cast<unsigned char>(text[last - 1]))) {
        --last;
    }

    return text.substr(first, last - first);
}

string uppercaseAscii(string text) {
    transform(text.begin(), text.end(), text.begin(),
        [](unsigned char ch) { return static_cast<char>(toupper(ch)); });
    return text;
}

bool endsWith(const string& text, const string& suffix) {
    return text.size() >= suffix.size()
        && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

string incomeRangeSql() {
    return
        "CASE "
        "WHEN c.annual_income < 60000 THEN 'Low (<60k)' "
        "WHEN c.annual_income < 100000 THEN 'Middle (60k-99k)' "
        "WHEN c.annual_income < 140000 THEN 'Upper (100k-139k)' "
        "ELSE 'High (140k+)' END";
}

void printDivider() {
    cout << string(100, '-') << '\n';
}

void bindString(MYSQL_BIND& bind, string& value, unsigned long& length) {
    length = static_cast<unsigned long>(value.size());
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = value.empty() ? const_cast<char*>("") : value.data();
    bind.buffer_length = length;
    bind.length = &length;
}

filesystem::path findProjectFile(const string& relativePath) {
    vector<filesystem::path> candidates = {
        filesystem::current_path() / ".." / relativePath,
        filesystem::current_path() / relativePath,
        filesystem::current_path() / ".." / ".." / relativePath
    };

    for (const auto& candidate : candidates) {
        error_code ec;
        if (filesystem::exists(candidate, ec)) {
            return filesystem::canonical(candidate, ec);
        }
    }

    return {};
}

bool executeSqlStatement(MYSQL* conn, const string& statement, const string& sourceName) {
    string sql = trim(statement);
    if (sql.empty()) {
        return true;
    }

    if (mysql_query(conn, sql.c_str()) != 0) {
        cerr << "Failed to execute SQL from " << sourceName << ":\n";
        cerr << sql << "\n";
        cerr << "MySQL error: " << mysql_error(conn) << "\n";
        return false;
    }

    do {
        MYSQL_RES* result = mysql_store_result(conn);
        if (result) {
            mysql_free_result(result);
        }
    } while (mysql_next_result(conn) == 0);

    return true;
}

bool executeSqlFile(MYSQL* conn, const filesystem::path& path) {
    ifstream input(path);
    if (!input) {
        cerr << "Cannot open SQL file: " << path.string() << "\n";
        return false;
    }

    cout << "Executing SQL file: " << path.filename().string() << "\n";

    string delimiter = ";";
    string statement;
    string line;
    while (getline(input, line)) {
        string trimmedLine = trim(line);
        string upperLine = uppercaseAscii(trimmedLine);

        if (upperLine.rfind("DELIMITER ", 0) == 0) {
            delimiter = trim(trimmedLine.substr(10));
            continue;
        }

        statement += line;
        statement += '\n';

        string trimmedStatement = trim(statement);
        if (endsWith(trimmedStatement, delimiter)) {
            string sql = trimmedStatement.substr(0, trimmedStatement.size() - delimiter.size());
            if (!executeSqlStatement(conn, sql, path.filename().string())) {
                return false;
            }
            statement.clear();
        }
    }

    if (!trim(statement).empty()) {
        return executeSqlStatement(conn, statement, path.filename().string());
    }

    return true;
}

bool initializeDatabase(MySqlConnection& setupConnection, const DbConfig& config) {
    if (!setupConnection.connect(config, false)) {
        cerr << "Cannot initialize database without MySQL server connection.\n";
        return false;
    }

    filesystem::path schemaPath = findProjectFile("20231628_Project2/database/schema.sql");
    filesystem::path samplePath = findProjectFile("20231628_Project2/database/sample_data.sql");
    if (schemaPath.empty() || samplePath.empty()) {
        cerr << "Cannot find schema.sql or sample_data.sql.\n";
        return false;
    }

    return executeSqlFile(setupConnection.handle(), schemaPath)
        && executeSqlFile(setupConnection.handle(), samplePath);
}

bool dropDatabase(const DbConfig& config) {
    MySqlConnection cleanupConnection;
    if (!cleanupConnection.connect(config, false)) {
        cerr << "Database cleanup skipped because MySQL server connection failed.\n";
        return false;
    }

    string query = "DROP DATABASE IF EXISTS `" + config.database + "`";
    if (mysql_query(cleanupConnection.handle(), query.c_str()) != 0) {
        cleanupConnection.printError("Failed to drop database");
        return false;
    }

    cout << "Database '" << config.database << "' dropped.\n";
    return true;
}

bool executePreparedAndPrint(MYSQL* conn, const string& query, vector<string> params) {
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
        cerr << "Failed to allocate MySQL statement.\n";
        return false;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), static_cast<unsigned long>(query.size())) != 0) {
        cerr << "Prepare failed: " << mysql_stmt_error(stmt) << '\n';
        mysql_stmt_close(stmt);
        return false;
    }

    vector<MYSQL_BIND> paramBinds(params.size());
    vector<unsigned long> paramLengths(params.size());
    for (size_t i = 0; i < params.size(); ++i) {
        bindString(paramBinds[i], params[i], paramLengths[i]);
    }

    if (!paramBinds.empty() && mysql_stmt_bind_param(stmt, paramBinds.data()) != 0) {
        cerr << "Parameter binding failed: " << mysql_stmt_error(stmt) << '\n';
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        cerr << "Statement execution failed: " << mysql_stmt_error(stmt) << '\n';
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_RES* metadata = mysql_stmt_result_metadata(stmt);
    if (!metadata) {
        cout << "Query completed. Affected rows: " << mysql_stmt_affected_rows(stmt) << "\n";
        mysql_stmt_close(stmt);
        return true;
    }

    int columnCount = mysql_num_fields(metadata);
    MYSQL_FIELD* fields = mysql_fetch_fields(metadata);
    vector<vector<char>> buffers(columnCount, vector<char>(kBufferSize));
    vector<unsigned long> lengths(columnCount);
    vector<BoolValue> isNull(columnCount);
    vector<MYSQL_BIND> resultBinds(columnCount);

    for (int i = 0; i < columnCount; ++i) {
        resultBinds[i].buffer_type = MYSQL_TYPE_STRING;
        resultBinds[i].buffer = buffers[i].data();
        resultBinds[i].buffer_length = kBufferSize;
        resultBinds[i].length = &lengths[i];
        resultBinds[i].is_null = &isNull[i].value;
    }

    if (mysql_stmt_bind_result(stmt, resultBinds.data()) != 0) {
        cerr << "Result binding failed: " << mysql_stmt_error(stmt) << '\n';
        mysql_free_result(metadata);
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_store_result(stmt) != 0) {
        cerr << "Result buffering failed: " << mysql_stmt_error(stmt) << '\n';
        mysql_free_result(metadata);
        mysql_stmt_close(stmt);
        return false;
    }

    printDivider();
    for (int i = 0; i < columnCount; ++i) {
        cout << left << setw(22) << fields[i].name;
    }
    cout << '\n';
    printDivider();

    int rows = 0;
    while (true) {
        int status = mysql_stmt_fetch(stmt);
        if (status == MYSQL_NO_DATA) {
            break;
        }
        if (status == 1) {
            cerr << "Fetch failed: " << mysql_stmt_error(stmt) << '\n';
            break;
        }

        for (int i = 0; i < columnCount; ++i) {
            string value = isNull[i].value ? "NULL" : string(buffers[i].data(), lengths[i]);
            if (value.size() > 21) {
                value = value.substr(0, 18) + "...";
            }
            cout << left << setw(22) << value;
        }
        cout << '\n';
        ++rows;
    }

    printDivider();
    cout << rows << " row(s) returned.\n";

    mysql_free_result(metadata);
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return true;
}
}

OdbcConnection::OdbcConnection() : env_(SQL_NULL_HENV), dbc_(SQL_NULL_HDBC), connected_(false) {}

OdbcConnection::~OdbcConnection() {
    if (dbc_ != SQL_NULL_HDBC) {
        if (connected_) {
            SQLDisconnect(dbc_);
        }
        SQLFreeHandle(SQL_HANDLE_DBC, dbc_);
    }
    if (env_ != SQL_NULL_HENV) {
        SQLFreeHandle(SQL_HANDLE_ENV, env_);
    }
}

bool OdbcConnection::connect(const DbConfig& config) {
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env_))) {
        cerr << "ODBC environment allocation failed.\n";
        return false;
    }
    SQLSetEnvAttr(env_, SQL_ATTR_ODBC_VERSION, reinterpret_cast<void*>(SQL_OV_ODBC3), 0);

    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC, env_, &dbc_))) {
        cerr << "ODBC connection allocation failed.\n";
        return false;
    }

    wstring conn =
        L"DSN=" + config.odbcDsn +
        L";UID=" + widen(config.user) +
        L";PWD=" + widen(config.password) +
        L";DATABASE=" + widen(config.database) + L";";

    SQLRETURN rc = SQLDriverConnectW(
        dbc_, nullptr,
        const_cast<SQLWCHAR*>(reinterpret_cast<const SQLWCHAR*>(conn.c_str())),
        SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);

    if (!SQL_SUCCEEDED(rc)) {
        printDiagnostics(SQL_HANDLE_DBC, dbc_, "ODBC connection failed");
        return false;
    }

    connected_ = true;
    cout << "ODBC connected to database '" << config.database << "'.\n";
    return true;
}

void OdbcConnection::printDiagnostics(SQLSMALLINT handleType, SQLHANDLE handle, const string& context) const {
    cerr << context << '\n';
    SQLWCHAR state[6];
    SQLWCHAR message[SQL_MAX_MESSAGE_LENGTH];
    SQLINTEGER nativeError;
    SQLSMALLINT textLength;

    for (SQLSMALLINT i = 1; SQLGetDiagRecW(handleType, handle, i, state, &nativeError,
                                           message, SQL_MAX_MESSAGE_LENGTH, &textLength) == SQL_SUCCESS; ++i) {
        wcerr << L"  SQLSTATE=" << state << L", Native=" << nativeError
            << L", Message=" << message << L'\n';
    }
}

bool OdbcConnection::executeAndPrint(const wstring& query) {
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &stmt))) {
        cerr << "ODBC statement allocation failed.\n";
        return false;
    }

    SQLRETURN rc = SQLExecDirectW(stmt, const_cast<SQLWCHAR*>(reinterpret_cast<const SQLWCHAR*>(query.c_str())), SQL_NTS);
    if (!SQL_SUCCEEDED(rc)) {
        printDiagnostics(SQL_HANDLE_STMT, stmt, "ODBC query execution failed");
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLSMALLINT columnCount = 0;
    SQLNumResultCols(stmt, &columnCount);
    if (columnCount <= 0) {
        cout << "Query completed.\n";
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return true;
    }

    vector<string> headers(columnCount);
    for (SQLSMALLINT i = 1; i <= columnCount; ++i) {
        SQLWCHAR colName[128];
        SQLSMALLINT nameLen = 0;
        SQLDescribeColW(stmt, i, colName, 128, &nameLen, nullptr, nullptr, nullptr, nullptr);
        wstring wideName(colName, nameLen);
        headers[i - 1] = narrowAscii(wideName);
    }

    printDivider();
    for (const auto& header : headers) {
        cout << left << setw(22) << header;
    }
    cout << '\n';
    printDivider();

    int rows = 0;
    while ((rc = SQLFetch(stmt)) == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO) {
        for (SQLSMALLINT i = 1; i <= columnCount; ++i) {
            char buffer[kBufferSize];
            SQLLEN indicator = 0;
            SQLGetData(stmt, i, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);
            string value = (indicator == SQL_NULL_DATA) ? "NULL" : buffer;
            if (value.size() > 21) {
                value = value.substr(0, 18) + "...";
            }
            cout << left << setw(22) << value;
        }
        cout << '\n';
        ++rows;
    }

    printDivider();
    cout << rows << " row(s) returned.\n";
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}

MySqlConnection::MySqlConnection() : conn_(nullptr) {}

MySqlConnection::~MySqlConnection() {
    if (conn_) {
        mysql_close(conn_);
    }
    mysql_library_end();
}

bool MySqlConnection::connect(const DbConfig& config) {
    return connect(config, true);
}

bool MySqlConnection::connect(const DbConfig& config, bool selectDatabase) {
    mysql_library_init(0, nullptr, nullptr);
    conn_ = mysql_init(nullptr);
    if (!conn_) {
        cerr << "MySQL initialization failed.\n";
        return false;
    }

    const char* database = selectDatabase ? config.database.c_str() : nullptr;
    if (!mysql_real_connect(conn_, config.host.c_str(), config.user.c_str(), config.password.c_str(),
                            database, config.port, nullptr, 0)) {
        printError("MySQL C API connection failed");
        return false;
    }

    mysql_set_character_set(conn_, "utf8mb4");
    if (selectDatabase) {
        cout << "MySQL C API connected to database '" << config.database << "'.\n";
    } else {
        cout << "MySQL C API connected to server.\n";
    }
    return true;
}

MYSQL* MySqlConnection::handle() {
    return conn_;
}

void MySqlConnection::printError(const string& context) const {
    cerr << context << ": " << (conn_ ? mysql_error(conn_) : "No connection handle") << '\n';
}

void displayMenu() {
    cout << "\n========== Automobile Company Query Menu ==========\n";
    cout << "1. Sales Trends\n";
    cout << "2. Defective Part Tracking\n";
    cout << "3. Top Brands by Revenue\n";
    cout << "4. Top Brands by Unit Sales\n";
    cout << "5. Seasonal Sales Patterns\n";
    cout << "6. Dealer Inventory Efficiency\n";
    cout << "7. Supplier Coverage\n";
    cout << "0. Exit\n";
    cout << "Select menu: ";
}

int getUserChoice() {
    int choice;
    if (!(cin >> choice)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return -1;
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    return choice;
}

string readLine(const string& prompt) {
    string value;
    do {
        cout << prompt;
        getline(cin, value);
        if (value.empty()) {
            cout << "Input cannot be empty.\n";
        }
    } while (value.empty());
    return value;
}

string readDate(const string& prompt) {
    while (true) {
        string value = readLine(prompt);
        if (value.size() == 10 && value[4] == '-' && value[7] == '-') {
            return value;
        }
        cout << "Use date format YYYY-MM-DD.\n";
    }
}

void executeSalesTrendsQuery(OdbcConnection& db) {
    string query =
        "SELECT DISTINCT b.brand_name AS brand, "
        "YEAR(st.sale_date) AS sale_year, "
        "MONTH(st.sale_date) AS sale_month, "
        "WEEK(st.sale_date, 1) AS sale_week, "
        "c.gender, " + incomeRangeSql() + " AS income_range "
        "FROM Sales_Transaction st "
        "JOIN Vehicle v ON st.VIN = v.VIN "
        "JOIN Model m ON v.model_ID = m.model_ID "
        "JOIN Brand b ON m.brand_ID = b.brand_ID "
        "JOIN Customer c ON st.customer_ID = c.customer_ID "
        "WHERE st.sale_date >= DATE_SUB(CURDATE(), INTERVAL 3 YEAR) "
        "ORDER BY b.brand_name, sale_year, sale_month, sale_week, c.gender, income_range";
    db.executeAndPrint(widen(query));
}

void executeDefectivePartTrackingQuery(MySqlConnection& db) {
    string supplier = "%" + readLine("Supplier name keyword (e.g., Getrag): ") + "%";
    string partType = "%" + readLine("Part type keyword (e.g., Transmission): ") + "%";
    string startDate = readDate("Start date (YYYY-MM-DD): ");
    string endDate = readDate("End date (YYYY-MM-DD): ");

    string query =
        "SELECT DISTINCT v.VIN, "
        "c.customer_name AS customer_name, "
        "DATE_FORMAT(st.sale_date, '%Y-%m-%d') AS sale_date "
        "FROM Part_Supplies ps "
        "JOIN Supplier s ON ps.supplier_ID = s.supplier_ID "
        "JOIN Part p ON ps.part_ID = p.part_ID "
        "JOIN Vehicle_Part vp ON ps.supply_ID = vp.supply_ID AND p.part_ID = vp.part_ID "
        "JOIN Vehicle v ON vp.VIN = v.VIN "
        "JOIN Sales_Transaction st ON v.VIN = st.VIN "
        "JOIN Customer c ON st.customer_ID = c.customer_ID "
        "WHERE ps.is_defective = TRUE "
        "AND s.supplier_name LIKE ? "
        "AND p.part_type LIKE ? "
        "AND ps.supplied_date BETWEEN ? AND DATE_ADD(?, INTERVAL 1 DAY) "
        "ORDER BY v.VIN";

    executePreparedAndPrint(db.handle(), query, {supplier, partType, startDate, endDate});
}

void executeTopBrandsByRevenueQuery(OdbcConnection& db) {
    string query =
        "SELECT b.brand_name AS brand, "
        "SUM(st.sale_price) AS revenue "
        "FROM Sales_Transaction st "
        "JOIN Vehicle v ON st.VIN = v.VIN "
        "JOIN Model m ON v.model_ID = m.model_ID "
        "JOIN Brand b ON m.brand_ID = b.brand_ID "
        "WHERE YEAR(st.sale_date) = YEAR(CURDATE()) - 1 "
        "GROUP BY b.brand_name "
        "ORDER BY revenue DESC "
        "LIMIT 2";
    db.executeAndPrint(widen(query));
}

void executeTopBrandsByUnitSalesQuery(OdbcConnection& db) {
    string query =
        "WITH brand_units AS ("
        "SELECT b.brand_name AS brand, COUNT(*) AS units_sold "
        "FROM Sales_Transaction st "
        "JOIN Vehicle v ON st.VIN = v.VIN "
        "JOIN Model m ON v.model_ID = m.model_ID "
        "JOIN Brand b ON m.brand_ID = b.brand_ID "
        "WHERE YEAR(st.sale_date) = YEAR(CURDATE()) - 1 "
        "GROUP BY b.brand_name"
        "), ranked_units AS ("
        "SELECT brand, units_sold, DENSE_RANK() OVER (ORDER BY units_sold DESC) AS sales_rank "
        "FROM brand_units"
        ") "
        "SELECT brand, units_sold "
        "FROM ranked_units "
        "WHERE sales_rank <= 2 "
        "ORDER BY sales_rank, brand DESC "
        "LIMIT 2";
    db.executeAndPrint(widen(query));
}

void executeSeasonalSalesPatternsQuery(MySqlConnection& db) {
    string bodyStyle = "%" + readLine("Body style keyword (e.g., Convertible, SUV): ") + "%";
    string query =
        "WITH monthly_sales AS ("
        "SELECT MONTH(st.sale_date) AS sale_month, "
        "COUNT(*) AS units_sold "
        "FROM Sales_Transaction st "
        "JOIN Vehicle v ON st.VIN = v.VIN "
        "JOIN Model m ON v.model_ID = m.model_ID "
        "WHERE m.body_style LIKE ? "
        "GROUP BY sale_month"
        ") "
        "SELECT sale_month, units_sold "
        "FROM monthly_sales "
        "WHERE units_sold = (SELECT MAX(units_sold) FROM monthly_sales) "
        "ORDER BY sale_month";
    executePreparedAndPrint(db.handle(), query, {bodyStyle});
}

void executeDealerInventoryEfficiencyQuery(OdbcConnection& db) {
    string query =
        "SELECT d.dealer_name, "
        "ROUND(AVG(TIMESTAMPDIFF(DAY, di.inventory_start_date, "
        "COALESCE(di.inventory_end_date, CURRENT_DATE()))), 2) AS avg_inventory_days "
        "FROM Dealer_Inventory di "
        "JOIN Dealer d ON di.dealer_ID = d.dealer_ID "
        "GROUP BY d.dealer_ID, d.dealer_name "
        "ORDER BY avg_inventory_days DESC "
        "LIMIT 1";
    db.executeAndPrint(widen(query));
}

void executeSupplierCoverageQuery(OdbcConnection& db) {
    string query =
        "SELECT s.supplier_name, "
        "COUNT(DISTINCT cp.model_ID) AS distinct_model_count "
        "FROM Supplier s "
        "JOIN Part_Supplies ps ON s.supplier_ID = ps.supplier_ID "
        "JOIN Composes cp ON ps.part_ID = cp.part_ID "
        "GROUP BY s.supplier_ID, s.supplier_name "
        "ORDER BY distinct_model_count DESC "
        "LIMIT 1";
    db.executeAndPrint(widen(query));
}

int main() {
    DbConfig config;
    cout << "Automobile Company Database Application\n";
    cout << "Default ODBC DSN: project2_mysql, host: 127.0.0.1, database: project2, user: root\n";
    config.password = readLine("MySQL password for root: ");

    bool initialized = false;
    {
        MySqlConnection setupConnection;
        initialized = initializeDatabase(setupConnection, config);
    }

    if (!initialized) {
        cerr << "Database initialization failed.\n";
        dropDatabase(config);
        return 1;
    }

    int exitCode = 0;
    {
        OdbcConnection odbc;
        MySqlConnection mysql;

        if (!odbc.connect(config)) {
            cerr << "Cannot continue without ODBC connection.\n";
            exitCode = 1;
        } else if (!mysql.connect(config)) {
            cerr << "Cannot continue without MySQL C API connection.\n";
            exitCode = 1;
        } else {
            bool running = true;
            while (running) {
                displayMenu();
                int choice = getUserChoice();
                switch (choice) {
                    case 1:
                        executeSalesTrendsQuery(odbc);
                        break;
                    case 2:
                        executeDefectivePartTrackingQuery(mysql);
                        break;
                    case 3:
                        executeTopBrandsByRevenueQuery(odbc);
                        break;
                    case 4:
                        executeTopBrandsByUnitSalesQuery(odbc);
                        break;
                    case 5:
                        executeSeasonalSalesPatternsQuery(mysql);
                        break;
                    case 6:
                        executeDealerInventoryEfficiencyQuery(odbc);
                        break;
                    case 7:
                        executeSupplierCoverageQuery(odbc);
                        break;
                    case 0:
                        running = false;
                        break;
                    default:
                        cout << "Invalid menu number. Select 0 through 7.\n";
                        break;
                }
            }
        }
    }

    cout << "Exit program.\n";
    if (!dropDatabase(config)) {
        return 1;
    }

    return exitCode;
}
