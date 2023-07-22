#include "Parser.hpp"

// Use tutorials in: https://llvm.org/docs/tutorial/

int main(int argc, char * argv[]) {
    Parser parser;
    if (!parser.Parse()) {
        return 1;
    }
    try {
        parser.Generate().print(llvm::outs(), nullptr);
    } catch (exception & e) {
        cout << "Error during parsing:" << endl;
        cout << e.what() << endl;
        return 1;
    }


    return 0;
}
