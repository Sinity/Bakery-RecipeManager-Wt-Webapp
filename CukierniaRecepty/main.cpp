#include <functional>
#include <memory>
#include <Wt/WServer>
#include <Wt/WApplication>
#include <Wt/WBootstrapTheme>
#include <Wt/WStackedWidget>
#include <Wt/WMenu>
#include "database.h"
#include "IngredientsWidget.h"
#include "RecipesWidget.h"
#include "UnitsWidget.h"

class App : public Wt::WApplication {
  public:
    App(const Wt::WEnvironment& env) : WApplication(env) {
        setTitle(L"Cukiernia - System Przepisów");
        setupLayout();
        initDatabase();

        recipeDetails = std::make_unique<RecipeDetailsWidget>(content.get());
        recipes = std::make_unique<RecipesWidget>(content.get(), *recipeDetails);
        ingredients = std::make_unique<IngredientsWidget>(content.get());
        units = std::make_unique<UnitsWidget>(content.get());

        menu->addItem("Przepisy", recipes.get());
        menu->addItem(L"Składniki", ingredients.get());
        menu->addItem("Jednostki", units.get());
        content->addWidget(recipeDetails.get());

        menu->itemSelected().connect(std::bind([=]() {
            if (menu->currentIndex() == 0) {
                recipes->populateRecipeList();
            } else if (menu->currentIndex() == 1) {
                ingredients->populateIngredientList();
            } else if (menu->currentIndex() == 2) {
                units->populateUnitsList();
            }
        }));

        internalPathChanged().connect(std::bind([this] {
            printf("\n\n\n\n\n\n\nNew internal path: %s\n\n", internalPath().c_str());
            if (internalPath() == "/recipe") {
                content->setCurrentIndex(3);
                menu->select(-1);
            }
        }));
    }

  private:
    void setupLayout() {
        setTheme(new Wt::WBootstrapTheme(this));

        container = std::make_unique<Wt::WContainerWidget>(root());
        content = std::make_unique<Wt::WStackedWidget>();

        menu = std::make_unique<Wt::WMenu>(content.get());
        menu->setStyleClass("nav nav-pills nav-horizontal");
        menu->setInternalPathEnabled();
        menu->setInternalBasePath("/");

        container->addWidget(menu.get());
        container->addWidget(content.get());
    }

    void initDatabase() {
        try {
            db.mapClass<Ingredient>("ingredient");
            db.mapClass<Unit>("unit");
            db.mapClass<Recipe>("recipe");
            db.mapClass<IngredientRecord>("ingredient_record");
            db.ensureTablesExisting();
        } catch (std::exception e) {
            printf("\n\n\n\n\n\n\n\nUNCAUGHT EXCEPTION WHILE INITIALIZING DB: %s\n\n\n\n\n", e.what());
        } catch (...) {
            printf("\n\n\n\n\n\n\n\nUNCAUGHT EXCEPTION WHILE INITIALIZING DB\n\n\n");
        }
    }

    std::unique_ptr<Wt::WContainerWidget> container;
    std::unique_ptr<Wt::WStackedWidget> content;
    std::unique_ptr<Wt::WMenu> menu;

    std::unique_ptr<RecipesWidget> recipes;
    std::unique_ptr<RecipeDetailsWidget> recipeDetails;
    std::unique_ptr<IngredientsWidget> ingredients;
    std::unique_ptr<UnitsWidget> units;
};

int main(int argc, char** argv) {
    try {
        return Wt::WRun(argc, argv, [](const Wt::WEnvironment& env) { return new App(env); });
    } catch (Wt::WServer::Exception& e) {
        std::cerr << "WServer exception: " << e.what() << std::endl;
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}
