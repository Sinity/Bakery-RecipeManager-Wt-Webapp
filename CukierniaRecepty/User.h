#pragma once
#include <Wt/Dbo/Dbo>
#include <Wt/WGlobal>
#include <Wt/Auth/Dbo/AuthInfo>

class User;
using AuthInfo = Wt::Auth::Dbo::AuthInfo<User>;

class User {
  public:
    int firmID;
    int accessLevel;

    template<class Action>
    void persist(Action& a) {
        Wt::Dbo::field(a, firmID, "firm_id");
        Wt::Dbo::field(a, accessLevel, "access_level");
    }
};
