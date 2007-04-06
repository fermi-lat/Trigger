#include "../../TriggerTables.h"


int main(){


    TriggerTables tt;

    tt.print();
    
    int i = tt[7](6); // should match
    std::cout << "gltword  engine" << std::endl;


    for (int k=0; k<32; ++k){
        std::cout << k <<"\t" << tt(k) << std::endl;
    }

    return 0;
}