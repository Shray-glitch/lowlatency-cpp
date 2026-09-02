#include "logger.hpp"

#include <string>


int main()
{
    Logger logger("logger_demo.log");


    char c = 'A';
    int id = 42;
    unsigned long quantity = 500;

    float ratio = 3.4f;
    double price = 150.25;

    const char* symbol = "AAPL";

    std::string message =
        "order accepted";


    logger.log(
        "char:% id:% quantity:%\n",
        c,
        id,
        quantity
    );


    logger.log(
        "ratio:% price:%\n",
        ratio,
        price
    );


    logger.log(
        "symbol:%\n",
        symbol
    );


    logger.log(
        "message:%\n",
        message
    );


    logger.log(
        "percent example:100%%\n"
    );


    return 0;
}