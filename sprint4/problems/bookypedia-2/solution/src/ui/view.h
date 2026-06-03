#pragma once
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "../app/use_cases.h"

namespace menu {
class Menu;
}

namespace app {
class UseCases;
}

namespace ui {

class View {
public:
    View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output);

private:
    bool AddAuthor(std::istream& cmd_input) const;
    bool AddBook(std::istream& cmd_input) const;
    bool DeleteAuthor(std::istream& cmd_input) const;
    bool EditAuthor(std::istream& cmd_input) const;
    bool ShowAuthors() const;
    bool ShowBooks() const;
    bool ShowBook(std::istream& cmd_input) const;
    bool ShowAuthorBooks() const;
    bool DeleteBook(std::istream& cmd_input) const;
    bool EditBook(std::istream& cmd_input) const;

    std::optional<app::AuthorInfo> SelectAuthor() const;
    std::optional<app::AuthorInfo> SelectFromAuthorList() const;
    std::optional<app::BookInfo> SelectBookFromList(const std::vector<app::BookInfo>& books) const;
    std::vector<app::AuthorInfo> GetAuthors() const;

    static std::vector<std::string> ParseTags(const std::string& line);

    menu::Menu& menu_;
    app::UseCases& use_cases_;
    std::istream& input_;
    std::ostream& output_;
};

}  // namespace ui
