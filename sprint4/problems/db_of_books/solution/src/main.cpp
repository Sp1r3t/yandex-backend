#include <iostream>
#include <optional>
#include <string>

#include <boost/json.hpp>
#include <pqxx/pqxx>

namespace json = boost::json;

static void CreateTable(pqxx::connection& conn) {
    pqxx::work tx{conn};
    tx.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id     SERIAL PRIMARY KEY,
    title  varchar(100) NOT NULL,
    author varchar(100) NOT NULL,
    year   integer NOT NULL,
    "ISBN" char(13) UNIQUE
);
)");
    tx.commit();
}

static void HandleAddBook(pqxx::connection& conn, const json::object& payload) {
    const std::string title  = json::value_to<std::string>(payload.at("title"));
    const std::string author = json::value_to<std::string>(payload.at("author"));
    const int         year   = payload.at("year").as_int64();

    std::optional<std::string> isbn;
    if (const auto& v = payload.at("ISBN"); !v.is_null()) {
        isbn = json::value_to<std::string>(v);
    }

    bool ok = false;
    try {
        pqxx::work tx{conn};
        if (isbn) {
            tx.exec_params(
                R"(INSERT INTO books (title, author, year, "ISBN") VALUES ($1, $2, $3, $4))",
                title, author, year, *isbn);
        } else {
            tx.exec_params(
                R"(INSERT INTO books (title, author, year, "ISBN") VALUES ($1, $2, $3, NULL))",
                title, author, year);
        }
        tx.commit();
        ok = true;
    } catch (const pqxx::sql_error&) {
        // duplicate ISBN or other constraint violation
    }

    json::object result;
    result["result"] = ok;
    std::cout << json::serialize(result) << "\n";
}

static void HandleAllBooks(pqxx::connection& conn) {
    pqxx::read_transaction tx{conn};
    auto rows = tx.exec(R"(
SELECT id, title, author, year, "ISBN"
FROM books
ORDER BY year DESC, title ASC, author ASC, "ISBN" ASC
)");

    json::array arr;
    for (const auto& row : rows) {
        json::object book;
        book["id"]     = row["id"].as<int>();
        book["title"]  = json::value(row["title"].c_str());
        book["author"] = json::value(row["author"].c_str());
        book["year"]   = row["year"].as<int>();
        if (row["ISBN"].is_null()) {
            book["ISBN"] = nullptr;
        } else {
            book["ISBN"] = json::value(row["ISBN"].c_str());
        }
        arr.push_back(std::move(book));
    }
    std::cout << json::serialize(arr) << "\n";
}

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: book_manager <db_url>\n";
        return EXIT_FAILURE;
    }

    try {
        pqxx::connection conn{argv[1]};
        CreateTable(conn);

        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;

            const auto val     = json::parse(line);
            const auto& obj    = val.as_object();
            const std::string action = json::value_to<std::string>(obj.at("action"));
            const auto& payload = obj.at("payload").as_object();

            if (action == "add_book") {
                HandleAddBook(conn, payload);
            } else if (action == "all_books") {
                HandleAllBooks(conn);
            } else if (action == "exit") {
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
