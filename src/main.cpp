#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
  if (argc > 1 && std::string(argv[1]) == "--hello") {
    std::cout << "hello from stub" << std::endl;
  }
  std::cout << "milo-experimentd (bootstrap)\n";
  return 0;
}

