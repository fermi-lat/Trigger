#include "../../TriggerTables.h"

#include <iomanip>

int main(){

    using namespace Trigger;
    TriggerTables tt;

    tt.print();
    
    int i = tt[7].match(6); // should match
    std::cout << "gltword  marker" << std::endl;


    for (int k=0; k<32; ++k){
        std::cout 
            << std::right << std::setw(5) << k
            << std::right << std::setw(5) << tt(k) << std::endl;
    }

    return 0;
}