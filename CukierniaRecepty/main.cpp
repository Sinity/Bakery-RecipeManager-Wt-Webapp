#include <functional>
#include <memory>
#include <Wt/WServer>
#include <Wt/WApplication>
#include <Wt/WBootstrapTheme>
#include <Wt/WStackedWidget>
#include <Wt/WMenu>
#include <Wt/Auth/AuthModel>
#include <Wt/Auth/AuthWidget>
#include <Wt/Auth/PasswordService>
#include "database.h"
#include "User.h"
#include "IngredientsWidget.h"
#include "RecipesWidget.h"
#include "UnitsWidget.h"

class App : public Wt::WApplication {
  public:
    App(const Wt::WEnvironment& env) : WApplication(env) {
        setTitle(L"Cukiernia - System Przepisów");
        setTheme(new Wt::WBootstrapTheme(this));

        setupLayout();
        initDatabase();
        setupAuth();

        menu->itemSelected().connect(std::bind([=]() {
            if(!db.login.loggedIn()) {
                return;
            }

            if (menu->currentIndex() == 0) {
                recipes->populateRecipeList();
            } else if (menu->currentIndex() == 1) {
                ingredients->populateIngredientList();
            } else if (menu->currentIndex() == 2) {
                units->populateUnitsList();
            }
        }));

        internalPathChanged().connect(std::bind([this] {
            Wt::log("notice") << "New internal path: " << internalPath().c_str();
            if (!db.login.loggedIn()) {
                setInternalPath("/login", false);
                content->setCurrentWidget(authWidget.get());
                menu->select(0);
            } else if (internalPath() == "/recipe") {
                content->setCurrentWidget(recipeDetails.get());
                menu->select(-1);
            }
        }));


        db.login.changed().emit();
        internalPathChanged().emit(internalPath());
    }

  private:
    void initDatabase() {
        try {
            db.mapClass<Ingredient>("ingredient");
            db.mapClass<Unit>("unit");
            db.mapClass<Recipe>("recipe");
            db.mapClass<IngredientRecord>("ingredient_record");
            db.mapClass<User>("user");
            db.mapClass<AuthInfo>("auth_info");
            db.mapClass<AuthInfo::AuthIdentityType>("auth_identity");
            db.mapClass<AuthInfo::AuthTokenType>("auth_token");
            db.ensureTablesExisting();

            db.users = std::make_unique<UserDatabase>(db);
        } catch (std::exception e) {
            printf("\n\n\n\n\n\n\n\nUNCAUGHT EXCEPTION WHILE INITIALIZING DB: %s\n\n\n\n\n", e.what());
        }
    }

    void setupLayout() {
        container = std::make_unique<Wt::WContainerWidget>(root());
        content = std::make_unique<Wt::WStackedWidget>();

        menu = std::make_unique<Wt::WMenu>(content.get());
        menu->setStyleClass("nav nav-pills nav-horizontal");
        menu->setInternalPathEnabled();
        menu->setInternalBasePath("/");

        container->addWidget(menu.get());
        container->addWidget(content.get());
    }

    void setupAuth() {
        authWidget = std::make_unique<Wt::Auth::AuthWidget>(Database::auth(), *(db.users), db.login);
        authWidget->model()->addPasswordAuth(&Database::passwordAuth());
        authWidget->setRegistrationEnabled(true);
        authWidget->processEnvironment();
        menu->addItem("Login", authWidget.get());

        db.login.changed().connect(std::bind([this] {
            for(auto* menuItem : menu->items()) {
                menu->removeItem(menuItem);
            }

            if(db.login.loggedIn() && db.users->find(db.login.user())->user().id() != -1) {
                Wt::Dbo::Transaction t{db};

                Wt::log("notice") << db.users->find(db.login.user())->user().id() << " logged in!";

                recipeDetails = std::make_unique<RecipeDetailsWidget>(content.get(), db);
                recipes = std::make_unique<RecipesWidget>(content.get(), *recipeDetails, db);
                ingredients = std::make_unique<IngredientsWidget>(content.get(), db);
                units = std::make_unique<UnitsWidget>(content.get(), db);

                menu->addItem("Przepisy", recipes.get());
                menu->addItem(L"Składniki", ingredients.get());
                menu->addItem("Jednostki", units.get());
                menu->addItem("Login", authWidget.get());
                content->addWidget(recipeDetails.get());
            } else {
                Wt::log("notice") << "User is not logged in!";

                //UI widgets are created/destoryed in bulk, so checking one shall suffice
                if(recipeDetails != nullptr) {
                    content->removeWidget(recipeDetails.get());
                    content->removeWidget(recipes.get());
                    content->removeWidget(ingredients.get());
                    content->removeWidget(units.get());
                }

                menu->addItem("Login", authWidget.get());

                recipeDetails = nullptr;
                recipes = nullptr;
                ingredients = nullptr;
                units = nullptr;

                setInternalPath("/login", true);
            }
        }));
    }

    std::unique_ptr<Wt::WContainerWidget> container;
    std::unique_ptr<Wt::WStackedWidget> content;
    std::unique_ptr<Wt::WMenu> menu;

    std::unique_ptr<RecipesWidget> recipes;
    std::unique_ptr<RecipeDetailsWidget> recipeDetails;
    std::unique_ptr<IngredientsWidget> ingredients;
    std::unique_ptr<UnitsWidget> units;
    std::unique_ptr<Wt::Auth::AuthWidget> authWidget;

    Database db;
};

Wt::WApplication* createApp(const Wt::WEnvironment& env) {
    auto* app = new App(env);
    app->messageResourceBundle().use("auth_strings");
    return app;
}

int main(int argc, char** argv) {
    try {
        Wt::WServer wSrv(argv[0]);
        wSrv.setServerConfiguration(argc, argv, WTHTTP_CONFIGURATION);
        wSrv.addEntryPoint(Wt::Application, createApp);

        Database::configureAuth();

        if(wSrv.start()) {
            Wt::WServer::waitForShutdown();
            wSrv.stop();
        }
    } catch (Wt::WServer::Exception& e) {
        std::cerr << "WServer exception: " << e.what() << std::endl;
    } catch (Wt::Dbo::Exception& e) {
        std::cerr << "Wt::Dbo exception: " << e.what() << std::endl;
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}
