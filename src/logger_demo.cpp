#include "logger.hpp"

#include <string>


int main()
{
    // The logger starts its background thread here.
    Logger logger("logger_demo.log");


    // Values of different supported types.
    char c = 'A';
    int id = 42;
    unsigned long quantity = 500;

    float ratio = 3.4f;
    double price = 150.25;

    const char* symbol = "AAPL";

    std::string message =
        "order accepted";


    // Each single % is replaced by the next value.
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


    // Two percent signs produce one real percent character.
    logger.log(
        "percent example:100%%\n"
    );


    // The destructor waits for queued values before closing the file.
    return 0;
}
