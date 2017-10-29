#pragma once
#include <vector>
#include <Wt/Dbo/Session>
#include <Wt/Dbo/ptr>
#include <Wt/Auth/Login>
#include <Wt/Auth/Dbo/UserDatabase>
#include <Wt/Auth/AuthService>
#include <Wt/Auth/HashFunction>
#include <Wt/Auth/PasswordService>
#include <Wt/Auth/PasswordVerifier>
#include <Wt/Dbo/backend/MySQL>
#include "User.h"

using UserDatabase = Wt::Auth::Dbo::UserDatabase<AuthInfo>;

namespace {
Wt::Auth::AuthService authService;
Wt::Auth::PasswordService passService(authService);
}

class Database : public Wt::Dbo::Session {
   public:
    Database() {
        connection = std::make_unique<Wt::Dbo::backend::MySQL>("cukiernia", "root", "root", "localhost", 3306);
        setConnection(*connection);
    }

    void ensureTablesExisting() {
        try {
            printf("\n\n\nTable Creation code: \n\n%s\n\n\n", this->tableCreationSql().c_str());
            this->createTables();
            printf("\n\n\nCreated tables\n\n\n");
        } catch (...) {
            printf("\n\n\nTables are already exsting, or something bad happened\n\n\n"); // TODO: make it not vague
        }
    }

    static void configureAuth() {
        authService.setAuthTokensEnabled(true, "logincookie");
        Wt::Auth::PasswordVerifier* verifier = new Wt::Auth::PasswordVerifier();
        verifier->addHashFunction(new Wt::Auth::BCryptHashFunction(7));
        passService.setVerifier(verifier);
        passService.setAttemptThrottlingEnabled(true);
    }

    static const Wt::Auth::AuthService& auth() {
        return authService;
    }

    static const Wt::Auth::PasswordService& passwordAuth() {
        return passService;
    }

    std::unique_ptr<UserDatabase> users;
    Wt::Auth::Login login;

   private:
    std::unique_ptr<Wt::Dbo::SqlConnection> connection;
};

