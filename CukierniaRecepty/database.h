#pragma once
#include <vector>
#include <Wt/Dbo/Session>
#include <Wt/Dbo/backend/MySQL>
#include <Wt/Dbo/backend/Sqlite3>

std::vector<std::string> GetWords(const std::string& input);

class Database : public Wt::Dbo::Session {
   public:
    Database() {
        connection = std::make_unique<Wt::Dbo::backend::MySQL>("cukiernia", "root", "root", "localhost", 3306);
        setConnection(*connection);
    }

    void ensureTablesExisting() {
        try {
            this->createTables();
            printf("\n\n\nCreated tables\n\n\n");
        } catch (...) {
            printf("\n\n\nTables are already exsting, or something bad happened\n\n\n"); // TODO: make it not vague
        }
    }

   private:
    std::unique_ptr<Wt::Dbo::SqlConnection> connection;
};

extern Database db;
