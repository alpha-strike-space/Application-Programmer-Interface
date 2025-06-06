#include <gtest/gtest.h>
#include <crow.h>
#include <nlohmann/json.hpp>

class HealthTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code that will be called before each test
    }

    void TearDown() override {
        // Cleanup code that will be called after each test
    }
};

TEST_F(HealthTest, HealthEndpointReturns200) {
    crow::SimpleApp app;
    
    // Define the health endpoint
    CROW_ROUTE(app, "/health").methods("GET"_method)([]() {
        crow::json::wvalue response;
        response["Health"] = "I'm alive!";
        return crow::response(200, response);
    });

    // Create a test request
    crow::request req;
    req.method = crow::HTTPMethod::Get;
    req.url = "/health";

    // Create a response object
    crow::response res;
    
    // Process the request
    app.handle(req, res);

    // Assert the response
    EXPECT_EQ(res.code, 200);
    
    // Parse the response body
    auto body = nlohmann::json::parse(res.body);
    EXPECT_EQ(body["Health"], "I'm alive!");
}

TEST_F(HealthTest, HealthEndpointReturnsCorrectContentType) {
    crow::SimpleApp app;
    
    // Define the health endpoint
    CROW_ROUTE(app, "/health").methods("GET"_method)([]() {
        crow::json::wvalue response;
        response["Health"] = "I'm alive!";
        return crow::response(200, response);
    });

    // Create a test request
    crow::request req;
    req.method = crow::HTTPMethod::Get;
    req.url = "/health";

    // Create a response object
    crow::response res;
    
    // Process the request
    app.handle(req, res);

    // Assert the content type
    EXPECT_EQ(res.get_header_value("Content-Type"), "application/json");
} 