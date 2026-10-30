#include <iostream>
void runTowerSystemTests();
void runCannonSystemTests();
void runTrapSystemTests();
int main() {
    runTowerSystemTests(); runCannonSystemTests(); runTrapSystemTests();
    std::cout << "Defense tests passed\n";
}
